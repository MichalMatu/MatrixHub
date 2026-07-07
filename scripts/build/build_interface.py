#   MatrixHub Interface Embedder
#
#   Builds the MatrixHub SvelteKit frontend and embeds the static output for
#   firmware delivery from PROGMEM.
#
#   Copyright (C) 2018 - 2023 rjwats
#   Copyright (C) 2023 - 2024 theelims
#   Copyright (C) 2023 Maxtrium B.V. [ code available under dual license ]
#   Copyright (C) 2024 runeharlyk
#   Copyright (C) 2025 hmbacher
#
#   All Rights Reserved. This software may be modified and distributed under
#   the terms of the LGPL v3 license. See the LICENSE file for details.

from io import StringIO
from pathlib import Path
from shutil import copytree, rmtree
from os.path import exists, getmtime
import os
import sys
import gzip
import mimetypes
from datetime import datetime

# Import shared prebuild utilities
try:
    from prebuild_utils import is_build_task
except ImportError:
    # If unexpected execution context, try appending script dir
    sys.path.append(os.path.dirname(os.path.realpath(__file__)))
    from prebuild_utils import is_build_task

Import("env")


def option_enabled(option_name):
    try:
        value = env.GetProjectOption(option_name, "0")
    except TypeError:
        try:
            value = env.GetProjectOption(option_name)
        except Exception:
            value = "0"
    except Exception:
        value = "0"

    return str(value).strip().lower() in ("1", "true", "yes", "on")


# Build-time optimization summary:
# - skip the frontend pipeline unless real UI inputs changed,
# - skip dependency installation unless lockfiles/setup changed,
# - keep gzip output deterministic and avoid rewriting WWWData.h when bytes match,
#   so incremental firmware builds can reuse the C++ compilation cache.
# - do not force a rebuild just because `upload` was requested; flashing should
#   reuse the last generated UI unless the actual frontend inputs changed.

# Check if this script should run
# SKIP_UI=1 env var allows skipping the heavy Svelte build for faster backend-only iteration
if os.environ.get('SKIP_UI') == '1' or option_enabled("custom_skip_ui"):
    print("Skipping interface build (SKIP_UI/custom_skip_ui).")
elif not is_build_task(['build', 'upload', 'buildfs', 'erase_upload']):
    # Skip script execution for all other tasks
    print("Skipping interface build for non-build task.")
else:
    project_dir = env["PROJECT_DIR"]
    buildFlags = env.ParseFlags(env["BUILD_FLAGS"])
    script_file = os.path.join(project_dir, "scripts/build/build_interface.py")

    interface_dir = project_dir + "/interface"
    # Must match the include used by the firmware: `#include <core/WWWData.h>`
    output_file = project_dir + "/lib/framework/core/WWWData.h"
    build_dir = interface_dir + "/build"
    filesystem_dir = project_dir + "/data/www"
    dependency_stamp_file = interface_dir + "/node_modules/.platformio-deps.stamp"
    # Track the actual frontend inputs so PlatformIO can skip the expensive UI
    # pipeline when only firmware code changed.
    build_input_paths = [
        interface_dir + "/src",
        interface_dir + "/static",
        interface_dir + "/messages",
        interface_dir + "/project.inlang",
        interface_dir + "/src/app.html",
        interface_dir + "/package.json",
        interface_dir + "/package-lock.json",
        interface_dir + "/pnpm-lock.yaml",
        interface_dir + "/yarn.lock",
        interface_dir + "/svelte.config.js",
        interface_dir + "/vite.config.ts",
        interface_dir + "/vite-plugin-littlefs.ts",
    ]
    # Changes to package manager config or install-time patches can alter the
    # build output even when app source files stayed the same, so treat them as
    # rebuild triggers too.
    rebuild_input_paths = build_input_paths + [
        interface_dir + "/.npmrc",
        interface_dir + "/scripts/patch-svelte-focus-trap.js",
    ]
    # Dependency installation is slower than the Vite build on incremental runs,
    # so keep a separate watch list for lockfiles and npm setup scripts.
    dependency_input_paths = [
        interface_dir + "/package.json",
        interface_dir + "/package-lock.json",
        interface_dir + "/pnpm-lock.yaml",
        interface_dir + "/yarn.lock",
        interface_dir + "/.npmrc",
        interface_dir + "/scripts/patch-svelte-focus-trap.js",
    ]


    def iter_input_files(paths):
        # Walk only the directories/files that can affect the generated web UI.
        for path_str in paths:
            path = Path(path_str)
            if not path.exists():
                continue
            if path.is_dir():
                for child in sorted(path.rglob("*")):
                    if child.is_file():
                        yield str(child), getmtime(child)
            elif path.is_file():
                yield str(path), getmtime(path)


    def find_latest_timestamp(paths):
        # A single newest mtime is enough for our "did anything relevant change?"
        # check and is much cheaper than hashing the whole frontend tree.
        latest_timestamp = None
        latest_path = None
        for file_path, file_mtime in iter_input_files(paths):
            if latest_timestamp is None or file_mtime > latest_timestamp:
                latest_timestamp = file_mtime
                latest_path = file_path
        return latest_timestamp, latest_path


    def describe_timestamp(timestamp, path):
        if timestamp is None or path is None:
            return "missing"
        return f"{datetime.fromtimestamp(timestamp)} ({path})"


    def get_output_target():
        if flag_exists("EMBED_WWW"):
            return output_file
        return filesystem_dir


    def find_output_timestamp(path_str):
        path = Path(path_str)
        if not path.exists():
            return None, None
        if path.is_file():
            return getmtime(path), str(path)
        return find_latest_timestamp([path_str])


    def should_rebuild_webapp():
        output_target = get_output_target()
        output_timestamp, output_path = find_output_timestamp(output_target)
        if output_timestamp is None:
            print(f"Frontend output target is missing - rebuild required ({output_target})")
            return True

        # Compare the generated header against real frontend inputs instead of
        # rebuilding on every upload/build invocation. In filesystem mode we
        # compare against the generated /data/www tree instead.
        last_source_change, last_source_path = find_latest_timestamp(rebuild_input_paths)
        last_script_change = getmtime(os.path.realpath(script_file))
        latest_input_change = max(
            ts for ts in [last_source_change, last_script_change] if ts is not None
        )

        print(
            f"Newest frontend input: {describe_timestamp(last_source_change, last_source_path)}, "
            f"generator: {describe_timestamp(last_script_change, script_file)}, "
            f"output target: {describe_timestamp(output_timestamp, output_path)}"
        )

        return output_timestamp < latest_input_change


    def should_install_dependencies():
        node_modules_dir = interface_dir + "/node_modules"
        if not exists(node_modules_dir):
            print("Frontend dependencies are missing - install required")
            return True

        dependency_change, dependency_path = find_latest_timestamp(dependency_input_paths)
        if dependency_change is None:
            return False

        if not exists(dependency_stamp_file):
            print("Frontend dependency stamp is missing - install required")
            return True

        # Only reinstall packages when dependency inputs are newer than the last
        # successful install marker inside node_modules.
        last_install = getmtime(dependency_stamp_file)
        print(
            f"Newest dependency input: {datetime.fromtimestamp(dependency_change)} "
            f"({dependency_path}), "
            f"last install: {datetime.fromtimestamp(last_install)}"
        )
        return last_install < dependency_change


    def gzip_file(file):
        with open(file, 'rb') as f_in:
            # Stable gzip output prevents false positives when comparing generated
            # assets across identical builds.
            compressed = gzip.compress(f_in.read(), compresslevel=9, mtime=0)
        with open(file + '.gz', 'wb') as f_out:
            f_out.write(compressed)
        os.remove(file)


    def write_text_if_changed(path, contents):
        if exists(path):
            with open(path, "r") as existing_file:
                if existing_file.read() == contents:
                    # Keeping the old timestamp avoids recompiling every C++
                    # file that includes ESP32SvelteKit.h -> WWWData.h.
                    print(f"{path} is unchanged - keeping existing timestamp")
                    return False

        with open(path, "w") as output_handle:
            output_handle.write(contents)
        print(f"Updated generated file: {path}")
        return True


    def flag_exists(flag):
        # PlatformIO exposes CPPDEFINES as a mix of bare symbols and
        # (name, value) tuples, so support both without noisy debug logging.
        defines = buildFlags.get("CPPDEFINES")
        
        for define in defines:
            if define == flag:
                return True
            if isinstance(define, (list, tuple)) and define[0] == flag:
                return True
                
        return False


    def get_package_manager():
        if exists(os.path.join(interface_dir, "pnpm-lock.yaml")):
            return "pnpm"
        if exists(os.path.join(interface_dir, "yarn.lock")):
            return "yarn"
        else:
            return "npm"


    def build_webapp():
        package_manager = get_package_manager()
        print(f"Building interface with {package_manager}")
        previous_dir = os.getcwd()
        os.chdir(interface_dir)
        try:
            # The heavy install/build path is reached only after the mtime-based
            # checks above say the UI or its dependencies really changed.
            if should_install_dependencies():
                if package_manager == "npm":
                    install_command = f"{package_manager} ci --no-audit --no-fund"
                else:
                    install_command = f"{package_manager} install"

                ret_install = env.Execute(install_command)
                if ret_install != 0:
                    print(f"Frontend install failed with code {ret_install}!")
                    env.Exit(1)

                # Touch a dedicated stamp file instead of relying on the whole
                # node_modules tree, which would be noisy and expensive to scan.
                Path(dependency_stamp_file).touch()
            else:
                print("Skipping frontend dependency install - lockfiles unchanged")
                
            # `npm run build` now performs a single explicit Paraglide compile
            # before Vite starts, replacing the older "compile once in npm and
            # again inside Vite for SSR/client" behavior.
            ret_build = env.Execute(f"{package_manager} run build")
            if ret_build != 0:
                print(f"Frontend build failed with code {ret_build}!")
                env.Exit(1)
        finally:
            os.chdir(previous_dir)


    def embed_webapp():
        if flag_exists("EMBED_WWW"):
            print("Converting interface to PROGMEM")
            build_progmem()
            return
        add_app_to_filesystem()


    def build_progmem():
        mimetypes.init()
        progmem = StringIO()
        progmem.write("#include <functional>\n")
        progmem.write("#include <Arduino.h>\n")

        assetMap = {}

        # Manual MIME map to fix macOS/environment issues where registry is incomplete
        MIME_TYPES = {
            ".html": "text/html",
            ".css": "text/css",
            ".js": "application/javascript",
            ".json": "application/json",
            ".png": "image/png",
            ".jpg": "image/jpeg",
            ".jpeg": "image/jpeg",
            ".ico": "image/x-icon",
            ".svg": "image/svg+xml",
            ".xml": "text/xml",
            ".pdf": "application/pdf",
            ".zip": "application/zip",
            ".gz": "application/gzip",
            ".txt": "text/plain",
        }

        def cpp_string_literal(value):
            return (
                '"'
                + value.replace("\\", "\\\\")
                .replace('"', '\\"')
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                + '"'
            )

        build_root = Path(build_dir)
        candidates = {}
        precompressed_companions = {}

        for path in sorted(build_root.rglob("*.gz")):
            asset_path = path.relative_to(build_dir).as_posix()
            raw_asset_path = asset_path[:-3]
            if (build_root / raw_asset_path).exists():
                precompressed_companions[raw_asset_path] = asset_path

        for path in sorted(build_root.rglob("*.*")):
            asset_path = path.relative_to(build_dir).as_posix()
            if asset_path in precompressed_companions:
                print(f"Skipping raw asset in favour of precompressed companion: {asset_path}")
                continue

            logical_asset_path = asset_path
            mime_path = asset_path
            prefer_precompressed = False

            # Keep only one logical asset per route. When a precompressed
            # companion exists, serve its gzip payload from the original
            # raw URI (for example bundle.js -> bytes from bundle.js.gz).
            if asset_path.endswith(".gz"):
                raw_asset_path = asset_path[:-3]
                if (build_root / raw_asset_path).exists():
                    logical_asset_path = raw_asset_path
                    mime_path = raw_asset_path
                    prefer_precompressed = True

            # Try manual map first, then system defaults
            ext = os.path.splitext(mime_path)[1].lower()
            asset_mime = MIME_TYPES.get(ext)
            
            if not asset_mime:
                asset_mime = (
                    mimetypes.guess_type(mime_path)[0] or "application/octet-stream"
                )

            file_data = (
                path.read_bytes()
                if prefer_precompressed
                # Use deterministic gzip for raw assets so identical frontend
                # output produces identical WWWData.h content.
                else gzip.compress(path.read_bytes(), compresslevel=9, mtime=0)
            )

            existing = candidates.get(logical_asset_path)
            if existing and existing["prefer_precompressed"] and not prefer_precompressed:
                continue
            if existing and not existing["prefer_precompressed"] and prefer_precompressed:
                print(f"Replacing raw asset with precompressed companion: {logical_asset_path}")

            print(f"Converting {asset_path} -> {logical_asset_path}")
            candidates[logical_asset_path] = {
                "mime": asset_mime,
                "size": len(file_data),
                "data": file_data,
                "prefer_precompressed": prefer_precompressed,
            }

        for idx, asset_path in enumerate(candidates.keys()):
            asset = candidates[asset_path]
            asset_var = f"ESP_SVELTEKIT_DATA_{idx}"
            progmem.write(f"// {asset_path}\n")
            progmem.write(f"const uint8_t {asset_var}[] = {{\n\t")

            for i, byte in enumerate(asset["data"]):
                if i and not (i % 16):
                    progmem.write("\n\t")
                progmem.write(f"0x{byte:02X},")

            progmem.write("\n};\n\n")
            assetMap[asset_path] = {
                "name": asset_var,
                "mime": asset["mime"],
                "size": asset["size"],
            }

        progmem.write(
            "typedef std::function<void(const String& uri, const String& contentType, const uint8_t * content, size_t len)> RouteRegistrationHandler;\n\n"
        )
        progmem.write("struct WWWAsset {\n")
        progmem.write("\tconst char* uri;\n")
        progmem.write("\tconst char* contentType;\n")
        progmem.write("\tconst uint8_t* content;\n")
        progmem.write("\tsize_t len;\n")
        progmem.write("};\n\n")
        progmem.write(f"constexpr size_t ESP_SVELTEKIT_ASSET_COUNT = {len(assetMap)};\n")
        progmem.write("const WWWAsset ESP_SVELTEKIT_ASSETS[] = {\n")

        for asset_path, asset in assetMap.items():
            progmem.write(
                f'\t{{{cpp_string_literal("/" + asset_path)}, {cpp_string_literal(asset["mime"])}, {asset["name"]}, {asset["size"]}}},\n'
            )

        progmem.write("};\n\n")
        progmem.write("class WWWData {\n")
        progmem.write("\tpublic:\n")
        progmem.write(
            "\t\tstatic void registerRoutes(RouteRegistrationHandler handler) {\n"
        )

        progmem.write("\t\t\tfor (size_t i = 0; i < ESP_SVELTEKIT_ASSET_COUNT; ++i) {\n")
        progmem.write("\t\t\t\tconst WWWAsset& asset = ESP_SVELTEKIT_ASSETS[i];\n")
        progmem.write("\t\t\t\tconst String uri(asset.uri);\n")
        progmem.write("\t\t\t\tconst String contentType(asset.contentType);\n")
        progmem.write("\t\t\t\thandler(uri, contentType, asset.content, asset.len);\n")
        progmem.write("\t\t\t}\n")

        progmem.write("\t\t}\n")
        progmem.write("\n")
        progmem.write("\t\tstatic const WWWAsset* findAsset(const String& uri) {\n")
        progmem.write("\t\t\tfor (size_t i = 0; i < ESP_SVELTEKIT_ASSET_COUNT; ++i) {\n")
        progmem.write("\t\t\t\tconst WWWAsset& asset = ESP_SVELTEKIT_ASSETS[i];\n")
        progmem.write("\t\t\t\tif (uri.equals(asset.uri)) {\n")
        progmem.write("\t\t\t\t\treturn &asset;\n")
        progmem.write("\t\t\t\t}\n")
        progmem.write("\t\t\t}\n")
        progmem.write("\t\t\treturn nullptr;\n")
        progmem.write("\t\t}\n")
        progmem.write("\n")
        progmem.write("\t\tstatic const WWWAsset* indexAsset() {\n")
        progmem.write('\t\t\treturn findAsset(String("/index.html"));\n')
        progmem.write("\t\t}\n")
        progmem.write("};\n\n")
        write_text_if_changed(output_file, progmem.getvalue())


    def add_app_to_filesystem():
        build_path = Path(build_dir)
        www_path = Path(filesystem_dir)
        if www_path.exists() and www_path.is_dir():
            rmtree(www_path)
        print("Copying and compress interface to data directory")
        copytree(build_path, www_path)
        for current_path, _, files in os.walk(www_path):
            for file in files:
                gzip_file(os.path.join(current_path, file))
        if ("upload" in BUILD_TARGETS):
            print("Build LittleFS file system image and upload to ESP32")
            env.Execute("pio run --target uploadfs")


    print("running: build_interface.py")
    if should_rebuild_webapp():
        build_webapp()
        embed_webapp()
