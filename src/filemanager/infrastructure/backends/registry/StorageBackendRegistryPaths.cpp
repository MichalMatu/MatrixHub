#include "filemanager/infrastructure/backends/registry/StorageBackendRegistry.h"

#include <string_view>

using namespace FILEMGR;

bool StorageBackendRegistry::ensureDirectoryTree(FS* fs, const SYSTEM::PsramString& directory) const
{
    if (!fs)
    {
        return true;
    }

    const SYSTEM::PsramString nativePath = toFilesystemPath(directory);
    if (nativePath.empty())
    {
        return false;
    }

    if (nativePath == "/")
    {
        return true;
    }

    SYSTEM::PsramString accum;
    accum.reserve(nativePath.size());

    size_t start = 0;
    if (!nativePath.empty() && nativePath.front() == '/')
    {
        accum.push_back('/');
        start = 1U;
    }

    std::string_view nativeView(nativePath.data(), nativePath.size());
    size_t pos = start;
    while (pos <= nativeView.size())
    {
        size_t next = nativeView.find('/', pos);
        if (next == std::string_view::npos)
        {
            next = nativeView.size();
        }

        const std::string_view part = nativeView.substr(pos, next - pos);
        if (!part.empty())
        {
            if (accum.size() > 1 && accum.back() != '/')
            {
                accum.push_back('/');
            }
            accum.append(part.data(), part.size());

            if (!fs->exists(accum.c_str()))
            {
                if (!fs->mkdir(accum.c_str()))
                {
                    return false;
                }
            }
        }

        pos = next + 1U;
    }

    return true;
}
