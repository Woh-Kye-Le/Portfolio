/* !
@file    	    Log.hpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares the logging API: LogLevel/LogSource/LogEntry, the ILogSink
    interface, the thread-safe Logger, and the
    LOG_DEBUG/LOG_INFO/LOG_WARN/LOG_ERROR macros used throughout the engine and
    editor.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Core
{
    // In order of how serious they are, so "level >= Warning" works for filtering
    enum class LogLevel : std::uint8_t
    {
        Debug,
        Info,
        Warning,
        Error,
    };

    inline constexpr std::size_t kLogLevelCount = 4;

    /*!
    \brief   Converts a LogLevel to its display name.
    \param   level The level to convert.
    \return  "Debug", "Info", "Warn" or "Error".
    */
    const char* ToString(LogLevel level) noexcept;

    /*!
    \brief  Where a message came from: a LOG_* call, or a line printed to stdout/stderr
            and picked up by StdCapture (printf, std::cout, std::cerr, third-party libraries...).
    */
    enum class LogSource : std::uint8_t
    {
        Engine,
        Stdout,
        Stderr,
    };

    /*!
    \brief   Converts a LogSource to its display name.
    \param   source The source to convert.
    \return  "", "stdout" or "stderr".
    */
    const char* ToString(LogSource source) noexcept;

    // Category of printed output (printf / std::cout / std::cerr) and of messages with no category
    inline constexpr std::string_view kGeneralCategory = "General";

    // The engine's systems, listed up front so the Console panel can offer them as filters before they
    // log anything. Any other name works too: LOG_INFO(Networking, ...) simply adds a "Networking" category.
    inline constexpr std::array<std::string_view, 21> kBaseCategories{
        "General",   "Editor",    "UI",     "Render",   "Shader",    "Camera",  "Texture",
        "Input",     "ECS",       "Prefab", "Resource", "IO",        "Physics", "Lighting",
        "Particles", "Animation", "Audio",  "Gizmos",   "Scripting", "Network", "Performance",
    };

    struct LogTarget
    {
        std::uint32_t object = 0;
        std::uint32_t block = 0;
        std::uint64_t session = 0;
        bool script = false;
    };

    struct LogEntry
    {
        std::chrono::system_clock::time_point time;
        LogLevel level = LogLevel::Info;
        LogSource source = LogSource::Engine;
        std::string category{kGeneralCategory};
        std::string message;
        // Source location of LOG_* calls; nullptr / 0 when unknown (captured output, Logger::Write)
        const char* file = nullptr;
        int line = 0;
        LogTarget target;
    };

    /*!
    \brief   Formats a time point as local time.
    \param   time The time point to format.
    \param   withDate Includes the date prefix when true.
    \return  "14:03:12.345", or "2026-09-27 14:03:12.345" with the date.
    */
    std::string FormatTimestamp(std::chrono::system_clock::time_point time, bool withDate);

    /*!
    \brief   Formats a full log line as it appears in the log file.
    \param   entry The entry to format.
    \param   withDate Includes the date in the timestamp when true.
    \return  "[2026-09-27 14:03:12.345] [Warn] [Audio] message", with the source for captured
             output: "[...] [Info] [General] [stdout] message".
    */
    std::string FormatLogLine(const LogEntry& entry, bool withDate);

    /*!
    \brief  One destination for log messages (terminal, log file, editor console...).
            The Logger calls its sinks one at a time, so a sink only needs its own locking for
            state it shares with other threads. A sink must never log itself or print to
            stdout/stderr (use Terminal in LogSinks.hpp): that would loop back into the Logger.
    */
    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;
        virtual void Write(const LogEntry& entry) = 0;
        virtual void Flush() {}
    };

    /*!
    \brief  Timestamps every message and hands it to all sinks. Thread-safe.
            Messages below the minimum level are dropped before they reach any sink. Logging never
            throws, so it is safe in destructors and error handlers: a message that cannot be
            delivered is dropped.
    */
    class Logger
    {
    public:
        // Starts with no sinks and minimum level Debug (used by tests); the shared Logger::Get() starts
        // with a ConsoleSink and the build's default minimum level
        Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        /*!
        \brief   Returns the process-wide Logger, created on first use with a ConsoleSink already
                 attached and never destroyed, so code that logs during static destruction stays
                 safe.
        \return  Reference to the singleton Logger.
        */
        static Logger& Get();

        /*!
        \brief   Returns the default minimum level for a fresh Logger.
        \return  Debug in Debug builds, Info in Release builds.
        */
        static LogLevel DefaultMinLevel() noexcept;

        /*!
        \brief   Registers a sink to receive every future log entry; ignored if sink is null.
        \param   sink Sink to add.
        */
        void AddSink(std::shared_ptr<ILogSink> sink);

        /*!
        \brief   Unregisters a previously added sink; does nothing if it was not registered.
        \param   sink Sink to remove.
        */
        void RemoveSink(const std::shared_ptr<ILogSink>& sink);

        /*!
        \brief   Sets the minimum level a message must reach to be delivered to any sink.
        \param   level New minimum level.
        */
        void SetMinLevel(LogLevel level) noexcept { minLevel_.store(level, std::memory_order_relaxed); }

        /*!
        \brief   Returns the current minimum level.
        */
        LogLevel MinLevel() const noexcept { return minLevel_.load(std::memory_order_relaxed); }

        /*!
        \brief   Returns whether a message at level would be delivered.
        \param   level Level to test against the current minimum level.
        */
        bool ShouldLog(LogLevel level) const noexcept { return level >= MinLevel(); }

        /*!
        \brief   Records that a message (or flush) was lost because formatting or a sink failed;
                 logging itself never throws.
        */
        void NoteDeliveryFailure() noexcept { deliveryFailures_.fetch_add(1, std::memory_order_relaxed); }

        /*!
        \brief   Returns the number of messages or flushes lost since construction.
        */
        std::size_t DeliveryFailures() const noexcept { return deliveryFailures_.load(std::memory_order_relaxed); }

        /*!
        \brief   Builds a LogEntry and dispatches it to every registered sink, unless level is
                 below the minimum level. Never throws: a failure while building or dispatching the
                 entry is swallowed and the message is dropped.
        \param   level Severity of the message.
        \param   category Log category; empty falls back to the general category.
        \param   message The message text.
        \param   file Source file the call came from, for engine call sites.
        \param   line Source line the call came from, for engine call sites.
        \param   source Where the message originated (engine code, captured stdout, captured
                 stderr).
        */
        void Write(LogLevel level, std::string_view category, std::string message, const char* file = nullptr,
                   int line = 0, LogSource source = LogSource::Engine, LogTarget target = {}) noexcept;

        /*!
        \brief   Flushes every registered sink that buffers output.
        */
        void Flush() noexcept;

    private:
        std::atomic<LogLevel> minLevel_{LogLevel::Debug};
        std::atomic<std::size_t> deliveryFailures_{0};
        std::mutex mutex_;
        std::vector<std::shared_ptr<ILogSink>> sinks_;
    };
}

// Usage: LOG_INFO(Audio, "Loaded {} sounds from {}", count, path.string());
// The first argument is the category as a plain name (Render, Audio, IO, ECS, Editor, Input... see
// kBaseCategories); a new name creates a new category. The format string is checked at compile time;
// log a runtime string with LOG_INFO(Editor, "{}", text).
// A message below the minimum level costs one comparison: its arguments are not even evaluated.
// Never throws (a message whose formatting fails is dropped), so it can be used in destructors.
#define ATELIER_LOG_AT(level, categoryName, ...)                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        auto& atelierLogger_ = ::Engine::Core::Logger::Get();                                                          \
        if (atelierLogger_.ShouldLog(level))                                                                           \
        {                                                                                                              \
            try                                                                                                        \
            {                                                                                                          \
                atelierLogger_.Write(level, categoryName, std::format(__VA_ARGS__), __FILE__, __LINE__);               \
            }                                                                                                          \
            catch (...)                                                                                                \
            {                                                                                                          \
                atelierLogger_.NoteDeliveryFailure();                                                                  \
            }                                                                                                          \
        }                                                                                                              \
    } while (false)

#define LOG_DEBUG(category, ...) ATELIER_LOG_AT(::Engine::Core::LogLevel::Debug, #category, __VA_ARGS__)
#define LOG_INFO(category, ...) ATELIER_LOG_AT(::Engine::Core::LogLevel::Info, #category, __VA_ARGS__)
#define LOG_WARN(category, ...) ATELIER_LOG_AT(::Engine::Core::LogLevel::Warning, #category, __VA_ARGS__)
#define LOG_ERROR(category, ...) ATELIER_LOG_AT(::Engine::Core::LogLevel::Error, #category, __VA_ARGS__)
