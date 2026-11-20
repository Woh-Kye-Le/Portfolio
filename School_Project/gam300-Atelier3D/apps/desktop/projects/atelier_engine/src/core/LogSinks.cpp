/* !
@file    	    LogSinks.cpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements Terminal, ConsoleSink, FileSink and MemorySink: coloured/plain
    terminal output, ANSI colour detection, per-session log file rotation, and
    the in-memory ring buffer used by the editor's Console panel.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/core/LogSinks.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Engine::Core
{
    namespace
    {
        enum class ColorSupport : std::uint8_t
        {
            Unknown,
            Yes,
            No,
        };

        std::mutex g_terminalMutex;
        int g_terminalOutFd = 1;
        int g_terminalErrFd = 2;
        // Checked on first use, and again whenever the targets change
        ColorSupport g_outColor = ColorSupport::Unknown;
        ColorSupport g_errColor = ColorSupport::Unknown;

        constexpr std::string_view kAnsiReset = "\x1b[0m";

        /*!
        \brief   Checks whether the environment requests colour output to be disabled.
        \return  true if NO_COLOR is set to a non-empty value (https://no-color.org).
        */
        bool NoColorRequested()
        {
#ifdef _WIN32
            wchar_t value[2] = {};
            return GetEnvironmentVariableW(L"NO_COLOR", value, 2) > 0;
#else
            const char* value = std::getenv("NO_COLOR"); // NOLINT(concurrency-mt-unsafe): read-only access
            return value != nullptr && value[0] != '\0';
#endif
        }

        /*!
        \brief   Detects whether fd is a console / terminal that understands ANSI colour codes,
                 enabling Windows virtual terminal processing on that handle if needed.
        \param   fd File descriptor to check.
        \return  true if fd supports ANSI colour output.
        */
        bool DetectColorSupport(int fd)
        {
            if (fd < 0 || NoColorRequested())
            {
                return false;
            }
#ifdef _WIN32
            // _get_osfhandle returns the HANDLE as an integer
            const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd)); // NOLINT(performance-no-int-to-ptr)
            DWORD mode = 0;
            if (handle == INVALID_HANDLE_VALUE || GetConsoleMode(handle, &mode) == 0)
            {
                return false; // redirected to a file or pipe
            }
            if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
            {
                return true;
            }
            // Windows 10+ consoles need this switched on to interpret colour codes
            return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
            if (isatty(fd) == 0)
            {
                return false;
            }
            const char* term = std::getenv("TERM"); // NOLINT(concurrency-mt-unsafe): read-only access
            return term != nullptr && std::strcmp(term, "dumb") != 0;
#endif
        }

        /*!
        \brief   Writes all of text to fd, retrying partial writes; gives up quietly on errors.
        \param   fd File descriptor to write to.
        \param   text Text to write.
        */
        void WriteAll(int fd, std::string_view text)
        {
            while (!text.empty())
            {
#ifdef _WIN32
                const auto chunk = static_cast<unsigned int>(std::min<std::size_t>(text.size(), 1u << 30));
                const int written = _write(fd, text.data(), chunk);
#else
                const ssize_t written = ::write(fd, text.data(), text.size());
                if (written < 0 && errno == EINTR)
                {
                    continue;
                }
#endif
                if (written <= 0)
                {
                    return;
                }
                text.remove_prefix(static_cast<std::size_t>(written));
            }
        }

        /*!
        \brief   Formats the local start time as "2026-09-27_14-03-12", safe in file names and
                 sorting by time.
        \return  The session's timestamp string.
        */
        std::string SessionStamp()
        {
            std::string stamp = FormatTimestamp(std::chrono::system_clock::now(), true).substr(0, 19);
            std::replace(stamp.begin(), stamp.end(), ' ', '_');
            std::replace(stamp.begin(), stamp.end(), ':', '-');
            return stamp;
        }

        /*!
        \brief   Checks whether file is a session log for baseName: <baseName>_<stamp>.log, plus
                 the older naming schemes <baseName>.log and <baseName>-<suffix>.log.
        \param   file Candidate file path.
        \param   baseName Filename prefix shared by every session's log file.
        \return  true if file matches one of the recognised naming schemes.
        */
        bool IsSessionLog(const std::filesystem::path& file, std::string_view baseName)
        {
            if (file.extension() != ".log")
            {
                return false;
            }
            const std::string stem = file.stem().string();
            if (stem == baseName)
            {
                return true;
            }
            return stem.size() > baseName.size() && stem.starts_with(baseName) &&
                   (stem[baseName.size()] == '_' || stem[baseName.size()] == '-');
        }

        /*!
        \brief   Deletes the oldest session logs in directory so that at most keep of them remain.
                 A file still open in another running editor cannot be removed and is simply kept.
        \param   directory Directory to prune session logs in.
        \param   baseName Filename prefix shared by every session's log file.
        \param   keep Maximum number of session logs to keep.
        */
        void RemoveOldSessions(const std::filesystem::path& directory, std::string_view baseName, std::size_t keep)
        {
            std::error_code ec;
            std::vector<std::filesystem::path> sessions;
            for (auto it = std::filesystem::directory_iterator(directory, ec);
                 !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
            {
                if (it->is_regular_file(ec) && IsSessionLog(it->path(), baseName))
                {
                    sessions.push_back(it->path());
                }
            }
            if (sessions.size() <= keep)
            {
                return;
            }

            // Names sort oldest first: the date and time come right after the base name
            std::sort(sessions.begin(), sessions.end(),
                      [](const auto& a, const auto& b) { return a.filename().string() < b.filename().string(); });
            const auto excess = static_cast<std::ptrdiff_t>(sessions.size() - keep);
            for (auto it = sessions.begin(); it != sessions.begin() + excess; ++it)
            {
                // Fails for a log another running editor still has open; that one is simply kept
                std::filesystem::remove(*it, ec);
            }
        }
    }

    void Terminal::Write(bool toStderr, std::string_view text)
    {
        const std::scoped_lock lock(g_terminalMutex);
        const int fd = toStderr ? g_terminalErrFd : g_terminalOutFd;
        if (fd >= 0)
        {
            WriteAll(fd, text);
        }
    }

    void Terminal::WriteLine(bool toStderr, std::string_view text, std::string_view ansiColor)
    {
        const std::scoped_lock lock(g_terminalMutex);
        const int fd = toStderr ? g_terminalErrFd : g_terminalOutFd;
        if (fd < 0)
        {
            return;
        }

        ColorSupport& support = toStderr ? g_errColor : g_outColor;
        if (support == ColorSupport::Unknown)
        {
            support = DetectColorSupport(fd) ? ColorSupport::Yes : ColorSupport::No;
        }

        // One write per line, so lines from different threads never mix colours
        std::string line;
        if (support == ColorSupport::Yes && !ansiColor.empty())
        {
            line.reserve(ansiColor.size() + text.size() + kAnsiReset.size() + 1);
            line.append(ansiColor).append(text).append(kAnsiReset);
        }
        else
        {
            line.reserve(text.size() + 1);
            line.append(text);
        }
        line.push_back('\n');
        WriteAll(fd, line);
    }

    void Terminal::SetTargets(int outFd, int errFd)
    {
        const std::scoped_lock lock(g_terminalMutex);
        if (outFd >= 0)
        {
            g_terminalOutFd = outFd;
            g_outColor = ColorSupport::Unknown;
        }
        if (errFd >= 0)
        {
            g_terminalErrFd = errFd;
            g_errColor = ColorSupport::Unknown;
        }
    }

    void Terminal::ResetTargets()
    {
        const std::scoped_lock lock(g_terminalMutex);
        g_terminalOutFd = 1;
        g_terminalErrFd = 2;
        g_outColor = ColorSupport::Unknown;
        g_errColor = ColorSupport::Unknown;
    }

    // Coloured line in the terminal; warnings and errors go to stderr
    void ConsoleSink::Write(const LogEntry& entry)
    {
        const bool toStderr = entry.level == LogLevel::Error;
        if (entry.source != LogSource::Engine)
        {
            Terminal::Write(toStderr, entry.message + "\n");
            return;
        }

        std::string_view color;
        switch (entry.level)
        {
        case LogLevel::Debug: color = "\x1b[94m"; break;   // bright blue
        case LogLevel::Info: break;                        // default text colour
        case LogLevel::Warning: color = "\x1b[93m"; break; // bright yellow
        case LogLevel::Error: color = "\x1b[91m"; break;   // bright red
        }
        Terminal::WriteLine(toStderr, FormatLogLine(entry, false), color);
    }

    FileSink::FileSink(const std::filesystem::path& directory, std::string_view baseName, std::size_t maxFiles)
    {
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);

        // Room for this session's file
        RemoveOldSessions(directory, baseName, std::max<std::size_t>(maxFiles, 1) - 1);

        const std::string stamp = SessionStamp();
        path_ = directory / std::format("{}_{}.log", baseName, stamp);
        // Two editors started within the same second
        for (int n = 2; std::filesystem::exists(path_, ec); ++n)
        {
            path_ = directory / std::format("{}_{}_{}.log", baseName, stamp, n);
        }

        file_.open(path_, std::ios::out | std::ios::trunc);
        // Reported through the logger so it reaches every sink already added (terminal and
        // Console panel), not only the terminal
        if (!file_.is_open())
        {
            LOG_WARN(Log,
                     "Log file could not be created - messages are shown here but will not be saved to disk "
                     "this session. Check that this folder is writable, then restart: {}",
                     directory.string());
            return;
        }

        file_ << "=== Atelier3D log started " << FormatTimestamp(std::chrono::system_clock::now(), true) << " ===\n";
        file_.flush();
    }

    // Plain text with the full date, so the file makes sense on its own later
    void FileSink::Write(const LogEntry& entry)
    {
        if (!file_.is_open())
        {
            return;
        }

        file_ << FormatLogLine(entry, true) << '\n';
        if (entry.level >= LogLevel::Warning)
        {
            file_.flush();
        }
    }

    void FileSink::Flush()
    {
        if (file_.is_open())
        {
            file_.flush();
        }
    }

    MemorySink::MemorySink(std::size_t capacity) : capacity_(std::max<std::size_t>(capacity, 1)) {}

    // Keeps the newest entries in memory until the Console panel collects them (oldest dropped when full)
    void MemorySink::Write(const LogEntry& entry)
    {
        const std::scoped_lock lock(mutex_);
        if (inbox_.size() >= capacity_)
        {
            inbox_.pop_front();
        }
        inbox_.push_back(entry);
    }

    // Hands over everything collected so far and empties the buffer
    void MemorySink::Drain(std::vector<LogEntry>& out)
    {
        const std::scoped_lock lock(mutex_);
        out.insert(out.end(), std::make_move_iterator(inbox_.begin()), std::make_move_iterator(inbox_.end()));
        inbox_.clear();
    }
}
