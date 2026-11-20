/* !
@file    	    ICommand.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		25/10/2026
@brief
    Declares ICommand, one undoable editor operation (command pattern). The
    UndoRedoManager keeps them on its undo and redo stacks.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <string>

namespace Engine::History
{
    class ICommand
    {
    public:
        virtual ~ICommand() = default;

        // Does the operation, Redo calls it again after an Undo
        virtual void Execute() = 0;
        // Puts things back the way they were before Execute
        virtual void Undo() = 0;
        // Short label for menus and the status bar
        virtual std::string Name() const = 0;
    };
}
