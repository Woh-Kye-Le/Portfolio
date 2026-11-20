/* !
@file    	    Entity.hpp
@author  	    sung.h@digipen.edu [30%]
         	    c.kojat@digipen.edu [70%]
@date    		22/09/2026
@brief
    Declares Entity, a lightweight handle (unique id) to an entity in a Scene.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>

namespace Engine::Scene
{
    class Scene;

    // A handle to an entity in a Scene. It's just an id (0 = no entity), so it's cheap to copy around.
    // Only Scene can make real ones, which keeps random ids from being passed in.
    class Entity
    {
    public:
        using Id = std::uint32_t;

        Entity() = default;

        //explicit Entity(Id id = 0) : id_(id) {}

        Id GetId() const { return id_; }
        explicit operator bool() const { return id_ != 0; }

        bool operator==(const Entity&) const = default;

    private:
        friend class Scene;

        Entity(Id id, const Scene* owner) : id_(id), owner_(owner) {}

        Id id_ = 0;
        const Scene* owner_ = nullptr;
    };
}
