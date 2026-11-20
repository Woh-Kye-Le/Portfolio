/* !
@file    	    UndoRedoManager.cpp
@author  	    sanni.go@digipen.edu [100%]
@date    		25/10/2026
@brief
    Implements UndoRedoManager: recording commands with a size limit, undoing
    and redoing them.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/history/UndoRedoManager.hpp"

#include <algorithm>
#include <utility>

namespace Engine::History
{
    UndoRedoManager::UndoRedoManager(std::size_t capacity) : capacity_(std::max<std::size_t>(capacity, 1)) {}

    void UndoRedoManager::Execute(std::unique_ptr<ICommand> command)
    {
        if (!command)
        {
            return;
        }
        command->Execute();
        Push(std::move(command));
    }

    void UndoRedoManager::Push(std::unique_ptr<ICommand> command)
    {
        if (!command)
        {
            return;
        }
        undo_.push_back(std::move(command));
        while (undo_.size() > capacity_)
        {
            undo_.pop_front();
        }
        redo_.clear();
    }

    bool UndoRedoManager::Undo()
    {
        if (undo_.empty())
        {
            return false;
        }
        std::unique_ptr<ICommand> command = std::move(undo_.back());
        undo_.pop_back();
        command->Undo();
        redo_.push_back(std::move(command));
        return true;
    }

    bool UndoRedoManager::Redo()
    {
        if (redo_.empty())
        {
            return false;
        }
        std::unique_ptr<ICommand> command = std::move(redo_.back());
        redo_.pop_back();
        command->Execute();
        undo_.push_back(std::move(command));
        return true;
    }

    void UndoRedoManager::Clear()
    {
        undo_.clear();
        redo_.clear();
    }
}
