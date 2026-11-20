/* !
@file    	    DiagnosticsPanel.hpp
@author  	    sanni.go@digipen.edu [80%]
                c.kojat@digipen.edu [20%]
@date    		28/09/2026
@brief
    Declares DiagnosticsPanel, the editor's Diagnostics window showing the
    editor controls, the editor camera and the live state of the input system.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"
#include "engine/messenger/EventBus.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace Engine::Input
{
    class InputSystem;
}

namespace Editor::Camera
{
    class EditorCamera;
}

namespace Editor::Panels
{
    // Editor window for trying the engine's core systems by hand:
    //   Editor input - the shortcuts and fly camera that use the InputSystem, with live camera values
    //   Input        - everything InputSystem reports: keys, pressed / released, modifiers, mouse, scroll, text
    //   Game events  - what the running game sends through the EventBus (the events block logic will use)
    // (Games are picked, played and built from the editor's menu bar; scenes are edited in Hierarchy / Inspector,
    // controls in the Controls window and sounds tried in the Audio window.)
    // DrawContents() must run inside Application::OnUpdate, before the InputSystem advances to the next frame.
    class DiagnosticsPanel final : public Panel
    {
    public:
        DiagnosticsPanel(Engine::Input::InputSystem& input, const Camera::EditorCamera& camera);

        void DrawContents() override;
        // Notes this frame's key / mouse events for the Input log; call every frame, even while the panel is hidden
        void RecordInputEvents();
        // Starts logging the game events sent on events (the Game events section). Call once, right after creating
        // the panel: it keeps logging while hidden, and stops when the panel is destroyed.
        void ConnectEvents(Engine::Core::EventBus& events);

    private:
        void DrawEditorInput();
        void DrawInput();
        void DrawGameEvents();

        Engine::Input::InputSystem& input_;
        const Camera::EditorCamera& camera_;

        // Input
        std::deque<std::string> inputLog_;
        std::string typedText_;

        // Game events
        std::deque<std::string> eventLog_;
        std::uint64_t gameUpdates_ = 0; // GameUpdate events since the last GameStarted
        // Last, so the handlers are unsubscribed before the log they write to is destroyed
        std::vector<Engine::Core::Subscription> subscriptions_;
    };
}
