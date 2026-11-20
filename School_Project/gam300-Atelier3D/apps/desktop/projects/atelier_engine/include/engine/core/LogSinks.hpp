/* !
@file    	    LogSinks.hpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares the log sink implementations (Terminal, ConsoleSink, FileSink,
    MemorySink) that a Logger writes each LogEntry to.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/core/Log.hpp"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <vector>

namespace Engine::Core
{
    /*!
    \brief  The process's real console window / terminal.
            Writes straight to file descriptors instead of through printf / std::cout, because
            StdCapture may be redirecting stdout and stderr into the Logger; going through them
            would loop forever.
    */
    class Terminal
    {
    public:
        /*!
        \brief   Writes raw text to the real stdout or stderr file descriptor.
        \param   toStderr Writes to stderr when true, stdout when false.
        \param   text The text to write, unmodified.
        */
        static void Write(bool toStderr, std::string_view text);

        /*!
        \brief   Writes text and a newline in the given ANSI colour when the target is a console
                 that shows colours, as plain text otherwise (redirected to a file, NO_COLOR set...).
        \param   toStderr Writes to stderr when true, stdout when false.
        \param   text The text to write, without a trailing newline.
        \param   ansiColor ANSI escape sequence for the colour (e.g. "[93m").
        */
        static void WriteLine(bool toStderr, std::string_view text, std::string_view ansiColor);

        /*!
        \brief   Used by StdCapture: send terminal output to these descriptors instead of
                 stdout/stderr (-1 keeps the current one).
        \param   outFd Descriptor to use for stdout, or -1 to leave it unchanged.
        \param   errFd Descriptor to use for stderr, or -1 to leave it unchanged.
        */
        static void SetTargets(int outFd, int errFd);

        /*!
        \brief   Restores Write()/WriteLine() to target the real stdout and stderr descriptors.
        */
        static void ResetTargets();
    };

    /*!
    \brief  Prints to the terminal: "[14:03:12.345] [Warn] [Audio] message", coloured by level
            (Debug blue, Warn yellow, Error red). Errors go to stderr. Lines captured from
            stdout/stderr are echoed unchanged, so the terminal looks the same as without capture.
    */
    class ConsoleSink final : public ILogSink
    {
    public:
        /*!
        \brief   Formats and prints entry to the terminal.
        \param   entry The log entry to print.
        */
        void Write(const LogEntry& entry) override;
    };

    /*!
    \brief  Writes one file per session, <directory>/<baseName>_2026-09-27_14-03-12.log, and
            deletes the oldest <baseName>*.log files so that at most maxFiles remain (files still
            open in another editor are kept). Line format:
            "[2026-09-27 14:03:12.345] [Warn] [Audio] message". Warnings and errors are flushed
            immediately so they survive a crash; the rest is flushed by Flush() and on destruction.
            If the file cannot be opened IsOpen() returns false and every message is ignored
            instead of failing; a warning is logged to the sinks already added to the Logger.
    */
    class FileSink final : public ILogSink
    {
    public:
        static constexpr std::size_t kDefaultMaxFiles = 10;

        /*!
        \brief   Opens (or creates) the session's log file and prunes older files past maxFiles.
        \param   directory Directory the log file is created in.
        \param   baseName Filename prefix shared by every session's log file.
        \param   maxFiles Maximum number of <baseName>*.log files to keep, oldest deleted first.
        */
        FileSink(const std::filesystem::path& directory, std::string_view baseName,
                 std::size_t maxFiles = kDefaultMaxFiles);
        FileSink(const FileSink&) = delete;
        FileSink& operator=(const FileSink&) = delete;

        /*!
        \brief   Formats entry and appends it to the log file.
        \param   entry The log entry to write.
        */
        void Write(const LogEntry& entry) override;

        /*!
        \brief   Flushes any buffered output to disk.
        */
        void Flush() override;

        /*!
        \brief   Returns the path of the file this sink writes to.
        */
        const std::filesystem::path& FilePath() const noexcept { return path_; }

        /*!
        \brief   Returns true if the log file was opened successfully.
        */
        bool IsOpen() const { return file_.is_open(); }

    private:
        std::filesystem::path path_;
        std::ofstream file_;
    };

    /*!
    \brief  Keeps the latest messages in memory until someone collects them with Drain() (the
            editor's Console panel, once per frame). Safe to write from any thread; the oldest
            messages are dropped past capacity.
    */
    class MemorySink final : public ILogSink
    {
    public:
        /*!
        \brief   Constructs a sink that keeps at most capacity messages.
        \param   capacity Maximum number of messages retained before the oldest are dropped.
        */
        explicit MemorySink(std::size_t capacity);

        /*!
        \brief   Appends entry to the in-memory buffer, dropping the oldest entry if at capacity.
        \param   entry The log entry to store.
        */
        void Write(const LogEntry& entry) override;

        /*!
        \brief   Appends every message written since the last Drain() to out, oldest first.
        \param   out Vector that newly buffered entries are appended to.
        */
        void Drain(std::vector<LogEntry>& out);

    private:
        std::mutex mutex_;
        std::deque<LogEntry> inbox_;
        std::size_t capacity_;
    };
}
