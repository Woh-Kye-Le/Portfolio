/* !
@file    	    UndoRedoManager.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		25/10/2026
@brief
    Declares UndoRedoManager, the undo and redo stacks of ICommands. It keeps
    the last 100 operations by default; older ones are dropped.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#pragma once

#include "engine/history/ICommand.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

namespace Engine::History
{
    // Every new operation goes on the undo stack and clears the redo stack
    class UndoRedoManager
    {
    public:
        static constexpr std::size_t kDefaultCapacity = 100;

        explicit UndoRedoManager(std::size_t capacity = kDefaultCapacity);

        // Runs the command then records it
        void Execute(std::unique_ptr<ICommand> command);
        // Records a command whose change has already been made
        void Push(std::unique_ptr<ICommand> command);

        // Return false if nothing to undo/redo
        bool Undo();
        bool Redo();
        void Clear();

        bool CanUndo() const { return !undo_.empty(); }
        bool CanRedo() const { return !redo_.empty(); }
        std::size_t UndoCount() const { return undo_.size(); }
        std::size_t RedoCount() const { return redo_.size(); }
        std::size_t Capacity() const { return capacity_; }
        // The command Undo/Redo would run next
        const ICommand* NextUndo() const { return undo_.empty() ? nullptr : undo_.back().get(); }
        const ICommand* NextRedo() const { return redo_.empty() ? nullptr : redo_.back().get(); }

    private:
        std::deque<std::unique_ptr<ICommand>> undo_; // newest at the back, oldest dropped from the front
        std::vector<std::unique_ptr<ICommand>> redo_;
        std::size_t capacity_;
    };
}
