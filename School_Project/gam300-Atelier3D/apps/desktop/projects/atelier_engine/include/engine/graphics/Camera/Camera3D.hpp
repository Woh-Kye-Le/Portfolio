/* !
@file    	    Camera3D.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares the Camera3D class for 3D perspective rendering. Implements the
    Camera interface with a perspective projection, supporting position
    translation and orientation control via forward/look-at vectors.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Graphics Headers
#include "engine/graphics/Camera/Camera.hpp"

// Standard Library Headers
#include <glm/glm.hpp>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          3D perspective camera with position, orientation, and look-at controls.
    *************************************************************************/
    class Camera3D : public Camera
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor initializing a standard perspective projection.
        *************************************************************************/
        Camera3D();

        /*!***********************************************************************
        @brief          Configures the perspective projection with explicit frustum parameters.
        @param          fovYDegrees   Vertical field-of-view angle in degrees.
        @param          aspectRatio   Width-to-height aspect ratio of the viewport.
        @param          zNear         Distance to the near clipping plane.
        @param          zFar          Distance to the far clipping plane.
        *************************************************************************/
        void SetPerspective(float fovYDegrees, float aspectRatio, float zNear, float zFar);

        /*!***********************************************************************
        @brief          Sets the world-space position of the camera.
        @param          position      New camera position vector.
        *************************************************************************/
        void SetPosition(const glm::vec3& position);

        /*!***********************************************************************
        @brief          Sets the camera orientation using a forward direction vector.
        @param          forward       Direction vector the camera should face.
        @param          worldUp       World-space up vector for orientation reference.
        *************************************************************************/
        void SetOrientation(const glm::vec3& forward, const glm::vec3& worldUp = glm::vec3(0.0f, 1.0f, 0.0f));

        /*!***********************************************************************
        @brief          Orients the camera to look at a specified target point.
        @param          target        World-space point the camera should face toward.
        @param          worldUp       World-space up vector for orientation reference.
        *************************************************************************/
        void SetLookAt(const glm::vec3& target, const glm::vec3& worldUp = glm::vec3(0.0f, 1.0f, 0.0f));

        /*!***********************************************************************
        @brief          Retrieves the current view transformation matrix.
        @return         Const reference to the 4x4 view matrix.
        *************************************************************************/
        const glm::mat4& GetViewMatrix() const override;

        /*!***********************************************************************
        @brief          Retrieves the current perspective projection matrix.
        @return         Const reference to the 4x4 projection matrix.
        *************************************************************************/
        const glm::mat4& GetProjectionMatrix() const override;

        /*!***********************************************************************
        @brief          Retrieves the combined view-projection matrix.
        @return         Const reference to the 4x4 view-projection matrix.
        *************************************************************************/
        const glm::mat4& GetViewProjectionMatrix() const override;

        /*!***********************************************************************
        @brief          Retrieves the world-space position of the camera.
        @return         Const reference to the camera position vector.
        *************************************************************************/
        const glm::vec3& GetPosition() const override;

        /*!***********************************************************************
        @brief          Retrieves the normalized forward direction vector of the camera.
        @return         Const reference to the forward direction vector.
        *************************************************************************/
        const glm::vec3& GetForward() const;

        /*!***********************************************************************
        @brief          Retrieves the normalized local up direction vector of the camera.
        @return         Const reference to the up direction vector.
        *************************************************************************/
        const glm::vec3& GetUp() const;

        /*!***********************************************************************
        @brief          Retrieves the normalized local right direction vector of the camera.
        @return         Const reference to the right direction vector.
        *************************************************************************/
        const glm::vec3& GetRight() const;

    private:
        /*!***********************************************************************
        @brief          Recomputes the view matrix from current position and orientation.
        *************************************************************************/
        void RecalculateView();

        /*!***********************************************************************
        @brief          Recomputes the combined view-projection matrix.
        *************************************************************************/
        void RecalculateCombined();

        glm::mat4 viewMatrix_ = glm::mat4(1.0f);
        glm::mat4 projectionMatrix_ = glm::mat4(1.0f);
        glm::mat4 viewProjectionMatrix_ = glm::mat4(1.0f);

        glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 3.0f);
        glm::vec3 forward_ = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right_ = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 worldUp_ = glm::vec3(0.0f, 1.0f, 0.0f);
    };
}
