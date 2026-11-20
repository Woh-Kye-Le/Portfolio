/* !
@file               Renderer3D.hpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Provides the Renderer3D class for perspective 3D rendering of simple
    geometric primitives such as quads, cubes, pyramids, planes, wireboxes,
    spheres, and lines using a built-in standard 3D shader and camera. It also
    owns the sky / environment, the scene lights and the editor's ground grid.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

/*
 * FUTURE: 3D submit queues, frustum culling, shadows and debug lines.
 */

// Engine Graphics Headers
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Renderer/Environment.hpp"
#include "engine/graphics/Renderer/MeshRenderer.hpp"
#include "engine/graphics/Shader/Shader.hpp"
#include "engine/graphics/Transform.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <cstdint>

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          3D renderer that draws lit built-in shapes, the sky, the editor grid and imported meshes with one
                    standard shader and a perspective camera.
    *************************************************************************/
    class Renderer3D
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor.
        *************************************************************************/
        Renderer3D() = default;
        /*!***********************************************************************
        @brief          Deleted copy constructor.
        *************************************************************************/
        Renderer3D(const Renderer3D&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator.
        @return         Reference to this Renderer3D.
        *************************************************************************/
        Renderer3D& operator=(const Renderer3D&) = delete;
        /*!***********************************************************************
        @brief          Default move constructor.
        *************************************************************************/
        Renderer3D(Renderer3D&&) = default;
        /*!***********************************************************************
        @brief          Default move assignment operator.
        @return         Reference to this Renderer3D.
        *************************************************************************/
        Renderer3D& operator=(Renderer3D&&) = default;

        /*!***********************************************************************
        @brief          Initializes the 3D camera, all procedural meshes and the Standard3D and Sky shaders.
        *************************************************************************/
        void Init();
        /*!***********************************************************************
        @brief          Releases all owned meshes and shader resources.
        *************************************************************************/
        void Shutdown();
        /*!***********************************************************************
        @brief          Prepares 3D render state (blend off, back-face culling and depth test on), fits the camera to
                        the viewport and applies the environment and lighting to the standard shader.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        void BeginFrame(int width, int height);
        /*!***********************************************************************
        @brief          Ends the current 3D rendering frame.
        *************************************************************************/
        void EndFrame();

        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled quad at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around Z axis.
        @param          size          Uniform XY scale applied to the unit quad.
        @param          position      XYZ center position of the quad in world units.
        *************************************************************************/
        void DrawQuad(float angleDegrees, float size, const glm::vec3& position) const;
        /*!***********************************************************************
        @brief          Draws a tinted, rotated, scaled circle at the given 3D position.
        @param          angleDegrees  Rotation angle in degrees around Z axis.
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

        static constexpr float kFieldOfViewDegrees = 60.0f;
        static constexpr float kNearPlane = 0.05f;
        static constexpr float kFarPlane = 1000.0f;

        /*!***********************************************************************
        @brief          Replaces the sky and haze every shape is drawn in.
        @param          environment   New environment settings.
        *************************************************************************/
        void SetEnvironment(const Environment& environment) { environment_ = environment; }
        /*!***********************************************************************
        @brief          Retrieves the sky and haze every shape is drawn in.
        @return         Const reference to the current Environment.
        *************************************************************************/
        const Environment& GetEnvironment() const { return environment_; }

        /*!***********************************************************************
        @brief          Retrieves the lit built-in shader used by default primitives. Imported meshes and asset
                        materials with no explicit shader use it to receive the sun, fog and tinted texture.
        @return         Const reference to the standard shader.
        *************************************************************************/
        const Graphics::Shader& StandardShader() const { return standardShader_; }

        /*!***********************************************************************
        @brief          Replaces the lights every shape is lit by: the first directional light is the sun, plus the sky
                        and ground ambient light.
        @param          lighting      New lighting context.
        *************************************************************************/
        void SetLighting(const Graphics::LightingContext& lighting) { lighting_ = lighting; }
        /*!***********************************************************************
        @brief          Retrieves the lights every shape is lit by.
        @return         Const reference to the current LightingContext.
        *************************************************************************/
        const Graphics::LightingContext& GetLighting() const { return lighting_; }
        /*!***********************************************************************
        @brief          Builds the default lighting: a warm sun high to one side over a soft blue sky and a dark ground,
                        plus four coloured point lights.
        @return         LightingContext holding the default sun, ambient light and point lights.
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
        enum class Primitive : std::uint8_t
        {
            Quad,
            Circle,
            Cube,
            Pyramid,
            Plane,
            Wirebox,
            Sphere,
            Line
        };

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
        @brief          Draws a built-in shape with a material of the caller's (an object's Material), whose shader
                        the caller has already given the lights.
        @param          primitive     Which shape.
        @param          model         Model transformation matrix.
        @param          material      What it is drawn with.
        *************************************************************************/
        void DrawPrimitiveWith(Primitive primitive, const glm::mat4& model, const Graphics::Material& material) const;
        /*!***********************************************************************
        @brief          Draws the editor's ground grid on the XZ plane (y = 0): 1-unit lines, brighter lines every 10,
                        and the X (red) and Z (blue) axes. The grid moves with the camera in steps of 10 units, so it
                        looks endless.
        *************************************************************************/
        void DrawGrid() const;

        /*!***********************************************************************
        @brief          Retrieves the camera every Draw call is seen through (the editor's fly camera moves it while a
                        game plays).
        @return         Reference to the 3D camera.
        *************************************************************************/
        Graphics::Camera3D& Camera() { return camera_; }
        /*!***********************************************************************
        @brief          Retrieves the camera every Draw call is seen through (the editor's fly camera moves it while a
                        game plays).
        @return         Const reference to the 3D camera.
        *************************************************************************/
        const Graphics::Camera3D& Camera() const { return camera_; }

    private:
        /*!***********************************************************************
        @brief          Rebuilds the perspective projection for the viewport size; a width or height of zero or less
                        counts as 1.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        void UpdateCameraAspect(int width, int height);
        /*!***********************************************************************
        @brief          Computes the normal matrix (inverse transpose of the upper 3x3) for a model matrix.
        @param          model         Model transformation matrix.
        @return         3x3 matrix for transforming normals.
        *************************************************************************/
        glm::mat3 NormalMatrixFromModel(const glm::mat4& model) const;

        static inline const glm::vec4 kDefaultShapeGrey = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);

        /*!***********************************************************************
        @brief          Default tint colour of each built-in shape.
        *************************************************************************/
        struct ShapeColors
        {
            glm::vec4 quad = kDefaultShapeGrey;
            glm::vec4 circle = kDefaultShapeGrey;
            glm::vec4 cube = kDefaultShapeGrey;
            glm::vec4 pyramid = kDefaultShapeGrey;
            glm::vec4 plane = kDefaultShapeGrey;
            glm::vec4 wirebox = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            glm::vec4 sphere = kDefaultShapeGrey;
            glm::vec4 line = glm::vec4(1.0f, 1.0f, 0.2f, 1.0f);
        };

        Graphics::Camera3D camera_;
        Graphics::Mesh quadMesh_;
        Graphics::Mesh circleMesh_;
        Graphics::Mesh cubeMesh_;
        Graphics::Mesh pyramidMesh_;
        Graphics::Mesh planeMesh_;
        Graphics::Mesh wireboxMesh_;
        Graphics::Mesh sphereMesh_;
        Graphics::Mesh lineMesh_;
        Graphics::Mesh gridMesh_;
        Graphics::Mesh gridAxesMesh_;
        Graphics::Mesh skyMesh_;
        Graphics::Shader standardShader_;
        Graphics::Shader skyShader_;
        Environment environment_;
        Graphics::LightingContext lighting_ = DefaultLighting();
        ShapeColors colors_;
        bool initialized_ = false;
        bool frameActive_ = false;
    };
}
