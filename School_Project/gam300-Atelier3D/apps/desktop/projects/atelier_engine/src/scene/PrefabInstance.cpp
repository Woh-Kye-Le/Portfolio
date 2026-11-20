/* !
@file    	    PrefabInstance.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Implements InstanceRootOf and InstanceMembers: which prefab instance a scene
    object belongs to, and the objects of an instance.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/PrefabInstance.hpp"

#include "engine/scene/Hierarchy.hpp"

namespace Engine::Scene
{
    std::uint32_t InstanceRootOf(const SceneDescription& scene, std::uint32_t objectId)
    {
        const GameObject* object = FindObject(scene, objectId);
        if (!object || !object->prefabInstance)
        {
            return 0;
        }
        const std::string& prefab = object->prefabInstance->prefab;
        for (std::size_t steps = 0; object && steps < scene.objects.size(); ++steps)
        {
            if (!object->prefabInstance || object->prefabInstance->prefab != prefab)
            {
                return 0; // a part whose root is gone (or was unpacked)
            }
            if (object->prefabInstance->IsRoot())
            {
                return object->id;
            }
            object = FindObject(scene, object->parent);
        }
        return 0;
    }

    std::vector<std::uint32_t> InstanceMembers(const SceneDescription& scene, std::uint32_t rootId)
    {
        std::vector<std::uint32_t> members;
        if (InstanceRootOf(scene, rootId) != rootId || rootId == 0)
        {
            return members;
        }
        members.push_back(rootId);
        for (const GameObject& object : scene.objects)
        {
            if (object.id != rootId && InstanceRootOf(scene, object.id) == rootId)
            {
                members.push_back(object.id);
            }
        }
        return members;
    }
}
