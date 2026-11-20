/* !
@file               Renderer2D.cpp
@author             w.kyele@digipen.edu [100%]
@date                   26/10/2026
@brief
    Implements the Renderer2D class, including camera setup, procedural mesh
    creation, built-in shader loading via search paths, and the 2D square and
    circle draw routines using the standard 2D shader pipeline.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Renderer/Renderer2D.hpp"

#include "engine/core/Log.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"
#include "engine/platform/Paths.hpp"

// Third-Party Headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Standard Library Headers
#include <string>

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          Initializes the orthographic camera, the square and circle meshes and the Standard2D shader.
    *************************************************************************/
    void Renderer2D::Init()
    {
        if (initialized_)
        {
            return;
        }

        camera_.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        camera_.SetRotation(0.0f);
        UpdateCameraAspect(16, 9);

        squareMesh_ = Graphics::Mesh::CreateQuad(1.0f, 1.0f);
        circleMesh_ = Graphics::Mesh::CreateCircle(1.0f, 48);

        const std::string vsPath = Platform::BuiltinShaderPath("Standard2D.vert");
        const std::string fsPath = Platform::BuiltinShaderPath("Standard2D.frag");

        standardShader_ = Graphics::Shader("Builtin/Standard2D");
        if (vsPath.empty() || !standardShader_.LoadFromFiles(vsPath, fsPath))
        {
            LOG_ERROR(Render,
                      "Renderer2D: could not load the Standard2D shader (VS: {}, FS: {}). Copy "
                      "src/assets/Shaders/Builtin/ next to the executable as assets/shaders/Builtin/ or set "
                      "GAM300_ROOT_DIR to apps/desktop.",
                      vsPath.empty() ? "<empty>" : vsPath, fsPath.empty() ? "<empty>" : fsPath);
        }

        colors_ = ShapeColors{};

        initialized_ = true;
    }

    /*!***********************************************************************
    @brief          Releases the owned meshes and shader resources.
    *************************************************************************/
    void Renderer2D::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }
        standardShader_ = Graphics::Shader();
        circleMesh_ = {};
        squareMesh_ = {};
        frameActive_ = false;
        initialized_ = false;
    }

    /*!***********************************************************************
    @brief          Fits the orthographic projection to the viewport aspect ratio, keeping a fixed view height of 3
                    units.
    @param          width         Viewport width in pixels.
    @param          height        Viewport height in pixels.
    *************************************************************************/
    void Renderer2D::UpdateCameraAspect(int width, int height)
    {
        const float w = width > 0 ? static_cast<float>(width) : 1.0f;
        const float h = height > 0 ? static_cast<float>(height) : 1.0f;
        const float aspect = w / h;
        const float viewH = 3.0f;
        const float viewW = viewH * aspect;
        camera_.SetProjection(-viewW * 0.5f, viewW * 0.5f, -viewH * 0.5f, viewH * 0.5f);
    }

    /*!***********************************************************************
    @brief          Prepares 2D render state (blend on, face culling off), fits the camera to the viewport and begins a
                    new frame.
    @param          width         Viewport width in pixels.
    @param          height        Viewport height in pixels.
    *************************************************************************/
    void Renderer2D::BeginFrame(int width, int height)
    {
        if (!initialized_)
        {
            Init();
        }
        Graphics::RenderAPI::EnableBlend(true);
        Graphics::RenderAPI::EnableFaceCulling(false);
        UpdateCameraAspect(width, height);
        frameActive_ = true;
    }

    /*!***********************************************************************
    @brief          Ends the current 2D rendering frame.
    *************************************************************************/
    void Renderer2D::EndFrame()
    {
        if (!initialized_)
        {
            return;
        }
        frameActive_ = false;
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled square at the given 2D position.
    @param          angleDegrees  Rotation angle in degrees around the Z axis.
    @param          size          Uniform XY scale applied to the unit square.
    @param          position      XY center position of the square in world units.
    *************************************************************************/
    void Renderer2D::DrawSquare(float angleDegrees, float size, const glm::vec2& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));
        model = glm::rotate(model, glm::radians(angleDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(size, size, 1.0f));

        standardShader_.Bind();
        standardShader_.SetMat4("u_ViewProjection", camera_.GetViewProjectionMatrix());
        standardShader_.SetMat4("u_Model", model);
        standardShader_.SetFloat4("u_AlbedoTint", colors_.square);

        squareMesh_.Bind();
        Graphics::RenderAPI::DrawIndexed(squareMesh_.GetIndexCount());
        squareMesh_.Unbind();

        standardShader_.Unbind();
    }

    /*!***********************************************************************
    @brief          Draws a tinted, rotated, scaled circle at the given 2D position.
    @param          angleDegrees  Rotation angle in degrees around the Z axis.
    @param          size          Uniform XY scale applied to the unit circle.
    @param          position      XY center position of the circle in world units.
    *************************************************************************/
    void Renderer2D::DrawCircle(float angleDegrees, float size, const glm::vec2& position) const
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));
        model = glm::rotate(model, glm::radians(angleDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(size, size, 1.0f));

        standardShader_.Bind();
        standardShader_.SetMat4("u_ViewProjection", camera_.GetViewProjectionMatrix());
        standardShader_.SetMat4("u_Model", model);
        standardShader_.SetFloat4("u_AlbedoTint", colors_.circle);

        circleMesh_.Bind();
        Graphics::RenderAPI::DrawIndexed(circleMesh_.GetIndexCount());
        circleMesh_.Unbind();

        standardShader_.Unbind();
    }
}
