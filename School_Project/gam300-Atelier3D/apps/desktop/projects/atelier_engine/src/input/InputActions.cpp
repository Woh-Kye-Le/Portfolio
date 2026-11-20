/* !
@file    	    InputActions.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		29/09/2026
@brief
    Implements InputActions: the key/mouse name table, the default bindings, and
    loading (with problem reports and defaults) and saving input_actions.json.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/input/InputActions.hpp"

#include "engine/core/Log.hpp"
#include "engine/input/Input.hpp"
#include "engine/serializer/JsonSerializer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <format>
#include <iterator>
#include <utility>

namespace Engine::Input
{
    namespace
    {
        struct NamedBinding
        {
            std::string_view name;
            InputBinding binding;
        };

        // short names so the big table below stays readable: K = keyboard key, M = mouse button
        constexpr InputBinding K(int code)
        {
            return {InputBinding::Device::Key, code};
        }

        constexpr InputBinding M(int code)
        {
            return {InputBinding::Device::Mouse, code};
        }

        // Every key and mouse button InputSystem tracks (see KeyCode / MouseButton), with its file name.
        // To support a new key in input_actions.json, add it here.
        constexpr std::array kNames{
            NamedBinding{"A", K(Key_A)},
            NamedBinding{"B", K(Key_B)},
            NamedBinding{"C", K(Key_C)},
            NamedBinding{"D", K(Key_D)},
            NamedBinding{"E", K(Key_E)},
            NamedBinding{"F", K(Key_F)},
            NamedBinding{"G", K(Key_G)},
            NamedBinding{"H", K(Key_H)},
            NamedBinding{"I", K(Key_I)},
            NamedBinding{"J", K(Key_J)},
            NamedBinding{"K", K(Key_K)},
            NamedBinding{"L", K(Key_L)},
            NamedBinding{"M", K(Key_M)},
            NamedBinding{"N", K(Key_N)},
            NamedBinding{"O", K(Key_O)},
            NamedBinding{"P", K(Key_P)},
            NamedBinding{"Q", K(Key_Q)},
            NamedBinding{"R", K(Key_R)},
            NamedBinding{"S", K(Key_S)},
            NamedBinding{"T", K(Key_T)},
            NamedBinding{"U", K(Key_U)},
            NamedBinding{"V", K(Key_V)},
            NamedBinding{"W", K(Key_W)},
            NamedBinding{"X", K(Key_X)},
            NamedBinding{"Y", K(Key_Y)},
            NamedBinding{"Z", K(Key_Z)},

            NamedBinding{"0", K(Key_0)},
            NamedBinding{"1", K(Key_1)},
            NamedBinding{"2", K(Key_2)},
            NamedBinding{"3", K(Key_3)},
            NamedBinding{"4", K(Key_4)},
            NamedBinding{"5", K(Key_5)},
            NamedBinding{"6", K(Key_6)},
            NamedBinding{"7", K(Key_7)},
            NamedBinding{"8", K(Key_8)},
            NamedBinding{"9", K(Key_9)},

            NamedBinding{"F1", K(Key_F1)},
            NamedBinding{"F2", K(Key_F2)},
            NamedBinding{"F3", K(Key_F3)},
            NamedBinding{"F4", K(Key_F4)},
            NamedBinding{"F5", K(Key_F5)},
            NamedBinding{"F6", K(Key_F6)},
            NamedBinding{"F7", K(Key_F7)},
            NamedBinding{"F8", K(Key_F8)},
            NamedBinding{"F9", K(Key_F9)},
            NamedBinding{"F10", K(Key_F10)},
            NamedBinding{"F11", K(Key_F11)},
            NamedBinding{"F12", K(Key_F12)},

            NamedBinding{"Space", K(Key_Space)},
            NamedBinding{"Enter", K(Key_Enter)},
            NamedBinding{"Escape", K(Key_Escape)},
            NamedBinding{"Tab", K(Key_Tab)},
            NamedBinding{"Backspace", K(Key_Backspace)},
            NamedBinding{"Delete", K(Key_Delete)},
            NamedBinding{"Insert", K(Key_Insert)},
            NamedBinding{"Home", K(Key_Home)},
            NamedBinding{"End", K(Key_End)},
            NamedBinding{"PageUp", K(Key_PageUp)},
            NamedBinding{"PageDown", K(Key_PageDown)},

            NamedBinding{"Up", K(Key_Up)},
            NamedBinding{"Down", K(Key_Down)},
            NamedBinding{"Left", K(Key_Left)},
            NamedBinding{"Right", K(Key_Right)},

            NamedBinding{"NumPad0", K(Key_NumPad0)},
            NamedBinding{"NumPad1", K(Key_NumPad1)},
            NamedBinding{"NumPad2", K(Key_NumPad2)},
            NamedBinding{"NumPad3", K(Key_NumPad3)},
            NamedBinding{"NumPad4", K(Key_NumPad4)},
            NamedBinding{"NumPad5", K(Key_NumPad5)},
            NamedBinding{"NumPad6", K(Key_NumPad6)},
            NamedBinding{"NumPad7", K(Key_NumPad7)},
            NamedBinding{"NumPad8", K(Key_NumPad8)},
            NamedBinding{"NumPad9", K(Key_NumPad9)},
            NamedBinding{"NumPadEnter", K(Key_NumPadEnter)},
            NamedBinding{"NumPadAdd", K(Key_NumPadAdd)},
            NamedBinding{"NumPadSubtract", K(Key_NumPadSubtract)},
            NamedBinding{"NumPadMultiply", K(Key_NumPadMultiply)},
            NamedBinding{"NumPadDivide", K(Key_NumPadDivide)},

            NamedBinding{"Comma", K(Key_Comma)},
            NamedBinding{"Period", K(Key_Period)},
            NamedBinding{"Slash", K(Key_Slash)},
            NamedBinding{"Semicolon", K(Key_Semicolon)},
            NamedBinding{"Apostrophe", K(Key_Apostrophe)},
            NamedBinding{"LeftBracket", K(Key_LeftBracket)},
            NamedBinding{"RightBracket", K(Key_RightBracket)},
            NamedBinding{"Backslash", K(Key_Backslash)},
            NamedBinding{"Minus", K(Key_Minus)},
            NamedBinding{"Equal", K(Key_Equal)},
            NamedBinding{"GraveAccent", K(Key_GraveAccent)},

            NamedBinding{"CapsLock", K(Key_CapsLock)},
            NamedBinding{"NumLock", K(Key_NumLock)},
            NamedBinding{"ScrollLock", K(Key_ScrollLock)},

            NamedBinding{"LeftCtrl", K(Key_LeftCtrl)},
            NamedBinding{"RightCtrl", K(Key_RightCtrl)},
            NamedBinding{"LeftShift", K(Key_LeftShift)},
            NamedBinding{"RightShift", K(Key_RightShift)},
            NamedBinding{"LeftAlt", K(Key_LeftAlt)},
            NamedBinding{"RightAlt", K(Key_RightAlt)},

            NamedBinding{"MouseLeft", M(Mouse_Left)},
            NamedBinding{"MouseRight", M(Mouse_Right)},
            NamedBinding{"MouseMiddle", M(Mouse_Middle)},
        };

        bool EqualsIgnoringCase(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                                      [](char x, char y) {
                                                          return std::tolower(static_cast<unsigned char>(x)) ==
                                                                 std::tolower(static_cast<unsigned char>(y));
                                                      });
        }

        std::string Describe(const std::vector<InputBinding>* bindings)
        {
            if (!bindings || bindings->empty())
            {
                return "nothing";
            }
            std::string text;
            for (const InputBinding& binding : *bindings)
            {
                text += (text.empty() ? "" : ", ") + BindingName(binding);
            }
            return text;
        }

        // Fills actions from the file; returns false when the whole file has to be ignored
        bool ReadActions(const std::filesystem::path& path, InputActions& actions, std::vector<std::string>& problems,
                         bool& fileMissing)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
            {
                fileMissing = true;
                problems.emplace_back("file not found - using the default bindings");
                return false;
            }

            Serializer::JSONSerializer json;
            try
            {
                json.LoadFile(path.string());
            }
            catch (const std::exception& e)
            {
                problems.push_back(std::string("not valid JSON (") + e.what() + ") - using the default bindings");
                return false;
            }

            const nlohmann::json& root = json.GetCurrentNode();
            if (!root.is_object())
            {
                problems.emplace_back("the file must contain a JSON object: { \"version\": 1, \"actions\": { ... } } - "
                                      "using the default bindings");
                return false;
            }

            // A missing or weird version is only a warning (old files didn't have one), but a version that's
            // too new means the file was made by a newer build, so we don't trust it at all
            if (!root.contains("version"))
            {
                problems.emplace_back("missing \"version\" - reading it as version 1");
            }
            else if (!root.at("version").is_number_integer())
            {
                problems.emplace_back("\"version\" is not a whole number - reading it as version 1");
            }
            else if (const auto version = root.at("version").get<std::int64_t>();
                     version < 1 || version > InputActions::kFormatVersion)
            {
                problems.push_back(std::format("unsupported format version {} (this build reads version {}) - using "
                                               "the default bindings",
                                               version, InputActions::kFormatVersion));
                return false;
            }

            if (!root.contains("actions") || !root.at("actions").is_object())
            {
                problems.emplace_back("missing the \"actions\" object: \"actions\": { \"MoveForward\": [\"W\"] } - "
                                      "using the default bindings");
                return false;
            }

            // actions already holds the defaults, so anything the file leaves out just keeps its default key.
            // We still report it, in case it was deleted by accident.
            const nlohmann::json& entries = root.at("actions");
            for (const auto& [action, _] : actions.All())
            {
                if (!entries.contains(action))
                {
                    problems.push_back(
                        std::format("missing action \"{}\" - using {}", action, Describe(actions.Find(action))));
                }
            }

            for (const auto& [action, value] : entries.items())
            {
                const auto keepDefault = [&actions, &problems, name = std::string(action)](const std::string& why)
                {
                    const auto* defaults = actions.Find(name);
                    problems.push_back(std::format("action \"{}\": {} - {}", name, why,
                                                   defaults ? "using " + Describe(defaults) : "left unbound"));
                    if (!defaults)
                    {
                        actions.Bind(name, {});
                    }
                };

                // Accept "W" as well as ["W", "Up"]
                std::vector<std::string> names;
                if (value.is_string())
                {
                    names.push_back(value.get<std::string>());
                }
                else if (value.is_array() &&
                         std::all_of(value.begin(), value.end(), [](const auto& item) { return item.is_string(); }))
                {
                    std::transform(value.begin(), value.end(), std::back_inserter(names),
                                   [](const auto& item) { return item.template get<std::string>(); });
                }
                else
                {
                    keepDefault("must be a key name or a list of key names");
                    continue;
                }

                std::vector<InputBinding> bindings;
                for (const std::string& name : names)
                {
                    if (const auto binding = ParseBinding(name))
                    {
                        if (std::find(bindings.begin(), bindings.end(), *binding) == bindings.end())
                        {
                            bindings.push_back(*binding);
                        }
                    }
                    else
                    {
                        problems.push_back(std::format("action \"{}\": unknown key \"{}\" - ignored", action, name));
                    }
                }

                if (bindings.empty() && !names.empty())
                {
                    keepDefault("no valid keys");
                    continue;
                }
                actions.Bind(action, std::move(bindings)); // [] deliberately leaves the action unbound
            }
            return true;
        }
    }

    // "W" / "mouseright" -> binding (case doesn't matter)
    std::optional<InputBinding> ParseBinding(std::string_view name)
    {
        const auto found = std::find_if(kNames.begin(), kNames.end(), [name](const NamedBinding& entry)
                                        { return EqualsIgnoringCase(entry.name, name); });
        if (found == kNames.end())
        {
            return std::nullopt;
        }
        return found->binding;
    }

    // The other way round, for saving the file and showing keys in the Controls panel
    std::string BindingName(const InputBinding& binding)
    {
        const auto found = std::find_if(kNames.begin(), kNames.end(),
                                        [&binding](const NamedBinding& entry) { return entry.binding == binding; });
        if (found == kNames.end())
        {
            return "?";
        }
        return std::string(found->name);
    }

    // Every key you can bind - fills the "+" list in the Controls panel
    const std::vector<InputBinding>& AllBindings()
    {
        static const std::vector<InputBinding> all = []
        {
            std::vector<InputBinding> bindings;
            bindings.reserve(kNames.size());
            std::transform(kNames.begin(), kNames.end(), std::back_inserter(bindings),
                           [](const NamedBinding& entry) { return entry.binding; });
            return bindings;
        }();
        return all;
    }

    // The bindings new games get (and what we fall back to when the file is broken)
    InputActions InputActions::Defaults()
    {
        InputActions actions;
        actions.Bind(Action::MoveForward, {K(Key_W)});
        actions.Bind(Action::MoveBack, {K(Key_S)});
        actions.Bind(Action::MoveLeft, {K(Key_A)});
        actions.Bind(Action::MoveRight, {K(Key_D)});
        actions.Bind(Action::MoveDown, {K(Key_Q)});
        actions.Bind(Action::MoveUp, {K(Key_E)});
        actions.Bind(Action::MoveFast, {K(Key_LeftShift), K(Key_RightShift)});
        actions.Bind(Action::Look, {M(Mouse_Right)});
        return actions;
    }

    // Never fails: anything wrong with the file is reported and the defaults fill the gaps
    InputActions InputActions::Load(const std::filesystem::path& path, std::vector<std::string>* problems)
    {
        InputActions actions = Defaults();
        std::vector<std::string> found;
        bool fileMissing = false;
        if (!ReadActions(path, actions, found, fileMissing))
        {
            actions = Defaults(); // a file that cannot be used changes nothing
        }

        // A missing file is normal for an old game, so that's just a warning; a broken file is an error
        for (const std::string& problem : found)
        {
            if (fileMissing)
            {
                LOG_WARN(Input, "Input actions {}: {}", path.string(), problem);
            }
            else
            {
                LOG_ERROR(Input, "Input actions {}: {}", path.string(), problem);
            }
        }
        if (problems)
        {
            *problems = std::move(found);
        }
        return actions;
    }

    // Writes { "version": 1, "actions": { "MoveForward": ["W"], ... } }
    void InputActions::Save(const std::filesystem::path& path) const
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        Serializer::JSONSerializer json;
        json.NewFile(path.string());
        json.WriteInt("version", kFormatVersion);
        json.CreateObject("actions");
        json.EnterObject("actions");
        for (const auto& [action, bindings] : actions_)
        {
            json.CreateArray(action);
            json.EnterArray(action);
            for (const InputBinding& binding : bindings)
            {
                json.WriteString(BindingName(binding)); // appends to the array
            }
            json.LeaveArray();
        }
        json.SaveFile(path.string());
    }

    // Replaces all of an action's keys (adds the action if it's new)
    void InputActions::Bind(std::string_view action, std::vector<InputBinding> bindings)
    {
        const auto existing = actions_.find(action);
        if (existing != actions_.end())
        {
            existing->second = std::move(bindings);
        }
        else
        {
            actions_.emplace(std::string(action), std::move(bindings));
        }
    }

    void InputActions::AddBinding(std::string_view action, InputBinding binding)
    {
        auto found = actions_.find(action);
        if (found == actions_.end())
        {
            found = actions_.emplace(std::string(action), std::vector<InputBinding>{}).first;
        }
        if (std::find(found->second.begin(), found->second.end(), binding) == found->second.end())
        {
            found->second.push_back(binding);
        }
    }

    void InputActions::RemoveBinding(std::string_view action, InputBinding binding)
    {
        const auto found = actions_.find(action);
        if (found != actions_.end())
        {
            std::erase(found->second, binding);
        }
    }

    void InputActions::RemoveAction(std::string_view action)
    {
        const auto found = actions_.find(action);
        if (found != actions_.end())
        {
            actions_.erase(found);
        }
    }

    const std::vector<InputBinding>* InputActions::Find(std::string_view action) const
    {
        const auto found = actions_.find(action);
        return found != actions_.end() ? &found->second : nullptr;
    }
}
