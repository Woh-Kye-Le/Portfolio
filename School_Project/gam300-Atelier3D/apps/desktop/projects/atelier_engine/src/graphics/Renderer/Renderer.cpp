/* !
@file               Renderer.cpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Implements the Renderer wrapper which owns Renderer2D and Renderer3D while
    forwarding the legacy 3D API used by the rest of the engine.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Renderer/Renderer.hpp"

#include "engine/graphics/Renderer/RenderAPI.hpp"

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          Initializes the 3D camera, all procedural meshes, and the standard shader.
    *************************************************************************/
    void Renderer::Init()
    {
        Graphics::RenderAPI::EnableDepthTest(true);
        Graphics::RenderAPI::EnableFaceCulling(true, Graphics::CullFace::Back, Graphics::FrontFace::CCW);
        Graphics::RenderAPI::EnableBlend(false);
        renderer2D_.Init();
        renderer3D_.Init();
    }

    /*!***********************************************************************
    @brief          Releases all owned meshes and shader resources.
    *************************************************************************/
    void Renderer::Shutdown()
    {
        renderer3D_.Shutdown();
        renderer2D_.Shutdown();
    }

    /*!***********************************************************************
    @brief          Prepares 3D render state (blend off, cull+depth on) and begins a new frame.
    @param          width         Viewport width in pixels.
    @param          height        Viewport height in pixels.
    *************************************************************************/
    void Renderer::BeginFrame(int width, int height)
    {
        renderer2D_.BeginFrame(width, height);
        renderer3D_.BeginFrame(width, height);
    }

    /*!***********************************************************************
    @brief          Ends the current 3D rendering frame.
    *************************************************************************/
    void Renderer::EndFrame()
    {
        renderer3D_.EndFrame();
        renderer2D_.EndFrame();
    }

    /*!***********************************************************************
    @brief          Sets the colour the framebuffer is cleared to.
    @param          r             Red component (0 to 1).
    @param          g             Green component (0 to 1).
    @param          b             Blue component (0 to 1).
    @param          a             Alpha component (0 to 1).
    *************************************************************************/
    void Renderer::SetClearColor(float r, float g, float b, float a) const
    {
        Graphics::RenderAPI::SetClearColor(r, g, b, a);
    }

    /*!***********************************************************************
    @brief          Clears the current framebuffer to the clear colour.
    *************************************************************************/
    void Renderer::Clear() const
    {
        Graphics::RenderAPI::Clear();
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled quad at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Z axis.
    @param          size          Uniform XY scale applied to the unit quad.
    @param          position      XYZ center position of the quad in world units.
    *************************************************************************/
    void Renderer::DrawQuad(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawQuad(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled circle at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Z axis.
    @param          size          Uniform XY scale applied to the unit circle.
    @param          position      XYZ center position of the circle in world units.
    *************************************************************************/
    void Renderer::DrawCircle(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawCircle(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled cube at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
    @param          size          Uniform XYZ scale applied to the unit cube.
    @param          position      XYZ center position of the cube in world units.
    *************************************************************************/
    void Renderer::DrawCube(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawCube(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled pyramid at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Y axis.
    @param          size          Uniform XYZ scale applied to the unit pyramid.
    @param          position      XYZ base center position of the pyramid in world units.
    *************************************************************************/
    void Renderer::DrawPyramid(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawPyramid(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled ground plane at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Y axis.
    @param          size          Uniform XZ scale applied to the unit ground plane.
    @param          position      XYZ center position of the ground plane in world units.
    *************************************************************************/
    void Renderer::DrawPlane(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawPlane(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a wireframe box outline at the given 3D position and rotation.
    @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
    @param          size          Uniform XYZ scale applied to the unit wirebox.
    @param          position      XYZ center position of the wirebox in world units.
    *************************************************************************/
    void Renderer::DrawWirebox(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawWirebox(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled sphere at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
    @param          size          Uniform XYZ scale applied to the unit sphere.
    @param          position      XYZ center position of the sphere in world units.
    *************************************************************************/
    void Renderer::DrawSphere(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawSphere(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled line segment at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Y axis.
    @param          size          Uniform XYZ scale applied to the unit line.
    @param          position      XYZ center position of the line in world units.
    *************************************************************************/
    void Renderer::DrawLine(float angleDegrees, float size, const glm::vec3& position) const
    {
        renderer3D_.DrawLine(angleDegrees, size, position);
    }

    /*!***********************************************************************
    @brief          Sets the sky and haze every shape is drawn in (see Environment).
    @param          environment   New environment settings.
    *************************************************************************/
    void Renderer::SetEnvironment(const Environment& environment)
    {
        renderer3D_.SetEnvironment(environment);
    }

    /*!***********************************************************************
    @brief          Retrieves the sky and haze every shape is drawn in.
    @return         Const reference to the current Environment.
    *************************************************************************/
    const Environment& Renderer::GetEnvironment() const
    {
        return renderer3D_.GetEnvironment();
    }

    /*!***********************************************************************
    @brief          Retrieves the lit built-in shader used by default primitives. Imported meshes and asset
                    materials with no explicit shader use it to receive sun, fog and the tinted texture.
    @return         Const reference to the standard Shader.
    *************************************************************************/
    const Graphics::Shader& Renderer::StandardShader() const
    {
        return renderer3D_.StandardShader();
    }

    /*!***********************************************************************
    @brief          Sets the lights every shape is lit by: the first directional light is the sun, plus the sky
                    / ground ambient light. Starts as DefaultLighting().
    @param          lighting      New lighting context.
    *************************************************************************/
    void Renderer::SetLighting(const Graphics::LightingContext& lighting)
    {
        renderer3D_.SetLighting(lighting);
    }

    /*!***********************************************************************
    @brief          Retrieves the lights every shape is lit by.
    @return         Const reference to the current LightingContext.
    *************************************************************************/
    const Graphics::LightingContext& Renderer::GetLighting() const
    {
        return renderer3D_.GetLighting();
    }

    /*!***********************************************************************
    @brief          Builds the default lighting: a warm sun high to one side, over a soft blue sky and a dark
                    ground.
    @return         The default LightingContext.
    *************************************************************************/
    Graphics::LightingContext Renderer::DefaultLighting()
    {
        return Renderer3D::DefaultLighting();
    }

    /*!***********************************************************************
    @brief          Draws the sky behind everything (call first in a frame): a gradient from the horizon to the
                    zenith, the ground below the horizon, and the sun's glow. Nothing if the environment is off.
    *************************************************************************/
    void Renderer::DrawSky() const
    {
        renderer3D_.DrawSky();
    }

    /*!***********************************************************************
    @brief          Draws a built-in shape with a full model matrix (position, rotation on every axis, scale).
                    Each shape is unit-sized: a Plane is 1 x 1 at scale 1, like the others.
    @param          primitive     Which shape.
    @param          model         Model transformation matrix.
    *************************************************************************/
    void Renderer::DrawPrimitive(Primitive primitive, const glm::mat4& model) const
    {
        renderer3D_.DrawPrimitive(primitive, model);
    }

    /*!***********************************************************************
    @brief          Draws a built-in shape with a user-provided albedo multiplier composed on top of the per-
                    shape default tint.
    @param          primitive     Which shape.
    @param          model         Model transformation matrix.
    @param          tint          Per-object albedo multiplier, applied to the shape's default
                                  colour.
    *************************************************************************/
    void Renderer::DrawPrimitive(Primitive primitive, const glm::mat4& model, const glm::vec4& tint) const
    {
        renderer3D_.DrawPrimitive(primitive, model, tint);
    }

    /*!***********************************************************************
    @brief          Draws the editor's ground grid on the XZ plane (y = 0): 1-unit lines, brighter lines every
                    10, and the X (red) and Z (blue) axes. The grid moves with the camera in steps of 10 units,
                    so it looks endless.
    *************************************************************************/
    void Renderer::DrawGrid() const
    {
        renderer3D_.DrawGrid();
    }

    /*!***********************************************************************
    @brief          Retrieves the camera every Draw call is seen through (the editor's fly camera moves it while
                    a game plays).
    @return         Mutable reference to the 3D camera.
    *************************************************************************/
    Graphics::Camera3D& Renderer::Camera()
    {
        return renderer3D_.Camera();
    }

    /*!***********************************************************************
    @brief          Retrieves the camera every Draw call is seen through.
    @return         Const reference to the 3D camera.
    *************************************************************************/
    const Graphics::Camera3D& Renderer::Camera() const
    {
        return renderer3D_.Camera();
    }
}
