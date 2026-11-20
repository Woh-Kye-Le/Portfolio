/* !
@file    	    AssetPreviews.cpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    Implements AssetPreviews: uploads the Assets browser's images as textures
    and renders its models into small offscreen framebuffers, a few per frame.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Shader.hpp brings in GLEW, which must come before anything that includes GL/gl.h
#include "engine/graphics/Shader/Shader.hpp"

#include "editor/panels/assets/AssetPreviews.hpp"

#include "editor/SavedGlState.hpp"

#include "engine/assets/Resources.hpp"
#include "engine/core/Log.hpp"
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/graphics/Material/Material.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/OpenGL/FramebufferGL.hpp"
#include "engine/graphics/OpenGL/TextureGL.hpp"
#include "engine/graphics/Renderer/MeshRenderer.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <system_error>
#include <unordered_set>

namespace Editor::Panels
{
    namespace
    {
        // Grey clay lit by a key light and a fill light, so the model's shape reads at a glance whatever its
        // textures are
        constexpr const char* kModelVertexShader = R"(#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
uniform mat4 u_Model;
uniform mat4 u_ViewProjection;
uniform mat3 u_NormalMatrix;
out vec3 v_Normal;
void main()
{
    v_Normal = normalize(u_NormalMatrix * a_Normal);
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
)";

        constexpr const char* kModelFragmentShader = R"(#version 330 core
in vec3 v_Normal;
uniform vec4 u_AlbedoTint;
out vec4 o_Color;
void main()
{
    vec3 n = normalize(gl_FrontFacing ? v_Normal : -v_Normal);
    float key = max(dot(n, normalize(vec3(0.5, 0.8, 0.6))), 0.0);
    float fill = max(dot(n, normalize(vec3(-0.6, 0.2, -0.4))), 0.0);
    o_Color = vec4(u_AlbedoTint.rgb * (0.25 + 0.75 * key + 0.25 * fill), 1.0);
}
)";
    }

    AssetPreviews::AssetPreviews() = default;
    AssetPreviews::~AssetPreviews() = default;

    void AssetPreviews::BeginFrame()
    {
        Engine::Graphics::RenderAPI::Init(); // does nothing once GLEW is loaded
        budget_ = Engine::Graphics::RenderAPI::IsReady() ? kMaxNewPerFrame : 0;
    }

    const AssetPreviews::Preview* AssetPreviews::Get(const std::filesystem::path& projectDir, const AssetEntry& asset)
    {
        if (asset.kind != AssetKind::Texture && asset.kind != AssetKind::Model)
        {
            return nullptr; // sounds, materials and shaders show their badge
        }
        if (const auto found = entries_.find(asset.path); found != entries_.end())
        {
            return &found->second.preview;
        }
        if (budget_ <= 0)
        {
            return nullptr;
        }
        --budget_;

        const std::filesystem::path file = projectDir / asset.path;
        Entry entry;
        std::error_code ec;
        entry.writeTime = std::filesystem::last_write_time(file, ec);
        if (asset.kind == AssetKind::Texture)
        {
            MakeTexturePreview(file, entry);
        }
        else
        {
            MakeModelPreview(file, entry);
        }
        return &entries_.insert_or_assign(asset.path, std::move(entry)).first->second.preview;
    }

    void AssetPreviews::Prune(const std::filesystem::path& projectDir, const std::vector<AssetEntry>& assets)
    {
        std::unordered_set<std::string> listed;
        for (const AssetEntry& asset : assets)
        {
            listed.insert(asset.path);
        }
        std::erase_if(entries_,
                      [&](const auto& item)
                      {
                          std::error_code ec;
                          const auto writeTime = std::filesystem::last_write_time(projectDir / item.first, ec);
                          return !listed.contains(item.first) || ec || writeTime != item.second.writeTime;
                      });
    }

    void AssetPreviews::Clear()
    {
        entries_.clear();
        modelShader_.reset();
        modelShaderFailed_ = false;
    }

    void AssetPreviews::MakeTexturePreview(const std::filesystem::path& file, Entry& entry)
    {
        const auto image = Engine::Assets::LoadTextureFromFile(file);
        if (!image || image->width <= 0 || image->height <= 0)
        {
            entry.preview.details = "Could not read this image";
            return;
        }

        Engine::Graphics::TextureSpec spec{};
        spec.width = static_cast<std::uint32_t>(image->width);
        spec.height = static_cast<std::uint32_t>(image->height);
        auto texture = std::make_unique<Engine::Graphics::OpenGL::TextureGL>("AssetPreview");
        if (!texture->Create(spec.width, spec.height, spec, image->rgba.data()))
        {
            entry.preview.details = "Could not upload this image";
            return;
        }
        entry.preview.textureId = texture->GetRendererID();
        entry.preview.width = image->width;
        entry.preview.height = image->height;
        entry.preview.details = std::format("{} x {}", image->width, image->height);
        entry.texture = std::move(texture);
    }

    void AssetPreviews::MakeModelPreview(const std::filesystem::path& file, Entry& entry)
    {
        const auto model = Engine::Assets::LoadMeshFromFile(file);
        if (!model || model->positions.size() < 3 || model->indices.empty())
        {
            entry.preview.details = "Could not read this model";
            return;
        }
        const std::size_t vertexCount = model->positions.size() / 3;
        entry.preview.details = std::format("{} vertices, {} triangles", vertexCount, model->indices.size() / 3);

        const Engine::Graphics::Shader* shader = ModelShader();
        if (!shader)
        {
            return; // the details still show, without a picture
        }

        // Frame the model's bounding sphere
        glm::vec3 low(model->positions[0], model->positions[1], model->positions[2]);
        glm::vec3 high = low;
        for (std::size_t i = 1; i < vertexCount; ++i)
        {
            const glm::vec3 p(model->positions[i * 3], model->positions[i * 3 + 1], model->positions[i * 3 + 2]);
            low = glm::min(low, p);
            high = glm::max(high, p);
        }
        const glm::vec3 center = (low + high) * 0.5f;
        const float radius = std::max(glm::length(high - low) * 0.5f, 0.001f);
        constexpr float kFieldOfView = 35.0f;
        const float distance = radius / std::sin(glm::radians(kFieldOfView * 0.5f)) * 1.05f;

        Engine::Graphics::Camera3D camera;
        camera.SetPerspective(kFieldOfView, 1.0f, std::max(distance - radius * 1.5f, distance * 0.01f),
                              distance + radius * 1.5f);
        camera.SetPosition(center + glm::normalize(glm::vec3(0.9f, 0.7f, 1.2f)) * distance);
        camera.SetLookAt(center);

        // Creating the framebuffer binds it too, so the editor's state is saved before that
        const Editor::SavedGlState saved;
        auto render = std::make_unique<Engine::Graphics::OpenGL::FramebufferGL>(kModelPreviewSize, kModelPreviewSize);
        if (!render->IsValid())
        {
            return;
        }
        const Engine::Graphics::Mesh mesh = Engine::Graphics::Mesh::FromAssetData(*model, "AssetPreview");
        Engine::Graphics::Material material;
        material.SetShader(*shader);
        material.SetAlbedoTint(glm::vec4(0.78f, 0.80f, 0.84f, 1.0f));

        render->Bind();
        glViewport(0, 0, kModelPreviewSize, kModelPreviewSize);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // see-through around the model
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Engine::Renderer::MeshRenderer::Draw(mesh, material, glm::mat4(1.0f), camera);

        entry.preview.textureId = render->GetColorTextureID();
        entry.preview.width = kModelPreviewSize;
        entry.preview.height = kModelPreviewSize;
        entry.render = std::move(render);
    }

    const Engine::Graphics::Shader* AssetPreviews::ModelShader()
    {
        if (!modelShader_ && !modelShaderFailed_)
        {
            auto shader = std::make_unique<Engine::Graphics::Shader>("AssetPreview");
            if (shader->Compile(kModelVertexShader, kModelFragmentShader))
            {
                modelShader_ = std::move(shader);
            }
            else
            {
                modelShaderFailed_ = true;
                LOG_WARN(Editor, "Assets: the model preview shader did not compile; models show without a picture");
            }
        }
        return modelShader_.get();
    }
}
