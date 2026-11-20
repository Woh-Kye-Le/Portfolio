/* !
@file    	    GameObjectValues.cpp
@date    		30/10/2026
@brief
    Implements ValuesOf by reading the JSON the scene file writes for an object.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/GameObjectValues.hpp"

#include "GameObjectJson.hpp"

namespace Engine::Scene
{
    ObjectValues ValuesOf(const GameObject& object)
    {
        ObjectValues values;
        const nlohmann::json node = Detail::ToJson(object);
        for (const auto& [component, fields] : node.items())
        {
            // Components are the objects in it; the block logic and the prefab link are objects too, but not components
            if (!fields.is_object() || component == "logic" || component == "prefab")
            {
                continue;
            }
            std::map<std::string, FieldValue>& into = values[component];
            for (const auto& [field, value] : fields.items())
            {
                if (value.is_boolean())
                {
                    into[field] = {.type = FieldValue::Type::Flag, .number = value.get<bool>() ? 1.0 : 0.0};
                }
                else if (value.is_number())
                {
                    into[field] = {.type = FieldValue::Type::Number, .number = value.get<double>()};
                }
                else if (value.is_string())
                {
                    into[field] = {.type = FieldValue::Type::Text, .text = value.get<std::string>()};
                }
            }
        }
        return values;
    }
}
