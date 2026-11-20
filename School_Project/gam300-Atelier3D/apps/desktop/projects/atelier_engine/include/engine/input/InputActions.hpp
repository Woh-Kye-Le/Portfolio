/* !
@file    	    InputActions.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		29/09/2026
@brief
    Declares InputActions, the named actions (MoveForward = W, ...) loaded from
    input_actions.json so keys can be remapped without recompiling, plus the key
    name table.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Input
{
    // A key (KeyCode) or a mouse button (MouseButton) that can trigger an action
    struct InputBinding
    {
        enum class Device : std::uint8_t
        {
            Key,
            Mouse
        };

        Device device = Device::Key;
        int code = 0;

        bool operator==(const InputBinding&) const = default;
    };

    // Names used in input_actions.json: "W", "Space", "LeftShift", "F5", "NumPad0", "MouseLeft", "MouseRight", ...
    // Parsing ignores case. Returns nullopt for names that are not a key or mouse button InputSystem tracks.
    std::optional<InputBinding> ParseBinding(std::string_view name);
    // The name ParseBinding reads back, e.g. "LeftShift"; "?" for a code without a name
    std::string BindingName(const InputBinding& binding);
    // Every key and mouse button that has a name, in a stable order (letters, digits, F-keys, ..., mouse buttons)
    const std::vector<InputBinding>& AllBindings();

    // Actions the engine uses itself (the fly camera); games may add their own names in input_actions.json
    namespace Action
    {
        inline constexpr std::string_view MoveForward = "MoveForward";
        inline constexpr std::string_view MoveBack = "MoveBack";
        inline constexpr std::string_view MoveLeft = "MoveLeft";
        inline constexpr std::string_view MoveRight = "MoveRight";
        inline constexpr std::string_view MoveUp = "MoveUp";
        inline constexpr std::string_view MoveDown = "MoveDown";
        inline constexpr std::string_view MoveFast = "MoveFast";
        inline constexpr std::string_view Look = "Look";
    }

    // Named actions and the keys / mouse buttons bound to them, so keys can be remapped by editing a file:
    //   { "version": 1, "actions": { "MoveForward": ["W", "Up"], "Look": ["MouseRight"], "Jump": "Space" } }
    // An action may list several bindings (any of them triggers it), a single name, or [] for none.
    class InputActions
    {
    public:
        using Map = std::map<std::string, std::vector<InputBinding>, std::less<>>;

        static constexpr int kFormatVersion = 1;

        // The engine's own actions: WASD + Q/E to move, Shift to go faster, hold the right mouse button to look
        static InputActions Defaults();

        // Reads an input_actions.json file, starting from Defaults(). Never throws. Every problem is written to the
        // log and, when problems is given, added to it:
        //   - missing file (logged as a warning): the defaults are used
        //   - invalid JSON, wrong layout or unsupported version (logged as errors): the defaults are used
        //   - a default action missing from the file, or an entry that is not a key name / list of key names:
        //     that action keeps its default binding (logged as errors)
        //   - an unknown key name: that name is ignored; if none of the action's names are valid it keeps its
        //     default binding (logged as errors)
        static InputActions Load(const std::filesystem::path& path, std::vector<std::string>* problems = nullptr);

        // Writes the actions in the format Load reads, creating the folder if needed.
        // Throws std::runtime_error if the file cannot be written.
        void Save(const std::filesystem::path& path) const;

        // Replaces the bindings of an action (adding the action if it is new)
        void Bind(std::string_view action, std::vector<InputBinding> bindings);
        // Adds one binding to an action (adding the action if it is new); a binding it already has is not repeated
        void AddBinding(std::string_view action, InputBinding binding);
        // Removes one binding from an action; the action stays, possibly with no bindings
        void RemoveBinding(std::string_view action, InputBinding binding);
        void RemoveAction(std::string_view action);
        // The bindings of an action, or nullptr if there is no action with that name
        const std::vector<InputBinding>* Find(std::string_view action) const;
        const Map& All() const { return actions_; }

    private:
        Map actions_;
    };
}
