/* !
@file    	    HierarchySystem.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Declares HierarchySystem, which places child entities relative to their
    parents.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Systems
{
    // Sets the Transform of every entity with a HierarchyComponent to its parent's Transform composed with its local
    // one, parents before their children, so a child follows a parent that moved or spun this frame. A parent that is
    // gone or has no Transform places the child as if it were at the origin. Runs every frame, after the systems that
    // move things and before anything reads where entities are (camera, audio, rendering).
    class HierarchySystem
    {
    public:
        static void Update(Scene::Scene& scene);
    };
}
