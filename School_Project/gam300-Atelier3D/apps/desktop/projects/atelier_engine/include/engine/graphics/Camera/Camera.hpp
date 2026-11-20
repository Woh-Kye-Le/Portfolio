/* !
@file    	    Camera.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Defines the abstract base Camera class for the graphics system. Provides a
    common interface for accessing view, projection, and combined view-
    projection matrices along with camera position for any camera type.

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
    @brief          Abstract base class defining the camera interface for all camera types.
    *************************************************************************/
    class Camera
    {
    public:
        /*!***********************************************************************
        @brief          Virtual destructor for proper polymorphic cleanup.
        *************************************************************************/
        virtual ~Camera() = default;

        /*!***********************************************************************
        @brief          Retrieves the current view transformation matrix.
        @return         Const reference to the 4x4 view matrix.
        *************************************************************************/
        virtual const glm::mat4& GetViewMatrix() const = 0;

        /*!***********************************************************************
        @brief          Retrieves the current projection transformation matrix.
        @return         Const reference to the 4x4 projection matrix.
        *************************************************************************/
        virtual const glm::mat4& GetProjectionMatrix() const = 0;

        /*!***********************************************************************
        @brief          Retrieves the combined view-projection matrix.
        @return         Const reference to the 4x4 view-projection matrix.
        *************************************************************************/
        virtual const glm::mat4& GetViewProjectionMatrix() const = 0;

        /*!***********************************************************************
        @brief          Retrieves the world-space position of the camera.
        @return         Const reference to the camera position vector.
        *************************************************************************/
        virtual const glm::vec3& GetPosition() const = 0;
    };
}
