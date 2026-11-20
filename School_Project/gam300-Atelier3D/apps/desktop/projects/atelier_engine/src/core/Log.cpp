/* !
@file    	    Log.cpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements Logger and the LogEntry formatting helpers declared in Log.hpp:
    timestamping, level/source-to-string conversion, and dispatching each entry
    to every registered sink.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/core/Log.hpp"

#include "engine/core/LogSinks.hpp"

#include <ctime>
#include <utility>

namespace Engine::Core
{
    namespace
    {
        // Set while this thread is inside Logger::Write, so a sink that logs by mistake drops the
        // message instead of deadlocking on the Logger's mutex
        thread_local bool t_insideWrite = false;

        /*!
        \brief  RAII guard that sets t_insideWrite for its lifetime.
        */
        class InsideWriteScope
        {
        public:
            InsideWriteScope() { t_insideWrite = true; }
            ~InsideWriteScope() { t_insideWrite = false; }

            InsideWriteScope(const InsideWriteScope&) = delete;
            InsideWriteScope& operator=(const InsideWriteScope&) = delete;
        };
    }

    /*!
    \brief   Converts a LogLevel to its display name ("Debug", "Info", "Warn", "Error").
    \param   level The level to convert.
    \return  A null-terminated static string; "Unknown" for an out-of-range value.
    */
    const char* ToString(LogLevel level) noexcept
    {
        switch (level)
        {
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info: return "Info";
        case LogLevel::Warning: return "Warn";
        case LogLevel::Error: return "Error";
        }
        return "Unknown";
    }

    /*!
    \brief   Converts a LogSource to its display name ("stdout", "stderr"); Engine has none since
             it is the implicit default and is not tagged in the formatted line.
    \param   source The source to convert.
    \return  A null-terminated static string; empty for Engine or an out-of-range value.
    */
    const char* ToString(LogSource source) noexcept
    {
        switch (source)
        {
        case LogSource::Engine: return "";
        case LogSource::Stdout: return "stdout";
        case LogSource::Stderr: return "stderr";
        }
        return "";
    }

    /*!
    \brief   Formats a time point as "HH:MM:SS.mmm", or "YYYY-MM-DD HH:MM:SS.mmm" with withDate.
    \param   time The time point to format, converted to local time.
    \param   withDate Includes the date prefix when true.
    \return  The formatted timestamp.
    */
    std::string FormatTimestamp(std::chrono::system_clock::time_point time, bool withDate)
    {
        const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &seconds);
#else
        localtime_r(&seconds, &local);
#endif
        // to_time_t truncates, so the remainder is always in [0, 1000)
        const auto sinceEpoch = time.time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            sinceEpoch - std::chrono::duration_cast<std::chrono::seconds>(sinceEpoch));
        const auto ms = static_cast<int>((millis.count() + 1000) % 1000);

        if (withDate)
        {
            return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}", local.tm_year + 1900, local.tm_mon + 1,
                               local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, ms);
        }
        return std::format("{:02}:{:02}:{:02}.{:03}", local.tm_hour, local.tm_min, local.tm_sec, ms);
    }

    /*!
    \brief   Formats a full log line: "[time] [level] [category] message", with an extra
             "[source]" segment when entry did not come from engine code.
    \param   entry The entry to format.
    \param   withDate Includes the date in the timestamp when true.
    \return  The formatted line, without a trailing newline.
    */
    std::string FormatLogLine(const LogEntry& entry, bool withDate)
    {
        const std::string time = FormatTimestamp(entry.time, withDate);
        if (entry.source == LogSource::Engine)
        {
            return std::format("[{}] [{}] [{}] {}", time, ToString(entry.level), entry.category, entry.message);
        }
        return std::format("[{}] [{}] [{}] [{}] {}", time, ToString(entry.level), entry.category,
                           ToString(entry.source), entry.message);
    }

    /*!
    \brief   Returns the process-wide Logger, created on first use with a ConsoleSink already
             attached and never destroyed, so code that logs during static destruction stays safe.
    \return  Reference to the singleton Logger.
    */
    Logger& Logger::Get()
    {
        static Logger* logger = []
        {
            // Never destroyed: code that runs during static destruction can still log safely
            auto* instance = new Logger();
            instance->SetMinLevel(DefaultMinLevel());
            instance->AddSink(std::make_shared<ConsoleSink>());
            return instance;
        }();
        return *logger;
    }

    /*!
    \brief   Returns the default minimum level for a fresh Logger: Debug in debug builds, Info in
             release (NDEBUG) builds.
    \return  The default minimum LogLevel.
    */
    LogLevel Logger::DefaultMinLevel() noexcept
    {
#ifdef NDEBUG
        return LogLevel::Info;
#else
        return LogLevel::Debug;
#endif
    }

    /*!
    \brief   Registers a sink to receive every future log entry; ignored if sink is null.
    \param   sink Sink to add.
    */
    void Logger::AddSink(std::shared_ptr<ILogSink> sink)
    {
        if (!sink)
        {
            return;
        }
        const std::scoped_lock lock(mutex_);
        sinks_.push_back(std::move(sink));
    }

    /*!
    \brief   Unregisters a previously added sink; does nothing if it was not registered.
    \param   sink Sink to remove.
    */
    void Logger::RemoveSink(const std::shared_ptr<ILogSink>& sink)
    {
        const std::scoped_lock lock(mutex_);
        std::erase(sinks_, sink);
    }

    /*!
    \brief   Builds a LogEntry and dispatches it to every registered sink, unless level is below
             the minimum level or this thread is already inside a Write call (which would
             otherwise deadlock or recurse if a sink logs). Never throws: a failure while building
             or dispatching the entry is swallowed and the message is dropped.
    \param   level Severity of the message.
    \param   category Log category; empty falls back to the general category.
    \param   message The message text.
    \param   file Source file the call came from, for engine call sites.
    \param   line Source line the call came from, for engine call sites.
    \param   source Where the message originated (engine code, captured stdout, captured stderr).
    */
    void Logger::Write(LogLevel level, std::string_view category, std::string message, const char* file, int line,
                       LogSource source, LogTarget target) noexcept
    {
        if (t_insideWrite || !ShouldLog(level))
        {
            return;
        }

        try
        {
            LogEntry entry;
            entry.level = level;
            entry.source = source;
            entry.category = category.empty() ? kGeneralCategory : category;
            entry.message = std::move(message);
            entry.file = file;
            entry.line = line;
            entry.target = target;

            const std::scoped_lock lock(mutex_);
            // Stamped under the lock so the timestamps in every sink are in the order the sinks see them
            entry.time = std::chrono::system_clock::now();

            const InsideWriteScope insideWrite;
            for (const auto& sink : sinks_)
            {
                // A failing sink (disk full, out of memory...) must not stop the others
                try
                {
                    sink->Write(entry);
                }
                catch (...)
                {
                    NoteDeliveryFailure();
                }
            }
        }
        catch (...)
        {
            // Out of memory or the lock failed: the message is lost, the program carries on
            NoteDeliveryFailure();
        }
    }

    /*!
    \brief   Flushes every registered sink that buffers output. Never throws: a failing sink is
             noted and the rest are still flushed.
    */
    void Logger::Flush() noexcept
    {
        try
        {
            const std::scoped_lock lock(mutex_);
            for (const auto& sink : sinks_)
            {
                try
                {
                    sink->Flush();
                }
                catch (...)
                {
                    NoteDeliveryFailure();
                }
            }
        }
        catch (...)
        {
            NoteDeliveryFailure();
        }
    }
}
