/* Start Header ************************************************************************/
/*!
\file       LogTests.cpp
\author     Henrey Hu [100%]
\par        zhihenghenrey.hu@digipen.edu
\date       27/9/2026
\brief      Unit tests for Logger, MemorySink, FileSink and StdCapture: message formatting,
            sink fan-out and failure handling, minimum-level filtering, the LOG_* macros,
            per-session log file rotation, and stdout/stderr capture.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "engine/core/Log.hpp"
#include "engine/core/LogSinks.hpp"
#include "engine/core/StdCapture.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using Engine::Core::FileSink;
using Engine::Core::ILogSink;
using Engine::Core::LogEntry;
using Engine::Core::Logger;
using Engine::Core::LogLevel;
using Engine::Core::LogSource;
using Engine::Core::MemorySink;
using Engine::Core::StdCapture;

namespace
{
    /*!
    \brief  Test double that records every entry it receives; safe to write from StdCapture's
            reader threads.
    */
    class RecordingSink final : public ILogSink
    {
    public:
        /*!
        \brief   Appends entry to the recorded list.
        */
        void Write(const LogEntry& entry) override
        {
            const std::scoped_lock lock(mutex_);
            entries_.push_back(entry);
        }

        /*!
        \brief   Returns a copy of every entry recorded so far.
        */
        std::vector<LogEntry> Entries()
        {
            const std::scoped_lock lock(mutex_);
            return entries_;
        }

        /*!
        \brief   Returns the messages of every recorded entry whose source matches.
        \param   source Source to filter by.
        */
        std::vector<std::string> Messages(LogSource source)
        {
            std::vector<std::string> messages;
            for (const auto& entry : Entries())
            {
                if (entry.source == source)
                {
                    messages.push_back(entry.message);
                }
            }
            return messages;
        }

    private:
        std::mutex mutex_;
        std::vector<LogEntry> entries_;
    };

    /*!
    \brief   Builds a LogEntry with the given level and message, timestamped now.
    \param   level Level to set on the entry.
    \param   message Message to set on the entry.
    \return  The constructed entry.
    */
    LogEntry MakeEntry(LogLevel level, std::string message)
    {
        LogEntry entry;
        entry.time = std::chrono::system_clock::now();
        entry.level = level;
        entry.message = std::move(message);
        return entry;
    }

    /*!
    \brief   Reads the full contents of a text file.
    \param   path File to read.
    \return  The file's contents.
    */
    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    /*!
    \brief  A fresh folder under the system temp directory, deleted again when the test ends.
    */
    class TempDir
    {
    public:
        /*!
        \brief   Creates a uniquely named temporary directory.
        */
        TempDir()
        {
            std::random_device random;
            path_ = std::filesystem::temp_directory_path() / ("AtelierLogTests-" + std::to_string(random()));
            std::filesystem::create_directories(path_);
        }

        /*!
        \brief   Recursively deletes the temporary directory. remove_all with an error_code
                 reports failures through ec instead of throwing.
        */
        ~TempDir() // NOLINT(bugprone-exception-escape)
        {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }

        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;

        /*!
        \brief   Returns the path of the temporary directory.
        */
        const std::filesystem::path& Path() const { return path_; }

    private:
        std::filesystem::path path_;
    };
}

/*!
\brief   Log: Format timestamp shows local time with milliseconds.
*/
// Timestamps use local time with milliseconds, with or without the date in front.
TEST(Log, FormatTimestampShowsLocalTimeWithMilliseconds)
{
    std::tm local{};
    local.tm_year = 2026 - 1900;
    local.tm_mon = 8;
    local.tm_mday = 27;
    local.tm_hour = 14;
    local.tm_min = 3;
    local.tm_sec = 12;
    local.tm_isdst = -1;
    const auto time = std::chrono::system_clock::from_time_t(std::mktime(&local)) + std::chrono::milliseconds(45);

    EXPECT_EQ(Engine::Core::FormatTimestamp(time, true), "2026-09-27 14:03:12.045");
    EXPECT_EQ(Engine::Core::FormatTimestamp(time, false), "14:03:12.045");
}

/*!
\brief   Log: Format log line matches the log file format.
*/
// A log file line is "[time] [Level] [Category] message", plus [stderr] for captured output.
TEST(Log, FormatLogLineMatchesTheLogFileFormat)
{
    LogEntry entry = MakeEntry(LogLevel::Warning, "Texture missing");
    entry.category = "Render";
    const std::string time = Engine::Core::FormatTimestamp(entry.time, true);
    EXPECT_EQ(Engine::Core::FormatLogLine(entry, true), "[" + time + "] [Warn] [Render] Texture missing");

    entry.level = LogLevel::Error;
    entry.category = "General";
    entry.source = LogSource::Stderr;
    EXPECT_EQ(Engine::Core::FormatLogLine(entry, true), "[" + time + "] [Error] [General] [stderr] Texture missing");
}

/*!
\brief   Log: Level names.
*/
// The level names shown in the Console and the log file (Warning is shortened to Warn).
TEST(Log, LevelNames)
{
    EXPECT_STREQ(Engine::Core::ToString(LogLevel::Debug), "Debug");
    EXPECT_STREQ(Engine::Core::ToString(LogLevel::Info), "Info");
    EXPECT_STREQ(Engine::Core::ToString(LogLevel::Warning), "Warn");
    EXPECT_STREQ(Engine::Core::ToString(LogLevel::Error), "Error");
}

/*!
\brief   Log: Base categories include the spec ones.
*/
// The categories the requirements ask for (Render, Audio, IO, ECS, Editor, Input) are all in the built-in list.
TEST(Log, BaseCategoriesIncludeTheSpecOnes)
{
    const auto& base = Engine::Core::kBaseCategories;
    for (const std::string_view category : {"General", "Render", "Audio", "IO", "ECS", "Editor", "Input"})
    {
        EXPECT_NE(std::find(base.begin(), base.end(), category), base.end()) << category;
    }
}

/*!
\brief   Logger: Writes every message to every sink.
*/
// One message goes to every sink with its level, category, text and source file/line.
TEST(Logger, WritesEveryMessageToEverySink)
{
    Logger logger;
    const auto first = std::make_shared<RecordingSink>();
    const auto second = std::make_shared<RecordingSink>();
    logger.AddSink(first);
    logger.AddSink(second);

    logger.Write(LogLevel::Warning, "Resource", "Texture missing", "Assets.cpp", 42);

    for (const auto& sink : {first, second})
    {
        const auto entries = sink->Entries();
        ASSERT_EQ(entries.size(), 1u);
        EXPECT_EQ(entries[0].level, LogLevel::Warning);
        EXPECT_EQ(entries[0].category, "Resource");
        EXPECT_EQ(entries[0].source, LogSource::Engine);
        EXPECT_EQ(entries[0].message, "Texture missing");
        EXPECT_STREQ(entries[0].file, "Assets.cpp");
        EXPECT_EQ(entries[0].line, 42);
    }
}

/*!
\brief   Logger: Empty category becomes general.
*/
// No category given = "General".
TEST(Logger, EmptyCategoryBecomesGeneral)
{
    Logger logger;
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);
    logger.Write(LogLevel::Info, "", "no category");

    const auto entries = sink->Entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].category, "General");
}

/*!
\brief   Logger: Removed sink gets no more messages.
*/
// After RemoveSink the sink only has what came before.
TEST(Logger, RemovedSinkGetsNoMoreMessages)
{
    Logger logger;
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);
    logger.Write(LogLevel::Info, "General", "before");
    logger.RemoveSink(sink);
    logger.Write(LogLevel::Info, "General", "after");

    const auto entries = sink->Entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].message, "before");
}

/*!
\brief   Logger: Drops messages below the minimum level.
*/
// Raising the minimum level at runtime drops the less important messages before they reach any sink.
TEST(Logger, DropsMessagesBelowTheMinimumLevel)
{
    Logger logger;
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);
    EXPECT_EQ(logger.MinLevel(), LogLevel::Debug);

    logger.SetMinLevel(LogLevel::Warning);
    EXPECT_FALSE(logger.ShouldLog(LogLevel::Info));
    EXPECT_TRUE(logger.ShouldLog(LogLevel::Error));
    logger.Write(LogLevel::Debug, "Input", "debug");
    logger.Write(LogLevel::Info, "General", "printed line", nullptr, 0, LogSource::Stdout);
    logger.Write(LogLevel::Warning, "Audio", "warning");
    logger.Write(LogLevel::Error, "General", "stderr line", nullptr, 0, LogSource::Stderr);

    logger.SetMinLevel(LogLevel::Debug);
    logger.Write(LogLevel::Debug, "Input", "debug again");

    std::vector<std::string> messages;
    for (const auto& entry : sink->Entries())
    {
        messages.push_back(entry.message);
    }
    EXPECT_EQ(messages, (std::vector<std::string>{"warning", "stderr line", "debug again"}));
}

/*!
\brief   Logger: Sink that logs does not deadlock.
*/
// A sink that logs from inside Write would lock the logger twice - the inner message must just be dropped.
TEST(Logger, SinkThatLogsDoesNotDeadlock)
{
    class LoggingSink final : public ILogSink
    {
    public:
        explicit LoggingSink(Logger& logger) : logger_(logger) {}
        void Write(const LogEntry&) override { logger_.Write(LogLevel::Error, "General", "from inside a sink"); }

    private:
        Logger& logger_;
    };

    Logger logger;
    const auto recorder = std::make_shared<RecordingSink>();
    logger.AddSink(std::make_shared<LoggingSink>(logger));
    logger.AddSink(recorder);

    logger.Write(LogLevel::Info, "General", "hello");

    const auto entries = recorder->Entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].message, "hello");
}

/*!
\brief   Logger: Failing sink does not stop the others or throw.
*/
// A sink that throws (e.g. disk full) must not stop the other sinks or crash the caller.
TEST(Logger, FailingSinkDoesNotStopTheOthersOrThrow)
{
    class ThrowingSink final : public ILogSink
    {
    public:
        void Write(const LogEntry&) override { throw std::runtime_error("disk full"); }
        void Flush() override { throw std::runtime_error("disk full"); }
    };

    Logger logger;
    const auto recorder = std::make_shared<RecordingSink>();
    logger.AddSink(std::make_shared<ThrowingSink>());
    logger.AddSink(recorder);

    EXPECT_NO_THROW(logger.Write(LogLevel::Error, "IO", "still delivered"));
    EXPECT_NO_THROW(logger.Flush());

    ASSERT_EQ(recorder->Entries().size(), 1u);
    EXPECT_EQ(recorder->Entries()[0].message, "still delivered");
    EXPECT_EQ(logger.DeliveryFailures(), 2u);
}

/*!
\brief   Logger: Macros format and record the call site.
*/
// LOG_WARN formats its arguments and records the file and line it was called from.
TEST(Logger, MacrosFormatAndRecordTheCallSite)
{
    const auto sink = std::make_shared<RecordingSink>();
    Logger::Get().AddSink(sink);
    const int expectedLine = __LINE__ + 1;
    LOG_WARN(Render, "Loaded {} of {} meshes", 3, 4);
    Logger::Get().RemoveSink(sink);

    const auto entries = sink->Entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].level, LogLevel::Warning);
    EXPECT_EQ(entries[0].category, "Render");
    EXPECT_EQ(entries[0].message, "Loaded 3 of 4 meshes");
    ASSERT_NE(entries[0].file, nullptr);
    EXPECT_EQ(std::filesystem::path(entries[0].file).filename(), "LogTests.cpp");
    EXPECT_EQ(entries[0].line, expectedLine);
}

/*!
\brief   Logger: Macro turns any name into a category.
*/
// Any name works as a category in the macros, so new systems can log without registering anything.
TEST(Logger, MacroTurnsAnyNameIntoACategory)
{
    auto& logger = Logger::Get();
    const LogLevel previous = logger.MinLevel();
    logger.SetMinLevel(LogLevel::Debug);
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);

    // Neither name is in kBaseCategories: new systems need no registration
    LOG_DEBUG(Networking, "Connected to {}", "server");
    LOG_ERROR(SkeletonRigging, "Bone {} missing", 7);

    logger.RemoveSink(sink);
    logger.SetMinLevel(previous);

    const auto entries = sink->Entries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].level, LogLevel::Debug);
    EXPECT_EQ(entries[0].category, "Networking");
    EXPECT_EQ(entries[0].message, "Connected to server");
    EXPECT_EQ(entries[1].category, "SkeletonRigging");
}

/*!
\brief   Logger: Filtered macro does not evaluate its arguments.
*/
// When the level is filtered out, the macro's arguments are not even evaluated (so debug logging costs nothing).
TEST(Logger, FilteredMacroDoesNotEvaluateItsArguments)
{
    auto& logger = Logger::Get();
    const LogLevel previous = logger.MinLevel();
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);

    int evaluations = 0;
    const auto expensive = [&evaluations]
    {
        ++evaluations;
        return 42;
    };
    logger.SetMinLevel(LogLevel::Info);
    LOG_DEBUG(Input, "Mouse at {}", expensive());
    logger.SetMinLevel(LogLevel::Debug);
    LOG_DEBUG(Input, "Mouse at {}", expensive());

    logger.RemoveSink(sink);
    logger.SetMinLevel(previous);

    EXPECT_EQ(evaluations, 1);
    const auto entries = sink->Entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].message, "Mouse at 42");
}

/*!
\brief   Logger: Write with a runtime category reaches the sinks; an empty category is General.
*/
// Write() also takes a category from a variable, and an empty one still becomes General.
TEST(Logger, WriteWithRuntimeCategoryReachesTheSinks)
{
    const auto sink = std::make_shared<RecordingSink>();
    Logger::Get().AddSink(sink);
    Logger::Get().Write(LogLevel::Error, "", "System::init failed");
    const std::string category = "Audio";
    Logger::Get().Write(LogLevel::Warning, category, "No sound device");
    Logger::Get().RemoveSink(sink);

    const auto entries = sink->Entries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].level, LogLevel::Error);
    EXPECT_EQ(entries[0].category, "General");
    EXPECT_EQ(entries[0].message, "System::init failed");
    EXPECT_EQ(entries[0].file, nullptr);
    EXPECT_EQ(entries[1].category, "Audio");
    EXPECT_EQ(entries[1].message, "No sound device");
}

/*!
\brief   MemorySink: Drain returns messages oldest first only once.
*/
// Drain hands over messages oldest first and empties the buffer, so the Console never shows one twice.
TEST(MemorySink, DrainReturnsMessagesOldestFirstOnlyOnce)
{
    MemorySink sink(10);
    sink.Write(MakeEntry(LogLevel::Info, "one"));
    sink.Write(MakeEntry(LogLevel::Error, "two"));

    std::vector<LogEntry> drained;
    sink.Drain(drained);
    ASSERT_EQ(drained.size(), 2u);
    EXPECT_EQ(drained[0].message, "one");
    EXPECT_EQ(drained[1].message, "two");

    drained.clear();
    sink.Drain(drained);
    EXPECT_TRUE(drained.empty());
}

/*!
\brief   MemorySink: Drops the oldest past capacity.
*/
// When the buffer is full the oldest messages are dropped.
TEST(MemorySink, DropsTheOldestPastCapacity)
{
    MemorySink sink(3);
    for (int i = 1; i <= 5; ++i)
    {
        sink.Write(MakeEntry(LogLevel::Info, std::to_string(i)));
    }

    std::vector<LogEntry> drained;
    sink.Drain(drained);
    ASSERT_EQ(drained.size(), 3u);
    EXPECT_EQ(drained[0].message, "3");
    EXPECT_EQ(drained[2].message, "5");
}

/*!
\brief   FileSink: Writes timestamped lines and flushes errors immediately.
*/
// The log file gets the same timestamped lines as the format above, including captured printf output.
TEST(FileSink, WritesTimestampedLinesAndFlushesErrorsImmediately)
{
    const TempDir dir;
    FileSink sink(dir.Path() / "logs", "editor");
    ASSERT_TRUE(sink.IsOpen());
    EXPECT_EQ(sink.FilePath().parent_path(), dir.Path() / "logs");

    LogEntry entry = MakeEntry(LogLevel::Error, "Could not load brick.png");
    entry.category = "Resource";
    sink.Write(entry);

    LogEntry captured = MakeEntry(LogLevel::Info, "printed with printf");
    captured.source = LogSource::Stdout;
    sink.Write(captured);
    sink.Flush();

    const std::string text = ReadFile(sink.FilePath());
    EXPECT_NE(text.find("=== Atelier3D log started "), std::string::npos);
    EXPECT_NE(text.find("[" + Engine::Core::FormatTimestamp(entry.time, true) +
                        "] [Error] [Resource] Could not load brick.png\n"),
              std::string::npos);
    EXPECT_NE(text.find("] [Info] [General] [stdout] printed with printf\n"), std::string::npos);
}

/*!
\brief   FileSink: Errors are on disk before flush.
*/
// Errors are written straight to disk, so they are not lost if the program crashes right after.
TEST(FileSink, ErrorsAreOnDiskBeforeFlush)
{
    const TempDir dir;
    FileSink sink(dir.Path(), "editor");
    sink.Write(MakeEntry(LogLevel::Error, "crash imminent"));

    EXPECT_NE(ReadFile(sink.FilePath()).find("crash imminent"), std::string::npos);
}

/*!
\brief   FileSink: Each session gets its own dated file.
*/
// Every run gets its own editor_<date>_<time>.log file, and the older one is left alone.
TEST(FileSink, EachSessionGetsItsOwnDatedFile)
{
    const TempDir dir;
    std::filesystem::path firstPath;
    {
        FileSink first(dir.Path(), "editor");
        first.Write(MakeEntry(LogLevel::Info, "first session"));
        firstPath = first.FilePath();
    }
    FileSink second(dir.Path(), "editor");
    second.Write(MakeEntry(LogLevel::Error, "second session"));

    const std::regex name(R"(editor_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(_\d+)?\.log)");
    EXPECT_TRUE(std::regex_match(firstPath.filename().string(), name)) << firstPath;
    EXPECT_TRUE(std::regex_match(second.FilePath().filename().string(), name)) << second.FilePath();
    ASSERT_NE(firstPath, second.FilePath());

    const std::string firstText = ReadFile(firstPath);
    const std::string secondText = ReadFile(second.FilePath());
    EXPECT_NE(firstText.find("first session"), std::string::npos);
    EXPECT_EQ(firstText.find("second session"), std::string::npos);
    EXPECT_NE(secondText.find("second session"), std::string::npos);
    EXPECT_EQ(secondText.find("first session"), std::string::npos);
}

/*!
\brief   FileSink: Editors running at once write separate files.
*/
// Two editors started in the same second still write to different files.
TEST(FileSink, EditorsRunningAtOnceWriteSeparateFiles)
{
    const TempDir dir;
    FileSink first(dir.Path(), "editor");
    FileSink second(dir.Path(), "editor");
    first.Write(MakeEntry(LogLevel::Error, "first editor"));
    second.Write(MakeEntry(LogLevel::Error, "second editor"));

    ASSERT_NE(first.FilePath(), second.FilePath());
    EXPECT_EQ(ReadFile(first.FilePath()).find("second editor"), std::string::npos);
    EXPECT_EQ(ReadFile(second.FilePath()).find("first editor"), std::string::npos);
}

/*!
\brief   FileSink: Keeps only the newest sessions.
*/
// Only the newest session logs are kept; files that are not session logs are never deleted.
TEST(FileSink, KeepsOnlyTheNewestSessions)
{
    const TempDir dir;
    // Older sessions, including the previous naming scheme, and files that are not session logs
    for (const char* name : {"editor.log", "editor-prev.log", "editor-1234.log", "editor_2026-01-01_00-00-01.log",
                             "editor_2026-01-01_00-00-02.log", "editor_2026-01-01_00-00-03.log",
                             "editor_2026-01-01_00-00-04.log", "game.log", "notes.txt", "editorial.log"})
    {
        std::ofstream(dir.Path() / name) << "old";
    }

    FileSink sink(dir.Path(), "editor", 3);
    ASSERT_TRUE(sink.IsOpen());

    std::vector<std::string> remaining;
    for (const auto& file : std::filesystem::directory_iterator(dir.Path()))
    {
        remaining.push_back(file.path().filename().string());
    }
    std::sort(remaining.begin(), remaining.end());

    std::vector<std::string> expected{"editor_2026-01-01_00-00-03.log",
                                      "editor_2026-01-01_00-00-04.log",
                                      sink.FilePath().filename().string(),
                                      "editorial.log",
                                      "game.log",
                                      "notes.txt"};
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(remaining, expected);
}

/*!
\brief   FileSink: Unwritable location is ignored instead of failing.
*/
// If the logs folder cannot be created, the sink just does nothing instead of crashing.
TEST(FileSink, UnwritableLocationIsIgnoredInsteadOfFailing)
{
    const TempDir dir;
    // A file where the log folder should be: the folder cannot be created
    std::ofstream(dir.Path() / "blocked") << "not a folder";

    FileSink sink(dir.Path() / "blocked" / "logs", "editor");
    EXPECT_FALSE(sink.IsOpen());
    sink.Write(MakeEntry(LogLevel::Error, "goes nowhere"));
    sink.Flush();
}

/*!
\brief   StdCapture: Turns printed lines into log messages.
*/
// printf, puts and std::cout lines all become log messages (stdout as Info, stderr as Error).
TEST(StdCapture, TurnsPrintedLinesIntoLogMessages)
{
    Logger logger;
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);

    StdCapture capture(logger);
    ASSERT_TRUE(capture.Start());
    std::printf("printf %d\n", 1);
    std::puts("puts line");
    std::cout << "cout line\n";
    std::fprintf(stderr, "fprintf stderr\n");
    std::cerr << "cerr line\n";
    capture.Stop();

    EXPECT_EQ(sink->Messages(LogSource::Stdout), (std::vector<std::string>{"printf 1", "puts line", "cout line"}));
    EXPECT_EQ(sink->Messages(LogSource::Stderr), (std::vector<std::string>{"fprintf stderr", "cerr line"}));

    for (const auto& entry : sink->Entries())
    {
        EXPECT_EQ(entry.level, entry.source == LogSource::Stderr ? LogLevel::Error : LogLevel::Info);
        EXPECT_EQ(entry.category, "General");
        EXPECT_EQ(entry.file, nullptr);
    }
}

/*!
\brief   StdCapture: Joins partial writes and strips line endings.
*/
// A line printed in two pieces becomes one message, and \r\n line endings are trimmed.
TEST(StdCapture, JoinsPartialWritesAndStripsLineEndings)
{
    Logger logger;
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);

    StdCapture capture(logger);
    ASSERT_TRUE(capture.Start());
    std::printf("par");
    std::printf("tial\n");
    std::printf("windows line\r\n");
    std::printf("\n");
    std::printf("no newline at the end");
    capture.Stop();

    EXPECT_EQ(sink->Messages(LogSource::Stdout),
              (std::vector<std::string>{"partial", "windows line", "no newline at the end"}));
}

/*!
\brief   StdCapture: Restores stdout when stopped.
*/
// After Stop, printing goes back to the terminal and is no longer captured.
TEST(StdCapture, RestoresStdoutWhenStopped)
{
    Logger logger;
    const auto sink = std::make_shared<RecordingSink>();
    logger.AddSink(sink);

    StdCapture capture(logger);
    ASSERT_TRUE(capture.Start());
    EXPECT_TRUE(capture.IsRunning());
    std::printf("captured\n");
    capture.Stop();
    capture.Stop();
    EXPECT_FALSE(capture.IsRunning());

    std::printf("[StdCapture test] this line is printed normally, not captured\n");
    std::fflush(stdout);

    EXPECT_EQ(sink->Messages(LogSource::Stdout), (std::vector<std::string>{"captured"}));
}

/*!
\brief   StdCapture: Only one capture at a time.
*/
// stdout can only be redirected once, so a second capture is refused until the first stops.
TEST(StdCapture, OnlyOneCaptureAtATime)
{
    Logger logger;
    StdCapture first(logger);
    StdCapture second(logger);

    ASSERT_TRUE(first.Start());
    EXPECT_FALSE(second.Start());
    first.Stop();

    EXPECT_TRUE(second.Start());
    second.Stop();
}
