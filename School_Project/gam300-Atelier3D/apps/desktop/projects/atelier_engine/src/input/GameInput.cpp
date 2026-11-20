/* !
@file    	    GameInput.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		29/09/2026
@brief
    Implements GameInput: action down/pressed/released checks across all
    bindings and the blocked mouse delta and scroll.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/input/GameInput.hpp"

namespace Engine::Input
{
    bool GameInput::IsActionDown(std::string_view action) const
    {
        return Check(action, Query::Down);
    }

    bool GameInput::IsActionPressed(std::string_view action) const
    {
        return Check(action, Query::Pressed);
    }

    bool GameInput::IsActionReleased(std::string_view action) const
    {
        return Check(action, Query::Released);
    }

    // Mouse movement/scroll count as nothing while the editor UI has the mouse,
    // so e.g. scrolling the Console doesn't zoom the game camera
    Vec2 GameInput::GetMouseDelta() const
    {
        return IsMouseBlocked() ? Vec2{} : input_.GetMouseDelta();
    }

    float GameInput::GetScrollDelta() const
    {
        return IsMouseBlocked() ? 0.0f : input_.GetScrollDelta();
    }

    // An action can have several bindings (e.g. W and Up), so it counts if any one of them matches
    bool GameInput::Check(std::string_view action, Query query) const
    {
        const auto* bindings = actions_.Find(action);
        if (!bindings)
        {
            return false;
        }

        for (const InputBinding& binding : *bindings)
        {
            const bool isKey = binding.device == InputBinding::Device::Key;
            if (isKey ? IsKeyboardBlocked() : IsMouseBlocked())
            {
                continue; // the editor UI has this device
            }

            bool result = false;
            switch (query)
            {
            case Query::Down:
                result = isKey ? input_.GetKey(binding.code) == KeyState::Down : input_.IsMouseButtonDown(binding.code);
                break;
            case Query::Pressed:
                result = isKey ? input_.IsKeyPressed(binding.code) : input_.IsMouseButtonPressed(binding.code);
                break;
            case Query::Released:
                result = isKey ? input_.IsKeyReleased(binding.code) : input_.IsMouseButtonReleased(binding.code);
                break;
            }
            if (result)
            {
                return true;
            }
        }
        return false;
    }
}
