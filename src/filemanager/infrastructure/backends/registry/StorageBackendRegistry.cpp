#include "filemanager/infrastructure/backends/registry/StorageBackendRegistry.h"
#include "filemanager/infrastructure/support/FileManagerPathUtils.h"

#include <cstring>
#include <string_view>

using namespace FILEMGR;
using SYSTEM::makePsramString;

namespace {
using PathView = std::string_view;

constexpr const char* kSdcardPrefix = "/sdcard";
constexpr const char* kLittlefsPrefix = "/littlefs";

PathView makeView(const char* text)
{
    return text ? PathView(text) : PathView();
}

PathView makeView(const SYSTEM::PsramString& text)
{
    return PathView(text.data(), text.size());
}

bool startsWithPathPrefix(PathView path, const char* prefix)
{
    const size_t prefixLength = strlen(prefix);
    if (path.size() < prefixLength || memcmp(path.data(), prefix, prefixLength) != 0)
    {
        return false;
    }

    return path.size() == prefixLength || path[prefixLength] == '/';
}
}  // namespace

StorageBackendRegistry::StorageBackendRegistry(FS* littleFs)
    : _littleFs(littleFs),
      _activeBackend(makePsramString(BACKEND_LITTLEFS)),
      _activePath()
{
}

void StorageBackendRegistry::begin()
{
}

FS* StorageBackendRegistry::resolveFilesystem(const SYSTEM::PsramString& path)
{
    SYSTEM::PsramString canonicalPath;
    if (!FileManagerPathUtils::canonicalizeAbsolutePath(path, SYSTEM::makePsramString("/"), canonicalPath))
    {
        return nullptr;
    }

    if (isSdPath(canonicalPath)) {
        return nullptr;
    }

    return _littleFs;
}

void StorageBackendRegistry::onSensorPathUpdated(const SYSTEM::PsramString& fsPath)
{
    _activePath = fsPath;
    _activeBackend = makePsramString(BACKEND_LITTLEFS);
}

bool StorageBackendRegistry::isSdPath(const SYSTEM::PsramString& path) const
{
    return startsWithPathPrefix(makeView(path), kSdcardPrefix);
}

SYSTEM::PsramString StorageBackendRegistry::toFilesystemPath(const SYSTEM::PsramString& path) const
{
    SYSTEM::PsramString canonicalPath;
    if (!FileManagerPathUtils::canonicalizeAbsolutePath(path, SYSTEM::makePsramString("/"), canonicalPath))
    {
        return SYSTEM::PsramString();
    }

    if (isSdPath(canonicalPath))
    {
        return SYSTEM::PsramString();
    }

    SYSTEM::PsramString nativePath = canonicalPath;
    if (startsWithPathPrefix(makeView(nativePath), kLittlefsPrefix))
    {
        const size_t prefixLength = strlen(kLittlefsPrefix);
        if (nativePath.size() == prefixLength)
        {
            nativePath.assign("/");
        }
        else
        {
            nativePath.erase(0, prefixLength);
        }
    }

    SYSTEM::PsramString canonicalNativePath;
    if (!FileManagerPathUtils::canonicalizeAbsolutePath(
            nativePath, SYSTEM::makePsramString("/"), canonicalNativePath))
    {
        return SYSTEM::PsramString();
    }

    return canonicalNativePath;
}
