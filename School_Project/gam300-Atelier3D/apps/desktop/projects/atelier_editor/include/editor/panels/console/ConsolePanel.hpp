/* !
@file    	    ConsolePanel.hpp
@author  	    zhihenghenrey.hu@digipen.edu [71%]
                sanni.go@digipen.edu [20%]
         	    c.kojat@digipen.edu [9%]
@date    		27/09/2026
@brief
    Declares ConsolePanel, the editor panel that shows the log messages
    collected by a MemorySink: timestamped, coloured by level, filterable by
    level, category and text.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"

#include "engine/core/Log.hpp"

#include <imgui/imgui.h>

#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Engine::Core
{
    class MemorySink;
}

namespace Editor::Panels
{
    /*!
    \brief  Shows the editor's log messages: timestamped, coloured by level, filterable by level,
            category and text. Everything logged or printed reaches it through the MemorySink; the
            same messages are in the log file in logDirectory, which Clear leaves untouched.
    */
    class ConsolePanel final : public Panel
    {
    public:
        static constexpr const char* kTitle = "Console";
        static constexpr std::size_t kMaxEntries = 5000;

        /*!
        \brief   Constructs the panel, seeding the category filter with every base category so they
                 are listed even before their first message arrives.
        \param   sink MemorySink to drain captured log entries from.
        \param   logDirectory Directory shown by, and opened from, the "Open log folder" button.
        */
        ConsolePanel(std::shared_ptr<Engine::Core::MemorySink> sink, std::filesystem::path logDirectory);

        /*!
        \brief   Draws the panel's contents into the current window: collects new entries, then the
                 toolbar and the entry table.
        */
        void DrawContents() override;

        void SetOnOpen(std::function<void(const Engine::Core::LogTarget&)> callback) { onOpen_ = std::move(callback); }

    private:
        /*!
        \brief   Drains new entries from the sink into history_, updating the per-level and
                 per-category counts and trimming history_ down to kMaxEntries.
        */
        void CollectNewEntries();

        /*!
        \brief   Empties the panel's visible history and resets counts, without touching the log
                 file. Categories stay listed (and hidden ones stay hidden); only their counts
                 restart.
        */
        void Clear();

        /*!
        \brief   Rebuilds visibleRows_ from history_ using the current filters. Called lazily
                 whenever rowsDirty_ is set, instead of on every filter change directly.
        */
        void RebuildVisibleRows();

        /*!
        \brief   Returns whether entry passes the current level, category and search filters.
        \param   entry Entry to test.
        \return  true if entry should be drawn.
        */
        bool IsShown(const Engine::Core::LogEntry& entry) const;

        /*!
        \brief   Draws the toolbar row: Clear, level toggles, category filter, minimum level combo,
                 auto-scroll checkbox, "Open log folder", the debug-only test menu, and the search
                 box.
        */
        void DrawToolbar();

        /*!
        \brief   Draws the toggle button for one level, showing its running count and dimmed when
                 the level is hidden.
        \param   level Level the button toggles.
        */
        void DrawLevelToggle(Engine::Core::LogLevel level);

        /*!
        \brief   Draws the "Categories" button and its popup, letting the user show/hide categories
                 individually or with All/None.
        */
        void DrawCategoryFilter();

        /*!
        \brief   Draws the "Log level" combo bound to the global Logger's minimum level; messages
                 below the selected level are not recorded anywhere (panel, terminal, log file).
        */
        void DrawMinLevelCombo();

        /*!
        \brief   Draws the debug-only "Test" menu used to exercise logging: one message per level,
                 one per category, raw stdout/stderr output, and a 10,000-message stress test.
        */
        void DrawTestMenu();

        /*!
        \brief   Draws the clipped, scrollable table of visible log entries, preserving scroll
                 position when old rows drop off the top and following new messages when
                 auto-scroll is on.
        */
        void DrawEntries();

        /*!
        \brief   Draws one row of the entry table: time, level, category, first line of the
                 message, a tooltip with the full message and source location, and a copy context
                 menu.
        \param   historyIndex Index of the entry within history_.
        */
        void DrawEntryRow(int historyIndex);

        /*!
        \brief   Opens logDirectory_ in the OS file explorer (ShellExecuteW on Windows, xdg-open on
                 POSIX). Logs a warning if the folder could not be opened.
        */
        void OpenLogFolder() const;

        std::shared_ptr<Engine::Core::MemorySink> sink_;
        std::filesystem::path logDirectory_;

        std::deque<Engine::Core::LogEntry> history_;
        std::vector<Engine::Core::LogEntry> incoming_;
        std::vector<int> visibleRows_;
        std::array<int, Engine::Core::kLogLevelCount> levelCounts_{};
        std::array<bool, Engine::Core::kLogLevelCount> showLevel_{true, true, true, true};
        // Every category seen so far (the base ones from the start), sorted by name, with its message count
        std::map<std::string, int, std::less<>> categoryCounts_;
        // By name, so a category that is hidden stays hidden for messages that arrive later
        std::set<std::string, std::less<>> hiddenCategories_;
        int droppedVisibleRows_ = 0;
        ImGuiTextFilter filter_;
        bool rowsDirty_ = true;
        bool autoScroll_ = true;
        bool scrollToBottom_ = false;
        bool wasAtBottom_ = true;
        std::function<void(const Engine::Core::LogTarget&)> onOpen_;
    };
}
