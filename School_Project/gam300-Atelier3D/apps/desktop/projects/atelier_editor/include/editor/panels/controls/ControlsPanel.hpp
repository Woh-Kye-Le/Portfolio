/* !
@file    	    ControlsPanel.hpp
@author  	    beh.s@digipen.edu [80%]
                sanni.go@digipen.edu [20%]
@date    		30/09/2026
@brief
    Declares ControlsPanel, the editor window for viewing and remapping the
    game's named input actions stored in configs/input_actions.json.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"

#include "engine/input/InputActions.hpp"

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Engine::Input
{
    class InputSystem;
}

namespace Editor::Panels
{
    // Edits the selected game's configs/input_actions.json: which keys trigger each action. Click a key to remove
    // it, + adds one from a list, Press key binds the next key pressed; new actions can be added for game code.
    // DrawContents() must run before the InputSystem advances to the next frame (it reads the key that was just
    // pressed).
    class ControlsPanel final : public Panel
    {
    public:
        // selectedGame: the game whose controls are shown (empty path if none)
        // onSaved: called after the file is saved (the editor reloads the game so the new keys work straight away)
        ControlsPanel(Engine::Input::InputSystem& input, std::function<std::filesystem::path()> selectedGame,
                      std::function<void()> onSaved);

        void DrawContents() override;

        // True while waiting for a key to bind; the editor then keeps keys away from its shortcuts and the game
        bool IsListeningForKey() const { return !listeningAction_.empty(); }
        // Stops waiting for a key (e.g. when the panel is hidden, so it cannot hold on to the keyboard)
        void CancelListening() { listeningAction_.clear(); }

    private:
        std::filesystem::path SelectedGame() const;
        void LoadControls();
        void SaveControls();

        Engine::Input::InputSystem& input_;
        std::function<std::filesystem::path()> selectedGame_;
        std::function<void()> onGameSaved_;

        std::filesystem::path controlsGameDir_;
        Engine::Input::InputActions controls_;
        bool controlsChanged_ = false;
        std::string controlsMessage_;
        bool controlsFailed_ = false;
        std::vector<std::string> controlsWarnings_; // problems in the file when it was loaded
        std::string listeningAction_;               // action waiting for a key press (empty if none)
        std::array<char, 64> newActionName_{};
    };
}
