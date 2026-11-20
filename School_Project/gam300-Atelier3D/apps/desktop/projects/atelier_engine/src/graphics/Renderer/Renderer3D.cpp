/* !
@file               Renderer3D.cpp
@author             w.kyele@digipen.edu [100%]
@date                   26/10/2026
@brief
    Implements the Renderer3D class, including 3D perspective camera setup,
    procedural mesh creation for eight primitive shapes, built-in shader
    resolution, the standard 3D shader draw pipeline per primitive, and the
    sky / grid / environment helpers.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*
 * FUTURE: 3D submit queues, material+lighting binds, frustum culling, debug lines.
 *         For now: prototype DrawCube/DrawPyramid helpers only.
 */

// Engine Graphics Headers
#include "engine/graphics/Renderer/Renderer3D.hpp"

#include "engine/core/Log.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"
#include "engine/graphics/Texture/Texture.hpp"
#include "engine/platform/Paths.hpp"

// Third-Party Headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Standard Library Headers
#include <cmath>
#include <string>

namespace Engine::Renderer
{
    namespace
    {
        constexpr float kNear = Renderer3D::kNearPlane;
        constexpr float kFar = Renderer3D::kFarPlane;

        constexpr int kGridHalfExtent = 60;
        constexpr int kGridMajor = 10;

        // A material on the given shader with the given albedo tint
        Graphics::Material MakeTintedMaterial(const Graphics::Shader& shader, const glm::vec4& tint)
        {
            Graphics::Material material;
            material.SetShader(shader);
            material.SetAlbedoTint(tint);
            return material;
        }
    }

    /*!***********************************************************************
    @brief          Builds the default lighting: a warm sun high to one side over a soft blue sky and a dark ground,
                    plus four coloured point lights.
    @return         LightingContext holding the default sun, ambient light and point lights.
    *************************************************************************/
    Graphics::LightingContext Renderer3D::DefaultLighting()
    {
        Graphics::LightingContext lighting;
        lighting.skyColor = glm::vec3(0.42f, 0.56f, 0.77f);
        lighting.groundColor = glm::vec3(0.22f, 0.23f, 0.25f);
        lighting.ambientStrength = 0.55f;
        lighting.AddDirectional(-glm::normalize(glm::vec3(0.45f, 0.8f, 0.35f)), glm::vec3(1.0f, 0.95f, 0.86f), 0.85f);
        lighting.AddPoint(glm::vec3(-6.5f, 3.5f, 1.0f), glm::vec3(1.0f, 0.55f, 0.25f), 1.15f, 20.0f);
        lighting.AddPoint(glm::vec3(6.5f, 4.2f, -1.0f), glm::vec3(0.35f, 0.65f, 1.0f), 1.10f, 22.0f);
        lighting.AddPoint(glm::vec3(0.0f, 7.0f, -5.5f), glm::vec3(1.0f, 1.0f, 0.75f), 1.0f, 28.0f);
        lighting.AddPoint(glm::vec3(0.0f, -1.5f, 5.5f), glm::vec3(0.75f, 0.85f, 1.0f), 0.95f, 24.0f);
        return lighting;
    }

    /*!***********************************************************************
    @brief          Initializes the 3D camera, all procedural meshes and the Standard3D and Sky shaders.
    *************************************************************************/
    void Renderer3D::Init()
    {
        if (initialized_)
        {
            return;
        }

        camera_.SetPosition(glm::vec3(0.0f, 1.0f, 4.0f));
        camera_.SetOrientation(glm::vec3(0.0f, 0.0f, -1.0f));
        camera_.SetPerspective(kFieldOfViewDegrees, 16.0f / 9.0f, kNear, kFar);

        quadMesh_ = Graphics::Mesh::CreateQuad(1.0f, 1.0f);
        circleMesh_ = Graphics::Mesh::CreateCircle(1.0f, 48);
        cubeMesh_ = Graphics::Mesh::CreateCube(1.0f);
        pyramidMesh_ = Graphics::Mesh::CreatePyramid(0.7f, -0.6f, 0.6f);
        planeMesh_ = Graphics::Mesh::CreatePlane(10.0f, 1);
        wireboxMesh_ = Graphics::Mesh::CreateWirebox(1.0f);
        sphereMesh_ = Graphics::Mesh::CreateSphere(1.0f, 16, 32);
        lineMesh_ = Graphics::Mesh::CreateLine(glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f));
        gridMesh_ = Graphics::Mesh::CreateGrid(kGridHalfExtent, kGridMajor);
        gridAxesMesh_ = Graphics::Mesh::CreateGridAxes(kFar);
        skyMesh_ = Graphics::Mesh(
            {{.position = {-1.0f, -1.0f, 0.0f}}, {.position = {3.0f, -1.0f, 0.0f}}, {.position = {-1.0f, 3.0f, 0.0f}}},
            {0, 1, 2}, "Sky");

        const std::string skyVsPath = Platform::BuiltinShaderPath("Sky.vert");
        const std::string skyFsPath = Platform::BuiltinShaderPath("Sky.frag");
        skyShader_ = Graphics::Shader("Builtin/Sky");
        if (skyVsPath.empty() || !skyShader_.LoadFromFiles(skyVsPath, skyFsPath))
        {
            LOG_ERROR(Render, "Renderer3D: could not load the Sky shader (VS: {}, FS: {}) - the background stays plain",
                      skyVsPath.empty() ? "<empty>" : skyVsPath, skyFsPath.empty() ? "<empty>" : skyFsPath);
        }

        const std::string vsPath = Platform::BuiltinShaderPath("Standard3D.vert");
        const std::string fsPath = Platform::BuiltinShaderPath("Standard3D.frag");
        standardShader_ = Graphics::Shader("Builtin/Standard3D");
        if (vsPath.empty() || !standardShader_.LoadFromFiles(vsPath, fsPath))
        {
            LOG_ERROR(Render,
                      "Renderer3D: could not load the Standard3D shader (VS: {}, FS: {}). Copy "
                      "src/assets/Shaders/Builtin/ next to the executable as assets/shaders/Builtin/ or set "
                      "GAM300_ROOT_DIR to apps/desktop.",
                      vsPath.empty() ? "<empty>" : vsPath, fsPath.empty() ? "<empty>" : fsPath);
        }

        colors_ = ShapeColors{};
        initialized_ = true;
    }

    /*!***********************************************************************
    @brief          Releases all owned meshes and shader resources.
    *************************************************************************/
    void Renderer3D::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }

        standardShader_ = Graphics::Shader();
        sphereMesh_ = {};
        wireboxMesh_ = {};
        planeMesh_ = {};
        pyramidMesh_ = {};
        cubeMesh_ = {};
        circleMesh_ = {};
        quadMesh_ = {};
        lineMesh_ = {};
        gridMesh_ = {};
        gridAxesMesh_ = {};
        skyMesh_ = {};
        skyShader_ = Graphics::Shader();
        frameActive_ = false;
        initialized_ = false;
    }

    /*!***********************************************************************
    @brief          Rebuilds the perspective projection for the viewport size; a width or height of zero or less counts
                    as 1.
    @param          width         Viewport width in pixels.
    @param          height        Viewport height in pixels.
    *************************************************************************/
    void Renderer3D::UpdateCameraAspect(int width, int height)
    {
        const float w = width > 0 ? static_cast<float>(width) : 1.0f;
        const float h = height > 0 ? static_cast<float>(height) : 1.0f;
        camera_.SetPerspective(kFieldOfViewDegrees, w / h, kNear, kFar);
    }

    /*!***********************************************************************
    @brief          Computes the normal matrix (inverse transpose of the upper 3x3) for a model matrix.
    @param          model         Model transformation matrix.
    @return         3x3 matrix for transforming normals.
    *************************************************************************/
    glm::mat3 Renderer3D::NormalMatrixFromModel(const glm::mat4& model) const
    {
        return glm::transpose(glm::inverse(glm::mat3(model)));
    }

    /*!***********************************************************************
    @brief          Prepares 3D render state (blend off, back-face culling and depth test on), fits the camera to the
                    viewport and applies the environment and lighting to the standard shader.
    @param          width         Viewport width in pixels.
    @param          height        Viewport height in pixels.
    *************************************************************************/
    void Renderer3D::BeginFrame(int width, int height)
    {
        if (!initialized_)
        {
            Init();
        }

        Graphics::RenderAPI::EnableBlend(false);
        Graphics::RenderAPI::EnableFaceCulling(true, Graphics::CullFace::Back, Graphics::FrontFace::CCW);
        Graphics::RenderAPI::EnableDepthTest(true);
        UpdateCameraAspect(width, height);

        standardShader_.Bind();
        environment_.Apply(standardShader_, lighting_, camera_.GetPosition());
        standardShader_.SetInt("u_Lit", 1);

        frameActive_ = true;
    }

    /*!***********************************************************************
    @brief          Ends the current 3D rendering frame.
    *************************************************************************/
    void Renderer3D::EndFrame()
    {
        if (!initialized_)
        {
            return;
        }
        frameActive_ = false;
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled quad at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around Z axis.
    @param          size          Uniform XY scale applied to the unit quad.
    @param          position      XYZ center position of the quad in world units.
    *************************************************************************/
    void Renderer3D::DrawQuad(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size, size, 1.0f);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.0f, 0.0f, 1.0f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.quad);
        MeshRenderer::Draw(quadMesh_, material, model, camera_);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled circle at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around Z axis.
    @param          size          Uniform XY scale applied to the unit circle.
    @param          position      XYZ center position of the circle in world units.
    *************************************************************************/
    void Renderer3D::DrawCircle(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size, size, 1.0f);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.0f, 0.0f, 1.0f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.circle);
        MeshRenderer::Draw(circleMesh_, material, model, camera_);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled cube at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
    @param          size          Uniform XYZ scale applied to the unit cube.
    @param          position      XYZ center position of the cube in world units.
    *************************************************************************/
    void Renderer3D::DrawCube(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.6f, 1.0f, 0.2f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.cube);
        MeshRenderer::Draw(cubeMesh_, material, model, camera_);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled pyramid at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Y axis.
    @param          size          Uniform XYZ scale applied to the unit pyramid.
    @param          position      XYZ base center position of the pyramid in world units.
    *************************************************************************/
    void Renderer3D::DrawPyramid(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.2f, 1.0f, 0.0f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.pyramid);
        MeshRenderer::Draw(pyramidMesh_, material, model, camera_);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled ground plane at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Y axis.
    @param          size          Uniform XZ scale applied to the unit plane.
    @param          position      XYZ center position of the plane in world units.
    *************************************************************************/
    void Renderer3D::DrawPlane(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size * 0.1f, 1.0f, size * 0.1f);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.0f, 1.0f, 0.0f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.plane);
        MeshRenderer::Draw(planeMesh_, material, model, camera_);
    }

    /*!***********************************************************************
    @brief          Draws a wireframe box outline at the given 3D position and rotation.
    @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
    @param          size          Uniform XYZ scale applied to the unit wirebox.
    @param          position      XYZ center position of the wirebox in world units.
    *************************************************************************/
    void Renderer3D::DrawWirebox(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.6f, 1.0f, 0.2f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.wirebox);
        MeshRenderer::Draw(wireboxMesh_, material, model, camera_, Graphics::PrimitiveType::Lines);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled sphere at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around an arbitrary axis.
    @param          size          Uniform XYZ scale applied to the unit sphere.
    @param          position      XYZ center position of the sphere in world units.
    *************************************************************************/
    void Renderer3D::DrawSphere(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.0f, 1.0f, 0.3f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.sphere);
        MeshRenderer::Draw(sphereMesh_, material, model, camera_);
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled line segment at the given 3D position.
    @param          angleDegrees  Rotation angle in degrees around the Y axis.
    @param          size          Uniform XYZ scale applied to the unit line.
    @param          position      XYZ center position of the line in world units.
    *************************************************************************/
    void Renderer3D::DrawLine(float angleDegrees, float size, const glm::vec3& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        Graphics::Transform transform;
        transform.position = position;
        transform.scale = glm::vec3(size);
        const glm::mat4 model = transform.ComposeModelMatrix(angleDegrees, glm::vec3(0.0f, 1.0f, 0.0f));

        Graphics::Material material = MakeTintedMaterial(standardShader_, colors_.line);
        MeshRenderer::Draw(lineMesh_, material, model, camera_, Graphics::PrimitiveType::Lines);
    }

    /*!***********************************************************************
    @brief          Draws a built-in shape with a full model matrix (position, rotation on every axis, scale).
                    Each shape is unit-sized: a Plane is 1 x 1 at scale 1, like the others.
    @param          primitive     Which shape.
    @param          model         Model transformation matrix.
    *************************************************************************/
    void Renderer3D::DrawPrimitive(Primitive primitive, const glm::mat4& model) const
    {
        DrawPrimitive(primitive, model, glm::vec4(1.0f));
    }

    /*!***********************************************************************
    @brief          Draws a built-in shape with a user-provided albedo multiplier composed on top of the per-shape
                    default tint.
    @param          primitive     Which shape.
    @param          model         Model transformation matrix.
    @param          userTint      Per-object albedo multiplier, applied to the shape's default colour.
    *************************************************************************/
    void Renderer3D::DrawPrimitive(Primitive primitive, const glm::mat4& model, const glm::vec4& userTint) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        const Graphics::Mesh* mesh = nullptr;
        glm::vec4 tint = colors_.cube;
        glm::mat4 unitModel = model;
        auto primitiveType = Graphics::PrimitiveType::Triangles;
        switch (primitive)
        {
        case Primitive::Quad:
            mesh = &quadMesh_;
            tint = colors_.quad;
            break;
        case Primitive::Circle:
            mesh = &circleMesh_;
            tint = colors_.circle;
            break;
        case Primitive::Cube:
            mesh = &cubeMesh_;
            tint = colors_.cube;
            break;
        case Primitive::Pyramid:
            mesh = &pyramidMesh_;
            tint = colors_.pyramid;
            break;
        case Primitive::Plane:
            mesh = &planeMesh_;
            tint = colors_.plane;
            unitModel = glm::scale(model, glm::vec3(0.1f, 1.0f, 0.1f));
            break;
        case Primitive::Wirebox:
            mesh = &wireboxMesh_;
            tint = colors_.wirebox;
            primitiveType = Graphics::PrimitiveType::Lines;
            break;
        case Primitive::Sphere:
            mesh = &sphereMesh_;
            tint = colors_.sphere;
            break;
        case Primitive::Line:
            mesh = &lineMesh_;
            tint = colors_.line;
            primitiveType = Graphics::PrimitiveType::Lines;
            break;
        }

        standardShader_.Bind();
        environment_.Apply(standardShader_, lighting_, camera_.GetPosition());
        standardShader_.SetInt("u_Lit", primitiveType == Graphics::PrimitiveType::Lines ? 0 : 1);

        Graphics::Material material = MakeTintedMaterial(standardShader_, tint * userTint);
        MeshRenderer::Draw(*mesh, material, unitModel, camera_, primitiveType);
    }

    /*!***********************************************************************
    @brief          Draws a built-in shape with the caller's material (its shader already has the lights).
    @param          primitive     Which shape.
    @param          model         Model transformation matrix.
    @param          material      What it is drawn with.
    *************************************************************************/
    void Renderer3D::DrawPrimitiveWith(Primitive primitive, const glm::mat4& model,
                                       const Graphics::Material& material) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }
        const Graphics::Mesh* mesh = &cubeMesh_;
        glm::mat4 unitModel = model;
        auto primitiveType = Graphics::PrimitiveType::Triangles;
        switch (primitive)
        {
        case Primitive::Quad: mesh = &quadMesh_; break;
        case Primitive::Circle: mesh = &circleMesh_; break;
        case Primitive::Cube: break; // already the mesh
        case Primitive::Pyramid: mesh = &pyramidMesh_; break;
        case Primitive::Plane:
            mesh = &planeMesh_;
            unitModel = glm::scale(model, glm::vec3(0.1f, 1.0f, 0.1f)); // as DrawPrimitive
            break;
        case Primitive::Sphere: mesh = &sphereMesh_; break;
        case Primitive::Wirebox:
            mesh = &wireboxMesh_;
            primitiveType = Graphics::PrimitiveType::Lines;
            break;
        case Primitive::Line:
            mesh = &lineMesh_;
            primitiveType = Graphics::PrimitiveType::Lines;
            break;
        }
        MeshRenderer::Draw(*mesh, material, unitModel, camera_, primitiveType);
    }

    /*!***********************************************************************
    @brief          Draws the editor's ground grid on the XZ plane (y = 0): 1-unit lines, brighter lines every 10, and
                    the X (red) and Z (blue) axes. The grid moves with the camera in steps of 10 units, so it looks
                    endless.
    *************************************************************************/
    void Renderer3D::DrawGrid() const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        const glm::vec3 eye = camera_.GetPosition();
        const float step = static_cast<float>(kGridMajor);
        const glm::vec3 offset(std::round(eye.x / step) * step, 0.0f, std::round(eye.z / step) * step);

        standardShader_.Bind();
        environment_.Apply(standardShader_, lighting_, camera_.GetPosition());
        standardShader_.SetInt("u_Lit", 0);
        // The shader keeps the last material's settings: a textured model drawn just before would otherwise tint the
        // grid with its texture
        standardShader_.SetInt("u_Textured", 0);
        standardShader_.SetMat4("u_ViewProjection", camera_.GetViewProjectionMatrix());
        standardShader_.SetMat3("u_NormalMatrix", glm::mat3(1.0f));
        standardShader_.SetFloat4("u_AlbedoTint", glm::vec4(1.0f));

        standardShader_.SetMat4("u_Model", glm::translate(glm::mat4(1.0f), offset));
        gridMesh_.Bind();
        Graphics::RenderAPI::DrawIndexed(gridMesh_.GetIndexCount(), Graphics::PrimitiveType::Lines);
        gridMesh_.Unbind();

        standardShader_.SetMat4("u_Model", glm::mat4(1.0f));
        gridAxesMesh_.Bind();
        Graphics::RenderAPI::DrawIndexed(gridAxesMesh_.GetIndexCount(), Graphics::PrimitiveType::Lines);
        gridAxesMesh_.Unbind();

        standardShader_.Unbind();
    }

    /*!***********************************************************************
    @brief          Draws the sky behind everything (call first in a frame): a gradient from the horizon to the zenith,
                    the ground below the horizon, and the sun's glow. Nothing if the environment is off.
    *************************************************************************/
    void Renderer3D::DrawSky() const
    {
        if (!initialized_ || !frameActive_ || !environment_.enabled || skyShader_.GetRendererID() == 0)
        {
            return;
        }

        Graphics::RenderAPI::EnableDepthTest(false);
        skyShader_.Bind();
        skyShader_.SetMat4("u_InverseViewProjection", glm::inverse(camera_.GetViewProjectionMatrix()));
        skyShader_.SetFloat3("u_CameraPos", camera_.GetPosition());
        skyShader_.SetFloat3("u_Zenith", environment_.zenithColor);
        skyShader_.SetFloat3("u_Horizon", environment_.horizonColor);
        skyShader_.SetFloat3("u_Ground", environment_.groundColor);
        const Graphics::Light* sun = lighting_.FindSun();
        skyShader_.SetFloat3("u_SunDirection", sun ? -glm::normalize(sun->direction) : glm::vec3(0.0f, 1.0f, 0.0f));
        skyShader_.SetFloat3("u_SunColor", sun ? sun->color : glm::vec3(0.0f));
        skyShader_.SetInt("u_HasPicture", environment_.skyPicture ? 1 : 0);
        skyShader_.SetFloat("u_PictureTurn", glm::radians(environment_.skyTurn));
        skyShader_.SetInt("u_Picture", 0);
        if (environment_.skyPicture)
        {
            environment_.skyPicture->Bind(0);
        }
        skyMesh_.Bind();
        Graphics::RenderAPI::DrawIndexed(skyMesh_.GetIndexCount());
        skyMesh_.Unbind();
        if (environment_.skyPicture)
        {
            environment_.skyPicture->Unbind();
        }
        skyShader_.Unbind();
        Graphics::RenderAPI::EnableDepthTest(true);
    }
}
