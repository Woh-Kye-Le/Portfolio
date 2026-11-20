/* !
@file    	    Camera3D.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Contains the implementation of the Camera3D class methods. Implements
    perspective projection setup, position translation, orientation via
    forward/look-at vectors, and view-projection matrix recalculation.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Camera/Camera3D.hpp"

// Standard Library Headers
#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Default constructor initializing a standard 60-degree FOV perspective camera.
    *************************************************************************/
    Camera3D::Camera3D()
    {
        SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        RecalculateView();
        RecalculateCombined();
    }

    /*!***********************************************************************
    @brief          Configures the perspective projection frustum parameters.
    @param          fovYDegrees   Vertical field-of-view angle in degrees.
    @param          aspectRatio   Width-to-height aspect ratio of the viewport.
    @param          zNear         Distance to the near clipping plane.
    @param          zFar          Distance to the far clipping plane.
    *************************************************************************/
    void Camera3D::SetPerspective(float fovYDegrees, float aspectRatio, float zNear, float zFar)
    {
        projectionMatrix_ = glm::perspective(glm::radians(fovYDegrees), aspectRatio, zNear, zFar);
        RecalculateCombined();
    }

    /*!***********************************************************************
    @brief          Updates the camera world-space position and refreshes matrices.
    @param          position      New camera position vector.
    *************************************************************************/
    void Camera3D::SetPosition(const glm::vec3& position)
    {
        position_ = position;
        RecalculateView();
        RecalculateCombined();
    }

    /*!***********************************************************************
    @brief          Sets camera orientation from a forward direction and world up vector.
    @param          forward       Direction vector the camera should face.
    @param          worldUp       World-space up vector for orientation reference.
    *************************************************************************/
    void Camera3D::SetOrientation(const glm::vec3& forward, const glm::vec3& worldUp)
    {
        forward_ = glm::normalize(forward);
        worldUp_ = worldUp;
        right_ = glm::normalize(glm::cross(forward_, worldUp_));
        up_ = glm::normalize(glm::cross(right_, forward_));
        RecalculateView();
        RecalculateCombined();
    }

    /*!***********************************************************************
    @brief          Orients the camera to face toward a target world-space point.
    @param          target        World-space point the camera should look toward.
    @param          worldUp       World-space up vector for orientation reference.
    *************************************************************************/
    void Camera3D::SetLookAt(const glm::vec3& target, const glm::vec3& worldUp)
    {
        worldUp_ = worldUp;
        const glm::vec3 forward = target - position_;
        SetOrientation(forward, worldUp_);
    }

    /*!***********************************************************************
    @brief          Retrieves the current view transformation matrix.
    @return         Const reference to the 4x4 view matrix.
    *************************************************************************/
    const glm::mat4& Camera3D::GetViewMatrix() const
    {
        return viewMatrix_;
    }

    /*!***********************************************************************
    @brief          Retrieves the current perspective projection matrix.
    @return         Const reference to the 4x4 projection matrix.
    *************************************************************************/
    const glm::mat4& Camera3D::GetProjectionMatrix() const
    {
        return projectionMatrix_;
    }

    /*!***********************************************************************
    @brief          Retrieves the combined view-projection matrix.
    @return         Const reference to the 4x4 view-projection matrix.
    *************************************************************************/
    const glm::mat4& Camera3D::GetViewProjectionMatrix() const
    {
        return viewProjectionMatrix_;
    }

    /*!***********************************************************************
    @brief          Retrieves the current world-space camera position.
    @return         Const reference to the camera position vector.
    *************************************************************************/
    const glm::vec3& Camera3D::GetPosition() const
    {
        return position_;
    }

    /*!***********************************************************************
    @brief          Retrieves the normalized camera forward direction vector.
    @return         Const reference to the forward direction vector.
    *************************************************************************/
    const glm::vec3& Camera3D::GetForward() const
    {
        return forward_;
    }

    /*!***********************************************************************
    @brief          Retrieves the normalized camera local up direction vector.
    @return         Const reference to the up direction vector.
    *************************************************************************/
    const glm::vec3& Camera3D::GetUp() const
    {
        return up_;
    }

    /*!***********************************************************************
    @brief          Retrieves the normalized camera local right direction vector.
    @return         Const reference to the right direction vector.
    *************************************************************************/
    const glm::vec3& Camera3D::GetRight() const
    {
        return right_;
    }

    /*!***********************************************************************
    @brief          Recomputes the view matrix from current position and orientation state.
    *************************************************************************/
    void Camera3D::RecalculateView()
    {
        viewMatrix_ = glm::lookAt(position_, position_ + forward_, up_);
    }

    /*!***********************************************************************
    @brief          Recomputes the combined view-projection matrix from cached matrices.
    *************************************************************************/
    void Camera3D::RecalculateCombined()
    {
        viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;
    }
}
