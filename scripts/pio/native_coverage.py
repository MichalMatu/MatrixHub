Import("env")

import pathlib
import shutil
import subprocess
import sys

clang = shutil.which("clang")
clangxx = shutil.which("clang++")

if not clang or not clangxx:
    gcc = shutil.which("gcc")
    gxx = shutil.which("g++")
    if not gcc or not gxx:
        raise FileNotFoundError(
            "Coverage build requires clang/clang++ or gcc/g++ in PATH"
        )

    print("clang/clang++ not found; using gcc/g++ coverage fallback")
    env.Replace(CC=gcc, CXX=gxx, LINK=gxx)
    env.Append(
        CCFLAGS=["--coverage"],
        CXXFLAGS=["--coverage"],
        LINKFLAGS=["--coverage"],
    )
elif sys.platform == "darwin":
    env.Replace(CC=clang, CXX=clangxx, LINK=clangxx)
    env.Append(
        CCFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"],
        CXXFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"],
        LINKFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"],
    )
    resource_dir = subprocess.check_output(
        ["xcrun", "clang", "--print-resource-dir"], text=True
    ).strip()
    profile_runtime = (
        pathlib.Path(resource_dir) / "lib" / "darwin" / "libclang_rt.profile_osx.a"
    )

    if not profile_runtime.exists():
        raise FileNotFoundError(
            f"LLVM profile runtime not found: {profile_runtime}"
        )

    env.Append(LINKFLAGS=[str(profile_runtime)])
else:
    env.Replace(CC=clang, CXX=clangxx, LINK=clangxx)
    env.Append(
        CCFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"],
        CXXFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"],
        LINKFLAGS=["-fprofile-instr-generate", "-fcoverage-mapping"],
    )
