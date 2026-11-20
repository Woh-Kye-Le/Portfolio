/* !
@file               Renderer2D.hpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Provides the Renderer2D class for orthographic 2D rendering of simple
    geometric primitives (squares, circles). It owns a 2D camera, built-in
    meshes and shader, and manages per-frame lifecycle and render state.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Graphics Headers
#include "engine/graphics/Camera/Camera2D.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Shader/Shader.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          2D orthographic renderer for drawing colored squares and circles with a built-in shader.
    *************************************************************************/
    class Renderer2D
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor.
        *************************************************************************/
        Renderer2D() = default;
        /*!***********************************************************************
        @brief          Deleted copy constructor.
        *************************************************************************/
        Renderer2D(const Renderer2D&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator.
        @return         Reference to this Renderer2D.
        *************************************************************************/
        Renderer2D& operator=(const Renderer2D&) = delete;
        /*!***********************************************************************
        @brief          Default move constructor.
        *************************************************************************/
        Renderer2D(Renderer2D&&) = default;
        /*!***********************************************************************
        @brief          Default move assignment operator.
        @return         Reference to this Renderer2D.
        *************************************************************************/
        Renderer2D& operator=(Renderer2D&&) = default;

        /*!***********************************************************************
        @brief          Initializes the orthographic camera, the square and circle meshes and the Standard2D shader.
        *************************************************************************/
        void Init();
        /*!***********************************************************************
        @brief          Releases the owned meshes and shader resources.
        *************************************************************************/
        void Shutdown();
        /*!***********************************************************************
        @brief          Prepares 2D render state (blend on, face culling off), fits the camera to the viewport and
                        begins a new frame.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        void BeginFrame(int width, int height);
        /*!***********************************************************************
        @brief          Ends the current 2D rendering frame.
        *************************************************************************/
        void EndFrame();

        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled square at the given 2D position.
        @param          angleDegrees  Rotation angle in degrees around the Z axis.
        @param          size          Uniform XY scale applied to the unit square.
        @param          position      XY center position of the square in world units.
        *************************************************************************/
        void DrawSquare(float angleDegrees, float size, const glm::vec2& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled circle at the given 2D position.
        @param          angleDegrees  Rotation angle in degrees around the Z axis.
        @param          size          Uniform XY scale applied to the unit circle.
        @param          position      XY center position of the circle in world units.
        *************************************************************************/
        void DrawCircle(float angleDegrees, float size, const glm::vec2& position) const;

    private:
        /*!***********************************************************************
        @brief          Fits the orthographic projection to the viewport aspect ratio, keeping a fixed view height of 3
                        units.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        void UpdateCameraAspect(int width, int height);

        /*!***********************************************************************
        @brief          Default tint colour of each shape drawn by the 2D renderer.
        *************************************************************************/
        struct ShapeColors
        {
            glm::vec4 square = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
            glm::vec4 circle = glm::vec4(1.0f, 0.55f, 0.15f, 1.0f);
        };

        Graphics::Camera2D camera_;
        Graphics::Mesh squareMesh_;
        Graphics::Mesh circleMesh_;
        Graphics::Shader standardShader_;
        ShapeColors colors_;
        bool initialized_ = false;
        bool frameActive_ = false;
    };
}
