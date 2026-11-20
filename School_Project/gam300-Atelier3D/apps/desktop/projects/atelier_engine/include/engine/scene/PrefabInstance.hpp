/* !
@file    	    PrefabInstance.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Declares PrefabInstance, the component that links a scene object to the
    prefab object it was made from, with the properties changed on it, and the
    helpers that find which instance an object belongs to.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Scene
{
    struct SceneDescription;

    // Every object of a prefab's root in a prefab file has this id; the others follow it (see Prefab.hpp)
    inline constexpr std::uint32_t kPrefabRootId = 1;

    // Marks a scene object as part of an instance of a prefab: which prefab file, and which object in it this one is
    // a copy of. Each object of an instance has one (the root and every part under it), so the editor can tell the
    // parts apart and Game::PrefabManager can bring each one up to date when the prefab changes.
    //
    // The object still holds all of its own data, so a scene plays the same without the prefab file. overrides lists
    // the properties set on this object only, which an update from the prefab leaves alone. Each is a path in the
    // object's scene file layout: "health/health" (one field), "light" (a whole component added or removed) or
    // "name". The root's name and Transform belong to each instance (where it is placed) and are never listed.
    struct PrefabInstance
    {
        std::string prefab;                 // the prefab file, relative to the game folder
        std::string guid;                   // the prefab's guid: finds it again if its file moved
        std::uint32_t object = 0;           // id of the matching object inside the prefab
        std::vector<std::string> overrides; // sorted

        bool IsRoot() const { return object == kPrefabRootId; }

        bool operator==(const PrefabInstance&) const = default;
    };

    // The root of the prefab instance the object belongs to, or 0 if it is not part of one. Climbs the parents while
    // they are linked to the same prefab.
    std::uint32_t InstanceRootOf(const SceneDescription& scene, std::uint32_t objectId);

    // The objects that make up the instance whose root is rootId: the root first, then its parts in scene order.
    // Objects added under it in the scene that are not from the prefab are not included.
    std::vector<std::uint32_t> InstanceMembers(const SceneDescription& scene, std::uint32_t rootId);
}
