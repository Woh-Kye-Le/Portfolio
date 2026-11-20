/* !
@file    	    Vertex.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Defines the Vertex struct representing a single mesh vertex. Stores vertex
    position, surface normal, texture coordinate, and per-vertex color data for
    use in mesh geometry and rendering pipelines.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <glm/glm.hpp>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Data structure holding per-vertex geometric and rendering attributes.
    *************************************************************************/
    struct Vertex
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec2 texCoord = glm::vec2(0.0f);
        glm::vec4 color = glm::vec4(1.0f);
        glm::vec4 joints = glm::vec4(0.0f);  // skinning: the four bones it follows (as numbers), into u_Bones
        glm::vec4 weights = glm::vec4(0.0f); // how much each bone moves it; all 0 for a mesh without a skeleton
    };
}
