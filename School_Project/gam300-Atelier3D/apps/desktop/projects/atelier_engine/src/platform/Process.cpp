/* !
@file               Process.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements RunProcess with CreateProcessW and a pipe on Windows, and
    posix_spawn and a pipe on Linux.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/platform/Process.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

#include <algorithm>
#include <array>

namespace Engine::Platform
{
#ifdef _WIN32
    namespace
    {
        // One command line argument quoted the way CommandLineToArgvW (and the C runtime) reads it back
        std::wstring Quote(const std::wstring& argument)
        {
            if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
            {
                return argument;
            }
            std::wstring quoted = L"\"";
            std::size_t backslashes = 0;
            for (const wchar_t c : argument)
            {
                if (c == L'\\')
                {
                    ++backslashes;
                    continue;
                }
                if (c == L'"')
                {
                    quoted.append(backslashes * 2 + 1,
                                  L'\\'); // the backslashes before a quote double up, the quote is escaped
                }
                else
                {
                    quoted.append(backslashes, L'\\');
                }
                backslashes = 0;
                quoted += c;
            }
            quoted.append(backslashes * 2, L'\\'); // the closing quote must not be escaped
            quoted += L'"';
            return quoted;
        }

        // Moves whatever the program has printed so far into text
        void Drain(HANDLE pipe, std::string& text)
        {
            std::array<char, 4096> buffer{};
            DWORD available = 0;
            while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) && available > 0)
            {
                DWORD read = 0;
                if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(std::min<std::size_t>(buffer.size(), available)),
                              &read, nullptr) ||
                    read == 0)
                {
                    break;
                }
                text.append(buffer.data(), read);
            }
        }
    }

    ProcessResult RunProcess(const std::filesystem::path& program, const std::vector<std::filesystem::path>& arguments,
                             std::chrono::seconds timeout)
    {
        ProcessResult result;

        SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE readEnd = nullptr;
        HANDLE writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &inheritable, 0))
        {
            return result;
        }
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0); // only the program's end is passed on
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        std::wstring commandLine = Quote(program.wstring());
        for (const std::filesystem::path& argument : arguments)
        {
            commandLine += L' ';
            commandLine += Quote(argument.wstring());
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = writeEnd;
        startup.hStdError = writeEnd;
        PROCESS_INFORMATION process{};
        const BOOL created = CreateProcessW(program.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
                                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
        CloseHandle(writeEnd); // so the pipe ends when the program does
        if (input != INVALID_HANDLE_VALUE)
        {
            CloseHandle(input);
        }
        if (!created)
        {
            CloseHandle(readEnd);
            return result;
        }
        result.started = true;

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;)
        {
            Drain(readEnd, result.output);
            if (WaitForSingleObject(process.hProcess, 20) == WAIT_OBJECT_0)
            {
                break;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, 5000);
                result.timedOut = true;
                break;
            }
        }
        Drain(readEnd, result.output);

        DWORD code = 0;
        if (!result.timedOut && GetExitCodeProcess(process.hProcess, &code))
        {
            result.exitCode = static_cast<int>(code);
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(readEnd);
        return result;
    }
#else
    ProcessResult RunProcess(const std::filesystem::path& program, const std::vector<std::filesystem::path>& arguments,
                             std::chrono::seconds timeout)
    {
        ProcessResult result;

        int pipes[2];
        if (pipe(pipes) != 0)
        {
            return result;
        }
        fcntl(pipes[0], F_SETFD, FD_CLOEXEC); // only the program gets the write end

        // The program prints into the pipe (output and errors together) and reads an empty input
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipes[1]);

        const std::string programText = program.string();
        std::vector<std::string> argumentTexts;
        argumentTexts.reserve(arguments.size());
        for (const std::filesystem::path& argument : arguments)
        {
            argumentTexts.push_back(argument.string());
        }
        std::vector<char*> argv{const_cast<char*>(programText.c_str())};
        for (std::string& text : argumentTexts)
        {
            argv.push_back(text.data());
        }
        argv.push_back(nullptr);

        pid_t pid = 0;
        const int spawned = posix_spawn(&pid, programText.c_str(), &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        close(pipes[1]);
        if (spawned != 0)
        {
            close(pipes[0]);
            return result;
        }
        result.started = true;

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::array<char, 4096> buffer{};
        for (;;)
        {
            pollfd watched{pipes[0], POLLIN, 0};
            const int ready = poll(&watched, 1, 100);
            if (ready > 0)
            {
                const ssize_t count = read(pipes[0], buffer.data(), buffer.size());
                if (count <= 0)
                {
                    break; // the program closed its output: it has finished
                }
                result.output.append(buffer.data(), static_cast<std::size_t>(count));
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                kill(pid, SIGKILL);
                result.timedOut = true;
                break;
            }
        }
        close(pipes[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        if (!result.timedOut && WIFEXITED(status))
        {
            result.exitCode = WEXITSTATUS(status);
        }
        return result;
    }
#endif
}
