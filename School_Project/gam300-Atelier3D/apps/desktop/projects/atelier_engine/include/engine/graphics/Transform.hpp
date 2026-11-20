/* !
@file    	    Transform.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares the Transform struct used by the renderers to describe an object's
    world-space placement: translation, Euler-angle rotation (degrees), and non-
    uniform scale. Exposes ComposeModelMatrix() to produce the standard
    ImGuizmo-compatible model matrix (TRS: translate * Rz * Ry * Rx * scale).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Third-Party Headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Aggregates the three TRS components of an object's world-space
                    transform and provides a convenience mat4 composer in the
                    ImGuizmo-compatible order (translate, Rz, Ry, Rx, scale).
    *************************************************************************/
    struct Transform
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotationEulerDegrees = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);

        /*!***********************************************************************
        @brief          Builds the model matrix: T * Rz * Ry * Rx * S (rotations in degrees).
                        Matches ImGuizmo::RecomposeMatrixFromComponents.
        @return         4x4 homogeneous TRS matrix suitable for MeshRenderer::Draw.
        *************************************************************************/
        glm::mat4 ComposeModelMatrix() const
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
            m = glm::rotate(m, glm::radians(rotationEulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(rotationEulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
            m = glm::rotate(m, glm::radians(rotationEulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
            return glm::scale(m, scale);
        }

        /*!***********************************************************************
        @brief          Builds a model matrix with an additional arbitrary-axis rotation
                        inserted between the Euler rotation group and the scale.
                        Used by Renderer helpers that accept a single spin axis.
        @param          extraAxisAngleDegrees  Rotation angle in degrees.
        @param          extraAxis              Unit axis around which to rotate.
        @return         T * Rz*Ry*Rx * R(axis,angle) * S 4x4 matrix.
        *************************************************************************/
        glm::mat4 ComposeModelMatrix(float extraAxisAngleDegrees, const glm::vec3& extraAxis) const
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
            m = glm::rotate(m, glm::radians(rotationEulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(rotationEulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
            m = glm::rotate(m, glm::radians(rotationEulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
            m = glm::rotate(m, glm::radians(extraAxisAngleDegrees), extraAxis);
            return glm::scale(m, scale);
        }
    };
}
