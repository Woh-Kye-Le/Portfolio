/* !
@file    	    GameObjectValues.hpp
@date    		30/10/2026
@brief
    Declares ValuesOf: every saved value of a game object's components, by the
    names the scene file uses, so a tool can compare or show them without
    knowing which components exist.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/SceneDescription.hpp"

#include <map>
#include <string>

namespace Engine::Scene
{
    // One saved field: a number, a true / false, or text
    struct FieldValue
    {
        enum class Type : std::uint8_t
        {
            Number,
            Flag,
            Text,
        };

        Type type = Type::Number;
        double number = 0.0; // a number, or 0 / 1 for a flag
        std::string text;    // text

        bool operator==(const FieldValue&) const = default;
    };

    // component name ("transform", "renderable", ...) -> field name ("x", "rotation_speed", ...) -> value
    using ObjectValues = std::map<std::string, std::map<std::string, FieldValue>>;

    // The values of the object's components, taken from the scene file's own layout: a component or field that is
    // saved is listed without being added here. The block logic and the prefab link are not components and are left
    // out. Like the file, a field that still has its default may be missing (a plain white tint).
    ObjectValues ValuesOf(const GameObject& object);
}
