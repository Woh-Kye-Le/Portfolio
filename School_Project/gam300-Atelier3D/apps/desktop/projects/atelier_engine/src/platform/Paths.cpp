/* !
@file    	    Paths.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		26/09/2026
@brief
    Implements ExecutablePath and ExecutableDir with GetModuleFileNameW on
    Windows and /proc/self/exe on Linux.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/platform/Paths.hpp"

#include "engine/core/FileSystem.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <string>
#include <system_error>
#include <vector>

#ifndef GAM300_ROOT_DIR
#define GAM300_ROOT_DIR ""
#endif

namespace Engine::Platform
{
    // Full path of the running .exe. We need this because games, templates and shaders are looked up
    // next to the exe, and the working directory depends on how the program was started.
    std::filesystem::path ExecutablePath()
    {
#ifdef _WIN32
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                break;
            }
            if (length < buffer.size())
            {
                buffer.resize(length);
                return std::filesystem::path(buffer);
            }
            // Path longer than the buffer: grow and retry
            buffer.resize(buffer.size() * 2);
        }
#else
        // on Linux the kernel gives us a link to our own executable
        std::error_code ec;
        auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec)
        {
            return exe;
        }
#endif
        return {};
    }

    std::filesystem::path ExecutableDir()
    {
        const auto exe = ExecutablePath();
        return exe.empty() ? std::filesystem::current_path() : exe.parent_path(); // working dir as a last resort
    }

    std::vector<std::filesystem::path> BuiltinShaderDirs()
    {
        std::vector<std::filesystem::path> folders;

        // The source tree first, so running out of the build folder picks up edits immediately
        const std::filesystem::path sourceRoot = GAM300_ROOT_DIR;
        if (!sourceRoot.empty())
        {
            folders.push_back(sourceRoot / "projects" / "atelier_engine" / "src" / "assets" / "Shaders" / "Builtin");
        }

        // Then the copies deployed next to the executable
        const std::filesystem::path exeDir = ExecutableDir();
        if (!exeDir.empty())
        {
            folders.push_back(exeDir / "assets" / "shaders" / "Builtin");
            folders.push_back(exeDir / "game" / "assets" / "shaders" / "Builtin");
        }

        return folders;
    }

    // Looks the built-in shader up in the given folders, in order, and returns the first match
    std::string BuiltinShaderPath(const std::string& name, const std::vector<std::filesystem::path>& folders)
    {
        for (const std::filesystem::path& folder : folders)
        {
            const std::filesystem::path candidate = folder / name;
            if (Core::FileExists(candidate))
            {
                return candidate.string();
            }
        }
        return {};
    }
}
