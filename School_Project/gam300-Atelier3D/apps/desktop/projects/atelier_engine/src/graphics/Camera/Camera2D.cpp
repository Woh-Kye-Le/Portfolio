/* !
@file               Camera2D.cpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Contains the implementation of the Camera2D class methods. Implements
    orthographic projection setup, position/rotation transforms, and
    view-projection matrix recalculation logic for 2D rendering.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Camera/Camera2D.hpp"

// Standard Library Headers
#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Default constructor initializing a unit orthographic projection from -1 to 1 on both axes.
    *************************************************************************/
    Camera2D::Camera2D()
    {
        SetProjection(-1.0f, 1.0f, -1.0f, 1.0f);
    }

    /*!***********************************************************************
    @brief          Constructs a camera with an explicit orthographic projection.
    @param          left          Left edge of the view volume.
    @param          right         Right edge of the view volume.
    @param          bottom        Bottom edge of the view volume.
    @param          top           Top edge of the view volume.
    *************************************************************************/
    Camera2D::Camera2D(float left, float right, float bottom, float top)
    {
        SetProjection(left, right, bottom, top);
    }

    /*!***********************************************************************
    @brief          Sets the orthographic projection bounds and refreshes the combined matrix.
    @param          left          Left edge of the view volume.
    @param          right         Right edge of the view volume.
    @param          bottom        Bottom edge of the view volume.
    @param          top           Top edge of the view volume.
    *************************************************************************/
    void Camera2D::SetProjection(float left, float right, float bottom, float top)
    {
        projectionMatrix_ = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
        RecalculateMatrices();
    }

    /*!***********************************************************************
    @brief          Sets the world-space position of the camera and refreshes the matrices.
    @param          position      New camera position vector.
    *************************************************************************/
    void Camera2D::SetPosition(const glm::vec3& position)
    {
        position_ = position;
        RecalculateMatrices();
    }

    /*!***********************************************************************
    @brief          Sets the camera rotation around the Z axis and refreshes the matrices.
    @param          rotationDegrees  Rotation angle in degrees.
    *************************************************************************/
    void Camera2D::SetRotation(float rotationDegrees)
    {
        rotationDegrees_ = rotationDegrees;
        RecalculateMatrices();
    }

    /*!***********************************************************************
    @brief          Retrieves the current view transformation matrix.
    @return         Const reference to the 4x4 view matrix.
    *************************************************************************/
    const glm::mat4& Camera2D::GetViewMatrix() const
    {
        return viewMatrix_;
    }

    /*!***********************************************************************
    @brief          Retrieves the current orthographic projection matrix.
    @return         Const reference to the 4x4 projection matrix.
    *************************************************************************/
    const glm::mat4& Camera2D::GetProjectionMatrix() const
    {
        return projectionMatrix_;
    }

    /*!***********************************************************************
    @brief          Retrieves the combined view-projection matrix.
    @return         Const reference to the 4x4 view-projection matrix.
    *************************************************************************/
    const glm::mat4& Camera2D::GetViewProjectionMatrix() const
    {
        return viewProjectionMatrix_;
    }

    /*!***********************************************************************
    @brief          Retrieves the world-space position of the camera.
    @return         Const reference to the camera position vector.
    *************************************************************************/
    const glm::vec3& Camera2D::GetPosition() const
    {
        return position_;
    }

    /*!***********************************************************************
    @brief          Retrieves the camera rotation around the Z axis.
    @return         Rotation angle in degrees.
    *************************************************************************/
    float Camera2D::GetRotation() const
    {
        return rotationDegrees_;
    }

    /*!***********************************************************************
    @brief          Rebuilds the view matrix from the position and rotation, then the combined view-projection matrix.
    *************************************************************************/
    void Camera2D::RecalculateMatrices()
    {
        const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position_);
        const glm::mat4 rotation =
            glm::rotate(glm::mat4(1.0f), glm::radians(rotationDegrees_), glm::vec3(0.0f, 0.0f, 1.0f));
        viewMatrix_ = rotation * translation;
        viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;
    }
}
