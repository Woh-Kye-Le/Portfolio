/* !
@file    	    ConsolePanel.cpp
@author  	    zhihenghenrey.hu@digipen.edu [71%]
                sanni.go@digipen.edu [20%]
         	    c.kojat@digipen.edu [9%]
@date    		27/09/2026
@brief
    Implements the editor's Console panel: draws the log toolbar (level/category
    filters, minimum level, search) and the scrolling table of captured log
    entries drained from a MemorySink.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/console/ConsolePanel.hpp"

#include "engine/core/LogSinks.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Needs windows.h first
#include <shellapi.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <thread>

extern char** environ;
#endif

namespace Editor::Panels
{
    namespace
    {
        using Engine::Core::LogEntry;
        using Engine::Core::LogLevel;
        using Engine::Core::LogSource;

        constexpr std::array kLevels{LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error};

        /*!
        \brief   Returns the index of level within kLevels / the per-level count arrays.
        */
        std::size_t LevelIndex(LogLevel level)
        {
            return static_cast<std::size_t>(level);
        }

        /*!
        \brief   Returns the display colour for level. Also written as text next to the colour, so
                 levels stay readable for colour-blind users.
        */
        ImVec4 LevelColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Debug: return {0.45f, 0.7f, 1.0f, 1.0f};
            case LogLevel::Info: break;
            case LogLevel::Warning: return {1.0f, 0.78f, 0.3f, 1.0f};
            case LogLevel::Error: return {1.0f, 0.4f, 0.4f, 1.0f};
            }
            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }

        /*!
        \brief   Returns the first line of message. Only the first line fits a row (every row must
                 be the same height for ImGuiListClipper).
        */
        std::string_view FirstLine(std::string_view message)
        {
            return message.substr(0, message.find('\n'));
        }

        /*!
        \brief   Continues the toolbar on the same line if itemWidth fits, wraps onto a new line
                 otherwise.
        \param   itemWidth Width of the item about to be drawn.
        */
        void SameLineIfFits(float itemWidth)
        {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGui::SameLine();
            if (ImGui::GetContentRegionAvail().x < itemWidth + spacing)
            {
                ImGui::NewLine();
            }
        }

        /*!
        \brief   Returns the width a button with the given label would take, including padding.
        */
        float ButtonWidth(const char* label)
        {
            return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        }

        /*!
        \brief   Returns the width a checkbox with the given label would take.
        */
        float CheckboxWidth(const char* label)
        {
            return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
                   ImGui::CalcTextSize(label, nullptr, true).x;
        }

        /*!
        \brief   Returns the width of the widest level name in kLevels, for sizing the level combo.
        */
        float WidestLevelNameWidth()
        {
            return std::accumulate(kLevels.begin(), kLevels.end(), 0.0f, [](float width, LogLevel level)
                                   { return std::max(width, ImGui::CalcTextSize(Engine::Core::ToString(level)).x); });
        }
    }

    /*!
    \brief   Constructs the panel, seeding the category filter with every base category so they
             are listed even before their first message arrives.
    \param   sink MemorySink to drain captured log entries from.
    \param   logDirectory Directory shown by, and opened from, the "Open log folder" button.
    */
    ConsolePanel::ConsolePanel(std::shared_ptr<Engine::Core::MemorySink> sink, std::filesystem::path logDirectory)
        : Panel(kTitle), sink_(std::move(sink)), logDirectory_(std::move(logDirectory))
    {
        for (const std::string_view category : Engine::Core::kBaseCategories)
        {
            categoryCounts_.emplace(category, 0);
        }
    }

    /*!
    \brief   Draws the toolbar and entry table into the current window (the docked editor hosts the panel in
             its own window).
    */
    void ConsolePanel::DrawContents()
    {
        CollectNewEntries();
        DrawToolbar();
        ImGui::Separator();
        DrawEntries();
    }

    /*!
    \brief   Drains new entries from the sink into history_, updating the per-level and
             per-category counts and trimming history_ down to kMaxEntries.
    */
    void ConsolePanel::CollectNewEntries()
    {
        if (!sink_)
        {
            return;
        }

        incoming_.clear();
        sink_->Drain(incoming_);
        if (incoming_.empty())
        {
            return;
        }

        for (LogEntry& entry : incoming_)
        {
            ++levelCounts_[LevelIndex(entry.level)];
            // A category logged for the first time is added here, shown by default
            ++categoryCounts_[entry.category];
            history_.push_back(std::move(entry));
        }
        while (history_.size() > kMaxEntries)
        {
            const LogEntry& oldest = history_.front();
            if (IsShown(oldest))
            {
                ++droppedVisibleRows_;
            }
            --levelCounts_[LevelIndex(oldest.level)];
            if (const auto it = categoryCounts_.find(oldest.category); it != categoryCounts_.end())
            {
                --it->second;
            }
            history_.pop_front();
        }

        rowsDirty_ = true;
        // Follow new messages only if the user was already looking at the latest ones
        if (autoScroll_ && wasAtBottom_)
        {
            scrollToBottom_ = true;
        }
    }

    /*!
    \brief   Empties the panel's visible history and resets counts, without touching the log file.
             Categories stay listed (and hidden ones stay hidden); only their counts restart.
    */
    void ConsolePanel::Clear()
    {
        history_.clear();
        visibleRows_.clear();
        levelCounts_.fill(0);
        // Categories stay listed (and hidden ones stay hidden); only their counts restart
        for (auto& entry : categoryCounts_)
        {
            entry.second = 0;
        }
        droppedVisibleRows_ = 0;
        rowsDirty_ = false;
        wasAtBottom_ = true;
    }

    /*!
    \brief   Returns whether entry passes the current level, category and search filters.
    \param   entry Entry to test.
    \return  true if entry should be drawn.
    */
    bool ConsolePanel::IsShown(const LogEntry& entry) const
    {
        if (!showLevel_[LevelIndex(entry.level)] || hiddenCategories_.contains(entry.category))
        {
            return false;
        }
        if (!filter_.IsActive())
        {
            return true;
        }
        // The search matches the category too ("Audio" finds every Audio message)
        const std::string text = entry.category + ' ' + entry.message;
        return filter_.PassFilter(text.data(), text.data() + text.size());
    }

    /*!
    \brief   Rebuilds visibleRows_ from history_ using the current filters. Called lazily whenever
             rowsDirty_ is set, instead of on every filter change directly.
    */
    void ConsolePanel::RebuildVisibleRows()
    {
        visibleRows_.clear();
        for (std::size_t i = 0; i < history_.size(); ++i)
        {
            if (IsShown(history_[i]))
            {
                visibleRows_.push_back(static_cast<int>(i));
            }
        }
        rowsDirty_ = false;
    }

    /*!
    \brief   Draws the toolbar row: Clear, level toggles, category filter, minimum level combo,
             auto-scroll checkbox, "Open log folder", the debug-only test menu, and the search box.
    */
    void ConsolePanel::DrawToolbar()
    {
        if (ImGui::Button("Clear"))
        {
            Clear();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Empty the panel (the log file keeps every message)");
        }

        for (const LogLevel level : kLevels)
        {
            DrawLevelToggle(level);
        }

        DrawCategoryFilter();
        DrawMinLevelCombo();

        SameLineIfFits(CheckboxWidth("Auto-scroll"));
        if (ImGui::Checkbox("Auto-scroll", &autoScroll_) && autoScroll_)
        {
            scrollToBottom_ = true;
        }

        SameLineIfFits(ButtonWidth("Open log folder"));
        if (ImGui::Button("Open log folder"))
        {
            OpenLogFolder();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("%s", logDirectory_.string().c_str());
        }

#ifndef NDEBUG
        DrawTestMenu();
#endif

        // The search box takes the rest of the line, or a line of its own when little space is left
        constexpr float kMinSearchWidth = 160.0f;
        SameLineIfFits(kMinSearchWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##search", "Search (-word excludes)", filter_.InputBuf,
                                     IM_ARRAYSIZE(filter_.InputBuf)))
        {
            filter_.Build();
            rowsDirty_ = true;
        }
    }

    /*!
    \brief   Draws the toggle button for one level, showing its running count and dimmed when the
             level is hidden.
    \param   level Level the button toggles.
    */
    void ConsolePanel::DrawLevelToggle(LogLevel level)
    {
        const std::size_t index = LevelIndex(level);
        const bool shown = showLevel_[index];
        // ### keeps the ID stable while the count in the label changes
        const std::string label =
            std::format("{} {}###level{}", Engine::Core::ToString(level), levelCounts_[index], index);

        // Room for a 4-digit count, so the toolbar does not shift while messages arrive
        const std::string widestLabel = std::format("{} 0000", Engine::Core::ToString(level));
        const float width = std::max(ButtonWidth(label.c_str()), ButtonWidth(widestLabel.c_str()));

        SameLineIfFits(width);
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(level));
        if (!shown)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
        }

        if (ImGui::Button(label.c_str(), ImVec2(width, 0.0f)))
        {
            showLevel_[index] = !shown;
            rowsDirty_ = true;
        }

        if (!shown)
        {
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip(shown ? "Hide %s messages" : "Show %s messages", Engine::Core::ToString(level));
        }
    }

    /*!
    \brief   Draws the "Categories" button and its popup, letting the user show/hide categories
             individually or with All/None.
    */
    void ConsolePanel::DrawCategoryFilter()
    {
        const std::size_t total = categoryCounts_.size();
        const std::size_t shownCount = total - std::min(total, hiddenCategories_.size());
        const std::string label = hiddenCategories_.empty()
                                      ? std::string("Categories###categories")
                                      : std::format("Categories {}/{}###categories", shownCount, total);
        // Room for "Categories 00/00", so the toolbar does not shift when categories are hidden
        const float width = std::max(ButtonWidth(label.c_str()), ButtonWidth("Categories 00/00"));

        SameLineIfFits(width);
        if (ImGui::Button(label.c_str(), ImVec2(width, 0.0f)))
        {
            ImGui::OpenPopup("##categories");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Show or hide messages by category");
        }

        if (!ImGui::BeginPopup("##categories"))
        {
            return;
        }

        if (ImGui::Button("All"))
        {
            hiddenCategories_.clear();
            rowsDirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("None"))
        {
            for (const auto& entry : categoryCounts_)
            {
                hiddenCategories_.insert(entry.first);
            }
            rowsDirty_ = true;
        }
        ImGui::Separator();

        for (const auto& [category, count] : categoryCounts_)
        {
            bool shown = !hiddenCategories_.contains(category);
            ImGui::PushID(category.c_str());
            if (ImGui::Checkbox(category.c_str(), &shown))
            {
                if (shown)
                {
                    hiddenCategories_.erase(category);
                }
                else
                {
                    hiddenCategories_.insert(category);
                }
                rowsDirty_ = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%d", count);
            ImGui::PopID();
        }

        ImGui::EndPopup();
    }

    /*!
    \brief   Draws the "Log level" combo bound to the global Logger's minimum level; messages
             below the selected level are not recorded anywhere (panel, terminal, log file).
    */
    void ConsolePanel::DrawMinLevelCombo()
    {
        auto& logger = Engine::Core::Logger::Get();
        const LogLevel current = logger.MinLevel();

        constexpr const char* kLabel = "Log level";
        const ImGuiStyle& style = ImGui::GetStyle();
        const float comboWidth = WidestLevelNameWidth() + ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f;
        SameLineIfFits(ImGui::CalcTextSize(kLabel).x + style.ItemInnerSpacing.x + comboWidth);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(kLabel);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::SetNextItemWidth(comboWidth);
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(current));
        const bool open = ImGui::BeginCombo("##minLevel", Engine::Core::ToString(current));
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Messages below this level are not recorded anywhere (panel, terminal, log file)");
        }
        if (!open)
        {
            return;
        }

        for (const LogLevel level : kLevels)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(level));
            if (ImGui::Selectable(Engine::Core::ToString(level), level == current))
            {
                logger.SetMinLevel(level);
            }
            ImGui::PopStyleColor();
            if (level == current)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    /*!
    \brief   Draws the debug-only "Test" menu used to exercise logging: one message per level, one
             per category, raw stdout/stderr output, and a 10,000-message stress test.
    */
    void ConsolePanel::DrawTestMenu()
    {
        SameLineIfFits(ButtonWidth("Test"));
        if (ImGui::Button("Test"))
        {
            ImGui::OpenPopup("##consoleTest");
        }

        if (!ImGui::BeginPopup("##consoleTest"))
        {
            return;
        }

        if (ImGui::MenuItem("Log one message of each level"))
        {
            LOG_DEBUG(Editor, "Test debug message: player position x:{}, y:{}", 30, 45);
            LOG_INFO(Editor, "Test info message");
            LOG_WARN(Editor, "Test warning message");
            LOG_ERROR(Editor, "Test error message");
        }
        if (ImGui::MenuItem("Log one message per category"))
        {
            for (const std::string_view category : Engine::Core::kBaseCategories)
            {
                // Category only known at runtime, so the Logger is called directly instead of a LOG_* macro
                Engine::Core::Logger::Get().Write(LogLevel::Info, category,
                                                  std::format("Test message in category {}", category));
            }
            LOG_INFO(Tutorial, "Test message in a category that is not in the base list (added automatically)");
        }
        if (ImGui::MenuItem("Print with printf, cout, cerr"))
        {
            std::printf("Test line from printf\n");
            std::cout << "Test line from std::cout\n";
            std::fprintf(stderr, "Test line from fprintf(stderr)\n");
            std::cerr << "Test line from std::cerr\n";
        }
        if (ImGui::MenuItem("Log 10,000 messages"))
        {
            for (int i = 1; i <= 10000; ++i)
            {
                LOG_INFO(Editor, "Stress test message {} of 10000", i);
            }
        }

        ImGui::EndPopup();
    }

    /*!
    \brief   Draws the clipped, scrollable table of visible log entries, preserving scroll position
             when old rows drop off the top and following new messages when auto-scroll is on.
    */
    void ConsolePanel::DrawEntries()
    {
        if (rowsDirty_)
        {
            RebuildVisibleRows();
        }

        if (visibleRows_.empty())
        {
            ImGui::TextDisabled("%s", history_.empty() ? "No messages" : "No messages match the current filters");
            droppedVisibleRows_ = 0;
            wasAtBottom_ = true;
            scrollToBottom_ = false;
            return;
        }

        constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_SizingFixedFit;
        // Its own ID: imgui.ini still holds column widths saved for the earlier 3-column "##entries" table,
        // which would squeeze the Category column
        if (!ImGui::BeginTable("##logEntries", 4, kFlags))
        {
            return;
        }

        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("00:00:00.000").x);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, WidestLevelNameWidth());
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Performance").x);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

        // Old rows dropped off the top: scroll up by as much, so the rows being read stay where they are
        if (droppedVisibleRows_ > 0 && !scrollToBottom_)
        {
            const float rowHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2.0f;
            ImGui::SetScrollY(
                std::max(0.0f, ImGui::GetScrollY() - static_cast<float>(droppedVisibleRows_) * rowHeight));
        }
        droppedVisibleRows_ = 0;

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visibleRows_.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                DrawEntryRow(visibleRows_[static_cast<std::size_t>(row)]);
            }
        }

        if (scrollToBottom_)
        {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom_ = false;
            // The scroll is applied next frame, so GetScrollY() still reports the old position
            wasAtBottom_ = true;
        }
        else
        {
            wasAtBottom_ = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        }

        ImGui::EndTable();
    }

    /*!
    \brief   Draws one row of the entry table: time, level, category, first line of the message,
             a tooltip with the full message and source location, and a copy context menu.
    \param   historyIndex Index of the entry within history_.
    */
    void ConsolePanel::DrawEntryRow(int historyIndex)
    {
        const LogEntry& entry = history_[static_cast<std::size_t>(historyIndex)];
        const ImVec4 color = LevelColor(entry.level);
        const std::string time = Engine::Core::FormatTimestamp(entry.time, false);
        const std::string_view firstLine = FirstLine(entry.message);

        ImGui::TableNextRow();
        // deque elements keep their address while others are added or dropped: a stable ID per message
        ImGui::PushID(&entry);

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::Selectable(time.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
        ImGui::PopStyleColor();

        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45.0f);
            ImGui::TextColored(color, "%s", Engine::Core::ToString(entry.level));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", entry.category.c_str());
            ImGui::TextUnformatted(entry.message.data(), entry.message.data() + entry.message.size());
            ImGui::Separator();
            if (entry.file != nullptr)
            {
                ImGui::TextDisabled("%s:%d", entry.file, entry.line);
            }
            else if (entry.source != LogSource::Engine)
            {
                ImGui::TextDisabled("Printed to %s", Engine::Core::ToString(entry.source));
            }
            else
            {
                ImGui::TextDisabled("Logged without a source location");
            }
            ImGui::TextDisabled("%s", Engine::Core::FormatTimestamp(entry.time, true).c_str());
            if (entry.target.object != 0)
            {
                ImGui::TextDisabled(entry.target.block != 0 ? "Use Open block to jump to the problem"
                                                            : "Use the source link to select the object");
            }
            ImGui::TextDisabled("Right-click to copy");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        if (ImGui::BeginPopupContextItem("##row"))
        {
            if (ImGui::MenuItem("Copy"))
            {
                ImGui::SetClipboardText(Engine::Core::FormatLogLine(entry, true).c_str());
            }
            if (ImGui::MenuItem("Copy message"))
            {
                ImGui::SetClipboardText(entry.message.c_str());
            }
            ImGui::EndPopup();
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(color, "%s", Engine::Core::ToString(entry.level));

        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("%s", entry.category.c_str());

        ImGui::TableSetColumnIndex(3);
        if (entry.target.object != 0 && onOpen_)
        {
            const char* label = entry.target.block != 0 ? "Open block"
                                : entry.target.script   ? "Open script"
                                                        : "Open object";

            if (ImGui::TextLink(label))
            {
                onOpen_(entry.target);
            }

            ImGui::SameLine();
        }
        if (entry.source != LogSource::Engine)
        {
            ImGui::TextDisabled("[%s]", Engine::Core::ToString(entry.source));
            ImGui::SameLine();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(firstLine.data(), firstLine.data() + firstLine.size());
        ImGui::PopStyleColor();
        if (firstLine.size() < entry.message.size())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(more)");
        }

        ImGui::PopID();
    }

    /*!
    \brief   Opens logDirectory_ in the OS file explorer (ShellExecuteW on Windows, xdg-open on
             POSIX). Logs a warning if the folder could not be opened.
    */
    void ConsolePanel::OpenLogFolder() const
    {
#ifdef _WIN32
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", logDirectory_.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        const bool opened = result > 32;
#else
        // Child output goes to /dev/null: it must not inherit the captured stdout/stderr pipes
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

        const std::string folder = logDirectory_.string();
        char program[] = "xdg-open";
        char* argv[] = {program, const_cast<char*>(folder.c_str()), nullptr};
        pid_t pid = 0;
        const bool opened = posix_spawnp(&pid, program, &actions, nullptr, argv, environ) == 0;
        posix_spawn_file_actions_destroy(&actions);
        if (opened)
        {
            // Reap the child so it does not linger as a zombie
            std::thread([pid] { waitpid(pid, nullptr, 0); }).detach();
        }
#endif
        if (!opened)
        {
            LOG_WARN(Editor, "Could not open the log folder {}", logDirectory_.string());
        }
    }
}
