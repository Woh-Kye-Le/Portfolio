/* !
@file    	    StdCapture.cpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements StdCapture: redirects the process's stdout/stderr file
    descriptors into pipes, reads them on background threads, and forwards each
    line to the Logger while the terminal keeps showing the original output.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/core/StdCapture.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/LogSinks.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Engine::Core
{
    namespace
    {
        // stdout and stderr are process-wide, so only one capture may own them at a time
        std::atomic<bool> g_captureActive{false};

        // A longer run of text without a newline is logged in pieces of this size
        constexpr std::size_t kMaxLineLength = std::size_t{16} * 1024;

#ifdef _WIN32
        constexpr unsigned int kPipeSize = 64 * 1024;

        // Platform wrappers around the underlying file-descriptor primitives used by
        // Redirect()/Restore()/ReadLoop(): FileNo, Dup, Dup2, Close, OpenPipe, MakeNotInheritable
        // and Read all behave the same on Windows and POSIX, only the underlying calls differ.

        /*!
        \brief   Returns the C file descriptor backing a FILE* stream.
        */
        int FileNo(std::FILE* stream)
        {
            return _fileno(stream);
        }
        /*!
        \brief   Duplicates fd to a new, unused descriptor.
        */
        int Dup(int fd)
        {
            return _dup(fd);
        }

        /*!
        \brief   Duplicates from onto the to descriptor, closing what to previously referred to.
        */
        int Dup2(int from, int to)
        {
            return _dup2(from, to);
        }

        /*!
        \brief   Closes a file descriptor.
        */
        void Close(int fd)
        {
            _close(fd);
        }

        /*!
        \brief   Opens an anonymous pipe, filling fds with its read and write ends.
        \return  true on success.
        */
        bool OpenPipe(int (&fds)[2])
        {
            return _pipe(fds, kPipeSize, _O_BINARY | _O_NOINHERIT) == 0;
        }

        /*!
        \brief   Prevents fd from being inherited by a child process. _dup2 makes the new
                 stdout/stderr inheritable; a child process holding the pipe's write end would
                 stop the reader thread from ever seeing the end of the pipe.
        */
        void MakeNotInheritable(int fd)
        {
            // _get_osfhandle returns the HANDLE as an integer
            const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd)); // NOLINT(performance-no-int-to-ptr)
            if (handle != INVALID_HANDLE_VALUE)
            {
                SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
            }
        }

        /*!
        \brief   Reads up to size bytes from fd into buffer.
        \return  Bytes read, 0 at end of stream, or a negative value on error.
        */
        int Read(int fd, char* buffer, std::size_t size)
        {
            return _read(fd, buffer, static_cast<unsigned int>(size));
        }
#else
        /*!
        \brief   Returns the C file descriptor backing a FILE* stream.
        */
        int FileNo(std::FILE* stream)
        {
            return fileno(stream);
        }

        /*!
        \brief   Duplicates fd to a new, unused descriptor.
        */
        int Dup(int fd)
        {
            return fcntl(fd, F_DUPFD_CLOEXEC, 0);
        }

        /*!
        \brief   Duplicates from onto the to descriptor, closing what to previously referred to;
                 retries on EINTR.
        */
        int Dup2(int from, int to)
        {
            int result = 0;
            do
            {
                result = dup2(from, to);
            } while (result < 0 && errno == EINTR);
            return result;
        }

        /*!
        \brief   Closes a file descriptor.
        */
        void Close(int fd)
        {
            close(fd);
        }

        /*!
        \brief   Opens an anonymous, close-on-exec pipe, filling fds with its read and write ends.
        \return  true on success.
        */
        bool OpenPipe(int (&fds)[2])
        {
            if (pipe(fds) != 0)
            {
                return false;
            }
            fcntl(fds[0], F_SETFD, FD_CLOEXEC);
            fcntl(fds[1], F_SETFD, FD_CLOEXEC);
            return true;
        }

        /*!
        \brief   No-op on POSIX: child processes that must not keep the pipe open are started with
                 their own stdout/stderr.
        */
        void MakeNotInheritable(int) {}

        /*!
        \brief   Reads up to size bytes from fd into buffer, retrying on EINTR.
        \return  Bytes read, 0 at end of stream, or a negative value on error.
        */
        ssize_t Read(int fd, char* buffer, std::size_t size)
        {
            ssize_t result = 0;
            do
            {
                result = read(fd, buffer, size);
            } while (result < 0 && errno == EINTR);
            return result;
        }
#endif

        /*!
        \brief   Points the terminal's stdout or stderr side at fd.
        \param   isError Targets stderr when true, stdout when false.
        \param   fd Descriptor to route terminal output through.
        */
        void SetTerminalTarget(bool isError, int fd)
        {
            Terminal::SetTargets(isError ? -1 : fd, isError ? fd : -1);
        }
    }

    StdCapture::StdCapture(Logger& logger) : logger_(logger)
    {
        channels_[0].stream = stdout;
        channels_[0].isError = false;
        channels_[1].stream = stderr;
        channels_[1].isError = true;
    }

    StdCapture::~StdCapture()
    {
        Stop();
    }

    // Redirects stdout/stderr into pipes so printf/std::cout from any library shows up in the log too
    bool StdCapture::Start()
    {
        if (running_)
        {
            return true;
        }

        bool expected = false;
        if (!g_captureActive.compare_exchange_strong(expected, true))
        {
            return false;
        }

        std::fflush(stdout);
        std::fflush(stderr);

        bool anyRedirected = false;
        for (Channel& channel : channels_)
        {
            anyRedirected = Redirect(channel) || anyRedirected;
        }

        if (!anyRedirected)
        {
            g_captureActive = false;
            return false;
        }

        running_ = true;
        return true;
    }

    // Puts stdout/stderr back the way they were
    void StdCapture::Stop()
    {
        if (!running_)
        {
            return;
        }

        for (Channel& channel : channels_)
        {
            Restore(channel);
        }

        running_ = false;
        g_captureActive = false;
    }

    bool StdCapture::Redirect(Channel& channel)
    {
        const int fd = FileNo(channel.stream);
        if (fd < 0)
        {
            return false;
        }

        const int savedFd = Dup(fd);
        if (savedFd < 0)
        {
            return false;
        }

        int pipeFds[2] = {-1, -1};
        if (!OpenPipe(pipeFds))
        {
            Close(savedFd);
            return false;
        }

        // Terminal output switches to the saved descriptor first: once fd feeds the Logger, echoing a
        // captured line back into fd would loop forever
        SetTerminalTarget(channel.isError, savedFd);

        if (Dup2(pipeFds[1], fd) < 0)
        {
            SetTerminalTarget(channel.isError, fd);
            Close(pipeFds[0]);
            Close(pipeFds[1]);
            Close(savedFd);
            return false;
        }

        // fd is now the only write end, so restoring it in Restore() ends the pipe
        Close(pipeFds[1]);
        MakeNotInheritable(fd);

        // Unbuffered, so each printf reaches the Console right away instead of when a buffer fills
        std::setvbuf(channel.stream, nullptr, _IONBF, 0);

        channel.fd = fd;
        channel.savedFd = savedFd;
        channel.reader = std::thread(&StdCapture::ReadLoop, this, pipeFds[0], channel.isError);
        return true;
    }

    void StdCapture::Restore(Channel& channel)
    {
        if (channel.savedFd < 0)
        {
            return;
        }

        std::fflush(channel.stream);

        // Closes the pipe's last write end: the reader logs what is left and exits
        Dup2(channel.savedFd, channel.fd);
        if (channel.reader.joinable())
        {
            channel.reader.join();
        }

        SetTerminalTarget(channel.isError, channel.fd);
        Close(channel.savedFd);

        channel.fd = -1;
        channel.savedFd = -1;
    }

    void StdCapture::ReadLoop(int readFd, bool isError)
    {
        const LogLevel level = isError ? LogLevel::Error : LogLevel::Info;
        const LogSource source = isError ? LogSource::Stderr : LogSource::Stdout;

        const auto emit = [&](std::string_view line)
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            if (!line.empty())
            {
                logger_.Write(level, kGeneralCategory, std::string(line), nullptr, 0, source);
            }
        };

        std::string pending;
        char buffer[4096];
        for (;;)
        {
            const auto count = Read(readFd, buffer, sizeof(buffer));
            if (count <= 0)
            {
                break;
            }

            std::string_view chunk(buffer, static_cast<std::size_t>(count));
            while (!chunk.empty())
            {
                const auto newline = chunk.find('\n');
                if (newline == std::string_view::npos)
                {
                    pending.append(chunk);
                    if (pending.size() >= kMaxLineLength)
                    {
                        emit(pending);
                        pending.clear();
                    }
                    break;
                }

                if (pending.empty())
                {
                    emit(chunk.substr(0, newline));
                }
                else
                {
                    pending.append(chunk.substr(0, newline));
                    emit(pending);
                    pending.clear();
                }
                chunk.remove_prefix(newline + 1);
            }
        }

        emit(pending);
        Close(readFd);
    }
}
