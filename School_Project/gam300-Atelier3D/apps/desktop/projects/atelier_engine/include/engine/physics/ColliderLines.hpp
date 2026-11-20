/* !
@file    	    ColliderLines.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		28/10/2026
@brief
    Declares OutlineOf, which turns a collider at a pose into the line
    segments of its wireframe (a box's edges, a sphere's circles, a capsule's
    rings, sides and rounded ends), for drawing colliders over the scene.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/physics/Body.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace Engine::Physics
{
    // A line segment in world space
    struct Line
    {
        glm::vec3 from{0.0f};
        glm::vec3 to{0.0f};
    };

    /*!
    \brief   The wireframe of a collider placed at a pose, the same shape the physics uses: a box's 12 edges; a
             sphere's three circles (around X, Y and Z); a capsule's ring at each end of its straight middle, four lines
             down its sides and the half circles over its rounded ends. A capsule with no straight middle is drawn as a
             sphere, as the physics makes it.
    \param   collider The collider, in world units, in the body's frame (its centre is moved by the pose too).
    \param   pose Where the body is and how it is turned.
    \param   segments The lines each full circle is made of (at least 8); half circles get half as many.
    \return  The segments, in world space.
    */
    std::vector<Line> OutlineOf(const Collider& collider, const BodyPose& pose, int segments = 24);
}
