/* !
@file    	    OpenFile.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    Implements OpenWithDefaultApp.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/OpenFile.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <thread>
#endif

#include <system_error>

namespace Editor
{
    bool OpenWithDefaultApp(const std::filesystem::path& path, std::string& error)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            error = path.filename().string() + " is not there";
            return false;
        }
#ifdef _WIN32
        // A result above 32 is success (ShellExecute's own convention)
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            nullptr, L"open", std::filesystem::absolute(path, ec).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32)
        {
            error = "no program is set to open " + path.extension().string() + " files";
            return false;
        }
        return true;
#else
        // Started directly, not through a shell, so a name like "Sam's Game" reaches it as it is. Its output goes to
        // /dev/null: it must not inherit the captured stdout/stderr pipes
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

        std::string file = std::filesystem::absolute(path, ec).string();
        char program[] = "xdg-open";
        char* argv[] = {program, file.data(), nullptr};
        pid_t pid = 0;
        const int spawned = posix_spawnp(&pid, program, &actions, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
        if (spawned != 0)
        {
            error = "xdg-open is not installed";
            return false;
        }
        // Reap the child so it does not linger as a zombie
        std::thread([pid] { waitpid(pid, nullptr, 0); }).detach();
        return true;
#endif
    }
}
