/* !
@file               Camera2D.hpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Declares the Camera2D class for 2D orthographic rendering. Implements the
    Camera interface with an orthographic projection, supporting position
    translation and Z-axis rotation for 2D scene viewing.

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
    @brief          2D orthographic camera supporting position and rotation transforms.
    *************************************************************************/
    class Camera2D : public Camera
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor initializing a unit orthographic projection from -1 to 1 on both axes.
        *************************************************************************/
        Camera2D();
        /*!***********************************************************************
        @brief          Constructs a camera with an explicit orthographic projection.
        @param          left          Left edge of the view volume.
        @param          right         Right edge of the view volume.
        @param          bottom        Bottom edge of the view volume.
        @param          top           Top edge of the view volume.
        *************************************************************************/
        Camera2D(float left, float right, float bottom, float top);

        /*!***********************************************************************
        @brief          Sets the orthographic projection bounds and refreshes the combined matrix.
        @param          left          Left edge of the view volume.
        @param          right         Right edge of the view volume.
        @param          bottom        Bottom edge of the view volume.
        @param          top           Top edge of the view volume.
        *************************************************************************/
        void SetProjection(float left, float right, float bottom, float top);
        /*!***********************************************************************
        @brief          Sets the world-space position of the camera and refreshes the matrices.
        @param          position      New camera position vector.
        *************************************************************************/
        void SetPosition(const glm::vec3& position);
        /*!***********************************************************************
        @brief          Sets the camera rotation around the Z axis and refreshes the matrices.
        @param          rotationDegrees  Rotation angle in degrees.
        *************************************************************************/
        void SetRotation(float rotationDegrees);

        /*!***********************************************************************
        @brief          Retrieves the current view transformation matrix.
        @return         Const reference to the 4x4 view matrix.
        *************************************************************************/
        const glm::mat4& GetViewMatrix() const override;
        /*!***********************************************************************
        @brief          Retrieves the current orthographic projection matrix.
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
        @brief          Retrieves the camera rotation around the Z axis.
        @return         Rotation angle in degrees.
        *************************************************************************/
        float GetRotation() const;

    private:
        /*!***********************************************************************
        @brief          Rebuilds the view matrix from the position and rotation, then the combined view-projection
                        matrix.
        *************************************************************************/
        void RecalculateMatrices();

        glm::mat4 viewMatrix_ = glm::mat4(1.0f);
        glm::mat4 projectionMatrix_ = glm::mat4(1.0f);
        glm::mat4 viewProjectionMatrix_ = glm::mat4(1.0f);
        glm::vec3 position_ = glm::vec3(0.0f);
        float rotationDegrees_ = 0.0f;
    };
}
