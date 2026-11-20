/* !
@file               Renderer.hpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Provides the top-level Renderer wrapper which owns Renderer2D and
    Renderer3D while preserving the legacy 3D-facing API used across the
    engine and editor.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Graphics Headers
#include "engine/graphics/Renderer/Renderer2D.hpp"
#include "engine/graphics/Renderer/Renderer3D.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          Top-level renderer that owns the 2D and 3D renderers and forwards the 3D-facing API used
                    across the engine and editor.
    *************************************************************************/
    class Renderer
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor.
        *************************************************************************/
        Renderer() = default;
        /*!***********************************************************************
        @brief          Deleted copy constructor.
        *************************************************************************/
        Renderer(const Renderer&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator.
        @return         Reference to this Renderer.
        *************************************************************************/
        Renderer& operator=(const Renderer&) = delete;
        /*!***********************************************************************
        @brief          Default move constructor.
        *************************************************************************/
        Renderer(Renderer&&) = default;
        /*!***********************************************************************
        @brief          Default move assignment operator.
        @return         Reference to this Renderer.
        *************************************************************************/
        Renderer& operator=(Renderer&&) = default;

        /*!***********************************************************************
        @brief          Initializes the 3D camera, all procedural meshes, and the standard shader.
        *************************************************************************/
        void Init();
        /*!***********************************************************************
        @brief          Releases all owned meshes and shader resources.
        *************************************************************************/
        void Shutdown();
        /*!***********************************************************************
        @brief          Prepares 3D render state (blend off, cull+depth on) and begins a new frame.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        void BeginFrame(int width, int height);
        /*!***********************************************************************
        @brief          Ends the current 3D rendering frame.
        *************************************************************************/
        void EndFrame();

        /*!***********************************************************************
        @brief          Sets the colour the framebuffer is cleared to.
        @param          r             Red component (0 to 1).
        @param          g             Green component (0 to 1).
        @param          b             Blue component (0 to 1).
        @param          a             Alpha component (0 to 1).
        *************************************************************************/
        void SetClearColor(float r, float g, float b, float a) const;
        /*!***********************************************************************
        @brief          Clears the current framebuffer to the clear colour.
        *************************************************************************/
        void Clear() const;

        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled quad at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around the Z axis.
        @param          size          Uniform XY scale applied to the unit quad.
        @param          position      XYZ center position of the quad in world units.
        *************************************************************************/
        void DrawQuad(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled circle at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around the Z axis.
        @param          size          Uniform XY scale applied to the unit circle.
        @param          position      XYZ center position of the circle in world units.
        *************************************************************************/
        void DrawCircle(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled cube at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
        @param          size          Uniform XYZ scale applied to the unit cube.
        @param          position      XYZ center position of the cube in world units.
        *************************************************************************/
        void DrawCube(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled pyramid at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around the Y axis.
        @param          size          Uniform XYZ scale applied to the unit pyramid.
        @param          position      XYZ base center position of the pyramid in world units.
        *************************************************************************/
        void DrawPyramid(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled ground plane at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around the Y axis.
        @param          size          Uniform XZ scale applied to the unit plane.
        @param          position      XYZ center position of the plane in world units.
        *************************************************************************/
        void DrawPlane(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a wireframe box outline at the given 3D position and rotation.
        @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
        @param          size          Uniform XYZ scale applied to the unit wirebox.
        @param          position      XYZ center position of the wirebox in world units.
        *************************************************************************/
        void DrawWirebox(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled sphere at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
        @param          size          Uniform XYZ scale applied to the unit sphere.
        @param          position      XYZ center position of the sphere in world units.
        *************************************************************************/
        void DrawSphere(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled line segment at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around the Y axis.
        @param          size          Uniform XYZ scale applied to the unit line.
        @param          position      XYZ center position of the line in world units.
        *************************************************************************/
        void DrawLine(float angleDegrees, float size, const glm::vec3& position) const;

        // The camera's lens (the editor builds the same projection for its gizmos)
        static constexpr float kFieldOfViewDegrees = Renderer3D::kFieldOfViewDegrees;
        static constexpr float kNearPlane = Renderer3D::kNearPlane;
        static constexpr float kFarPlane = Renderer3D::kFarPlane;

        /*!***********************************************************************
        @brief          Sets the sky and haze every shape is drawn in (see Environment).
        @param          environment   New environment settings.
        *************************************************************************/
        void SetEnvironment(const Environment& environment);
        /*!***********************************************************************
        @brief          Retrieves the sky and haze every shape is drawn in.
        @return         Const reference to the current Environment.
        *************************************************************************/
        const Environment& GetEnvironment() const;

        /*!***********************************************************************
        @brief          Retrieves the lit built-in shader used by default primitives. Imported meshes and asset
                        materials with no explicit shader use it to receive sun, fog and the tinted texture.
        @return         Const reference to the standard Shader.
        *************************************************************************/
        const Graphics::Shader& StandardShader() const;

        /*!***********************************************************************
        @brief          Sets the lights every shape is lit by: the first directional light is the sun, plus the sky
                        / ground ambient light. Starts as DefaultLighting().
        @param          lighting      New lighting context.
        *************************************************************************/
        void SetLighting(const Graphics::LightingContext& lighting);
        /*!***********************************************************************
        @brief          Retrieves the lights every shape is lit by.
        @return         Const reference to the current LightingContext.
        *************************************************************************/
        const Graphics::LightingContext& GetLighting() const;
        /*!***********************************************************************
        @brief          Builds the default lighting: a warm sun high to one side, over a soft blue sky and a dark
                        ground.
        @return         The default LightingContext.
        *************************************************************************/
        static Graphics::LightingContext DefaultLighting();

        /*!***********************************************************************
        @brief          Draws the sky behind everything (call first in a frame): a gradient from the horizon to the
                        zenith, the ground below the horizon, and the sun's glow. Nothing if the environment is off.
        *************************************************************************/
        void DrawSky() const;

        /*!***********************************************************************
        @brief          The built-in shapes DrawPrimitive can draw.
        *************************************************************************/
        using Primitive = Renderer3D::Primitive;

        /*!***********************************************************************
        @brief          Draws a built-in shape with a full model matrix (position, rotation on every axis, scale).
                        Each shape is unit-sized: a Plane is 1 x 1 at scale 1, like the others.
        @param          primitive     Which shape.
        @param          model         Model transformation matrix.
        *************************************************************************/
        void DrawPrimitive(Primitive primitive, const glm::mat4& model) const;

        /*!***********************************************************************
        @brief          Draws a built-in shape with a user-provided albedo multiplier composed on top of the per-shape
                        default tint.
        @param          primitive     Which shape.
        @param          model         Model transformation matrix.
        @param          tint          Per-object albedo multiplier, applied to the shape's default colour.
        *************************************************************************/
        void DrawPrimitive(Primitive primitive, const glm::mat4& model, const glm::vec4& tint) const;

        /*!***********************************************************************
        @brief          Draws the editor's ground grid on the XZ plane (y = 0): 1-unit lines, brighter lines every 10,
                        and the X (red) and Z (blue) axes. The grid moves with the camera in steps of 10 units, so
                        it looks endless.
        *************************************************************************/
        void DrawGrid() const;

        /*!***********************************************************************
        @brief          Retrieves the camera every Draw call is seen through (the editor's fly camera moves it while
                        a game plays).
        @return         Mutable reference to the 3D camera.
        *************************************************************************/
        Graphics::Camera3D& Camera();
        /*!***********************************************************************
        @brief          Retrieves the camera every Draw call is seen through.
        @return         Const reference to the 3D camera.
        *************************************************************************/
        const Graphics::Camera3D& Camera() const;

        /*!***********************************************************************
        @brief          Retrieves the owned 2D renderer.
        @return         Const reference to the Renderer2D.
        *************************************************************************/
        const class Renderer2D& GetRenderer2D() const { return renderer2D_; }
        /*!***********************************************************************
        @brief          Retrieves the owned 2D renderer.
        @return         Mutable reference to the Renderer2D.
        *************************************************************************/
        class Renderer2D& GetRenderer2D() { return renderer2D_; }
        /*!***********************************************************************
        @brief          Retrieves the owned 3D renderer.
        @return         Const reference to the Renderer3D.
        *************************************************************************/
        const class Renderer3D& GetRenderer3D() const { return renderer3D_; }
        /*!***********************************************************************
        @brief          Retrieves the owned 3D renderer.
        @return         Mutable reference to the Renderer3D.
        *************************************************************************/
        class Renderer3D& GetRenderer3D() { return renderer3D_; }

    private:
        Engine::Renderer::Renderer2D renderer2D_;
        Engine::Renderer::Renderer3D renderer3D_;
    };
}
