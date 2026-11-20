/* !
@file    	    ExecutionHighlight.hpp
@date    		30/10/2026
@brief
    Declares how a block is drawn while the game runs (F02 Execution
    Highlighting): the block a script just ran is current, the one before it
    previous and the one it runs next next. Works out the states from the
    scripts' positions, with no ImGui, so it can be tested.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/LogicRunner.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Editor::Panels::ExecutionHighlight
{
    // How a block is drawn, strongest first
    enum class Highlight : std::uint8_t
    {
        None,
        Previous, // ran earlier since the script started (a loop's earlier rounds too)
        Next,     // runs next: while paused, half its colour
        Current,  // ran last: its colour
    };

    // The blocks in each state, sorted, across all of an object's scripts
    struct ScriptHighlights
    {
        std::vector<std::uint32_t> current;
        std::vector<std::uint32_t> previous;
        std::vector<std::uint32_t> next;
    };

    // A running script gives all three. One that ended in the latest frame or Step still shows its last block as
    // current, with no next; after that every block it ran, its last one too, is previous. One that has not started
    // gives nothing. A block is not also its own script's previous or next (a wait shows only as current).
    inline ScriptHighlights Collect(const std::vector<Engine::Logic::LogicRunner::ScriptState>& scripts)
    {
        ScriptHighlights found;
        for (const Engine::Logic::LogicRunner::ScriptState& script : scripts)
        {
            if (script.last == 0)
            {
                continue;
            }
            if (!script.running && !script.justEnded)
            {
                found.previous.insert(found.previous.end(), script.before.begin(), script.before.end());
                found.previous.push_back(script.last);
                continue;
            }
            found.current.push_back(script.last);
            const bool next = script.running && script.next != 0 && script.next != script.last;
            for (const std::uint32_t block : script.before)
            {
                if (block != script.last && !(next && block == script.next))
                {
                    found.previous.push_back(block);
                }
            }
            if (next)
            {
                found.next.push_back(script.next);
            }
        }
        for (std::vector<std::uint32_t>* each : {&found.current, &found.previous, &found.next})
        {
            std::sort(each->begin(), each->end());
        }
        return found;
    }

    // Current beats Next beats Previous: a block one script just ran and another runs next shows as current
    inline Highlight HighlightOf(const ScriptHighlights& highlights, std::uint32_t blockId)
    {
        const auto has = [blockId](const std::vector<std::uint32_t>& blocks)
        { return std::binary_search(blocks.begin(), blocks.end(), blockId); };
        if (has(highlights.current))
        {
            return Highlight::Current;
        }
        if (has(highlights.next))
        {
            return Highlight::Next;
        }
        return has(highlights.previous) ? Highlight::Previous : Highlight::None;
    }

    // The blocks that keep their colour while the game plays, sorted: those running now (active, as ActiveBlocks gives
    // them, which holds a block that ran for a moment long enough to see, and highlights' current), and everything
    // they run inside of: the blocks holding them and their script's hat
    inline std::vector<std::uint32_t> RunningBlocks(const Engine::Logic::LogicDesc& logic,
                                                    std::vector<std::uint32_t> active,
                                                    const ScriptHighlights& highlights)
    {
        active.insert(active.end(), highlights.current.begin(), highlights.current.end());
        std::vector<std::uint32_t> running = active;
        const auto holders = [&](const auto& self, const std::vector<Engine::Logic::Block>& blocks) -> void
        {
            for (const Engine::Logic::Block& block : blocks)
            {
                if (Engine::Logic::ContainsAny(block.body, active) ||
                    Engine::Logic::ContainsAny(block.elseBody, active))
                {
                    running.push_back(block.id);
                }
                self(self, block.body);
                self(self, block.elseBody);
            }
        };
        for (const Engine::Logic::Script& script : logic.scripts)
        {
            holders(holders, script.blocks);
            if (!script.blocks.empty() && Engine::Logic::ContainsAny(script.blocks, active))
            {
                running.push_back(script.blocks.front().id);
            }
        }
        std::sort(running.begin(), running.end());
        running.erase(std::unique(running.begin(), running.end()), running.end());
        return running;
    }
}
