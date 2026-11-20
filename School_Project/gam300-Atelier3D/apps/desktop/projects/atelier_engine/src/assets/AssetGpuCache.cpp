/* !
@file    	    AssetGpuCache.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		29/09/2026
@brief
    Implements AssetGpuCache: lazy GPU upload per asset handle, fallback shader
    and white texture, the placeholder cube mesh, and cleanup of GPU objects for
    unloaded assets.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/AssetGpuCache.hpp"

#include "engine/assets/Animation.hpp"

#include "engine/core/FileSystem.hpp"
#include "engine/core/Log.hpp"
#include "engine/platform/Paths.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Engine::Assets
{
    namespace
    {
        // Last-resort fallback shader source (unlit textured). These strings are only used when the
        // GLSL files under Builtin/ cannot be found - the preferred path is always loading from disk.
        constexpr const char* kFallbackVertexShader = R"(#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_ViewProjection;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
)";

        constexpr const char* kFallbackFragmentShader = R"(#version 330 core
in vec2 v_TexCoord;
uniform sampler2D u_Texture;
out vec4 o_Color;

void main()
{
    o_Color = texture(u_Texture, v_TexCoord);
}
)";
    }

    // Needs a GL context, so GameRuntime calls this on the first frame
    void AssetGpuCache::Init()
    {
        if (initialized_)
        {
            return;
        }

        // Fallback shader: prefer loading the GLSL files from Builtin/. If the files are not available
        // next to the executable or in the source tree, fall back to the compiled-in strings so
        // something always draws even when assets are mis-deployed.
        auto fallback = std::make_shared<Graphics::Shader>("Builtin/FallbackUnlit");
        const std::string vsPath = Platform::BuiltinShaderPath("FallbackUnlit.vert");
        const std::string fsPath = Platform::BuiltinShaderPath("FallbackUnlit.frag");
        bool loadedFromFiles = false;
        if (!vsPath.empty() && !fsPath.empty())
        {
            const std::string vs = Core::ReadText(vsPath).value_or("");
            const std::string fs = Core::ReadText(fsPath).value_or("");
            if (!vs.empty() && !fs.empty() && fallback->Compile(vs, fs))
            {
                loadedFromFiles = true;
            }
        }
        if (!loadedFromFiles)
        {
            if (!fallback->Compile(kFallbackVertexShader, kFallbackFragmentShader))
            {
                LOG_ERROR(Shader, "AssetGpuCache: built-in fallback shader failed to compile");
            }
            else if (!vsPath.empty() || !fsPath.empty())
            {
                LOG_WARN(Shader, "AssetGpuCache: built-in FallbackUnlit shader files not found; "
                                 "using compiled-in fallback. Hint: copy src/assets/Shaders/Builtin/ "
                                 "next to the executable as assets/shaders/Builtin/.");
            }
        }
        fallbackShader_ = std::move(fallback);

        // 1x1 solid white texture (the "no albedo texture present" identity).
        Graphics::TextureSpec ws{};
        ws.format = Graphics::TextureFormat::RGBA8;
        ws.minFilter = Graphics::TextureFilter::Linear;
        ws.magFilter = Graphics::TextureFilter::Nearest;
        ws.wrapS = Graphics::TextureWrap::Repeat;
        ws.wrapT = Graphics::TextureWrap::Repeat;
        ws.hasMipmaps = false;
        const std::uint8_t white[4] = {255, 255, 255, 255};
        whiteTexture_ = Graphics::CreateTexture("white");
        if (!whiteTexture_->Create(1, 1, ws, white))
        {
            whiteTexture_.reset();
        }

        // Placeholder unit cube (used for: cube object with a texture but no mesh).
        placeholderCubeMesh_ =
            std::make_shared<Graphics::Mesh>(Graphics::Mesh::FromAssetData(texturedCube_, "PlaceholderCube"));

        initialized_ = true;
    }

    // Drop every GPU copy (meshes, textures, shaders, models) before the GL context goes away
    void AssetGpuCache::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }
        gpuMeshes_.clear();
        gpuShaders_.clear();
        gpuTextures_.clear();
        versions_.clear();
        retargeted_.clear();
        gpuMaterials_.clear();
        gpuModels_.clear();
        fallbackShader_.reset();
        whiteTexture_.reset();
        placeholderCubeMesh_.reset();
        initialized_ = false;
    }

    std::shared_ptr<Graphics::Mesh> AssetGpuCache::GetOrUploadMesh(MeshHandle handle, ResourceManager& resources)
    {
        if (!handle.IsValid())
        {
            return placeholderCubeMesh_;
        }

        // Already on the GPU? Then this is just a map lookup. Otherwise upload once and keep it.
        const std::uint64_t key = MakeKey(kKindMesh, handle);
        auto it = gpuMeshes_.find(key);
        if (it != gpuMeshes_.end())
        {
            return it->second;
        }

        Assets::Mesh* const cpuMesh = resources.Get(handle);
        const Assets::Mesh* const source = cpuMesh ? cpuMesh : &texturedCube_;
        auto gpu = std::make_shared<Graphics::Mesh>(Graphics::Mesh::FromAssetData(*source));
        return gpuMeshes_.emplace(key, std::move(gpu)).first->second;
    }

    std::shared_ptr<Graphics::Shader> AssetGpuCache::GetOrUploadShader(ShaderHandle handle, ResourceManager& resources)
    {
        if (!handle.IsValid())
        {
            return fallbackShader_;
        }

        const std::uint64_t key = MakeKey(kKindShader, handle);
        const std::uint32_t version = resources.VersionOf(handle);
        auto it = gpuShaders_.find(key);
        if (it != gpuShaders_.end() && versions_[key] == version)
        {
            return it->second;
        }
        if (it != gpuShaders_.end())
        {
            gpuShaders_.erase(it); // its file was saved since: compiled again below
        }
        versions_[key] = version;

        std::shared_ptr<Graphics::Shader> result = fallbackShader_;
        Assets::Shader* const cpuShader = resources.Get(handle);
        if (cpuShader)
        {
            auto s = std::make_shared<Graphics::Shader>("asset shader");
            if (s->Compile(*cpuShader))
            {
                result = std::move(s);
            }
            else
            {
                LOG_WARN(Shader, "Asset shader failed to compile; drawing with built-in fallback shader");
            }
        }
        return gpuShaders_.emplace(key, std::move(result)).first->second;
    }

    std::shared_ptr<Graphics::Texture> AssetGpuCache::GetOrUploadTexture(TextureHandle handle,
                                                                         ResourceManager& resources)
    {
        if (!handle.IsValid())
        {
            return whiteTexture_;
        }

        const std::uint64_t key = MakeKey(kKindTexture, handle);
        const std::uint32_t version = resources.VersionOf(handle);
        auto it = gpuTextures_.find(key);
        if (it != gpuTextures_.end() && versions_[key] == version)
        {
            return it->second;
        }
        if (it != gpuTextures_.end())
        {
            gpuTextures_.erase(it); // its file was saved since: uploaded again below
        }
        versions_[key] = version;

        std::shared_ptr<Graphics::Texture> result = whiteTexture_;
        Assets::Texture* const cpuTexture = resources.Get(handle);
        if (cpuTexture)
        {
            Graphics::TextureSpec s{};
            s.format = Graphics::TextureFormat::RGBA8;
            s.minFilter = Graphics::TextureFilter::Linear;
            s.magFilter = Graphics::TextureFilter::Nearest;
            s.wrapS = Graphics::TextureWrap::Repeat;
            s.wrapT = Graphics::TextureWrap::Repeat;
            s.hasMipmaps = false;
            auto tex = Graphics::CreateTexture();
            if (tex->Create(static_cast<std::uint32_t>(cpuTexture->width),
                            static_cast<std::uint32_t>(cpuTexture->height), s, cpuTexture->rgba.data()))
            {
                result = std::move(tex);
            }
        }
        return gpuTextures_.emplace(key, std::move(result)).first->second;
    }

    const Assets::AnimationClip* AssetGpuCache::RetargetedClip(ModelHandle model, ModelHandle from, int index,
                                                               ResourceManager& resources)
    {
        const Assets::Model* target = resources.Get(model);
        const Assets::Model* source = resources.Get(from);
        if (!target || !source || index < 0 || static_cast<std::size_t>(index) >= source->animations.size())
        {
            return nullptr;
        }
        const auto key = std::make_tuple(MakeKey(kKindModel, model), MakeKey(kKindModel, from), index);
        auto found = retargeted_.find(key);
        if (found == retargeted_.end())
        {
            found = retargeted_
                        .emplace(key, Assets::Retarget(source->animations[static_cast<std::size_t>(index)],
                                                       source->nodes, target->nodes))
                        .first;
        }
        return &found->second;
    }

    std::shared_ptr<Graphics::Model> AssetGpuCache::GetOrUploadModel(ModelHandle handle, ResourceManager& resources,
                                                                     const Graphics::Shader& defaultShader, int level)
    {
        if (!handle.IsValid())
        {
            return nullptr;
        }

        Assets::Model* const cpuModel = resources.Get(handle);
        if (level > 0 && (!cpuModel || static_cast<std::size_t>(level) > cpuModel->lods.size()))
        {
            level = 0; // a level it does not have
        }
        const std::uint64_t key =
            MakeKey(level == 0 ? kKindModel : kKindModelLod + static_cast<std::uint32_t>(level) - 1, handle);
        auto it = gpuModels_.find(key);
        if (it != gpuModels_.end())
        {
            return it->second;
        }

        if (!cpuModel)
        {
            return nullptr;
        }

        // A model's textures sit next to it, so a material's texture path is resolved against the
        // model's own folder and then loaded through the ResourceManager: one disk read and one
        // GPU upload per image, shared with every other user of that file.
        const std::filesystem::path modelDir = std::filesystem::path(resources.PathOf(handle)).parent_path();
        const auto resolveTexture = [this, &resources,
                                     &modelDir](const std::string& path) -> std::shared_ptr<Graphics::Texture>
        {
            const std::filesystem::path raw(path);
            const std::filesystem::path resolved = raw.is_absolute() ? raw : (modelDir / raw).lexically_normal();
            return GetOrUploadTexture(resources.LoadTexture(resolved.generic_string()), resources);
        };

        // A simpler version is the same model (nodes, materials, skins, animations) with its level's meshes
        std::optional<Assets::Model> simpler;
        if (level > 0)
        {
            simpler = *cpuModel;
            simpler->meshes = cpuModel->lods[static_cast<std::size_t>(level) - 1];
            simpler->lods.clear();
        }
        auto gpu = std::make_shared<Graphics::Model>(
            Graphics::Model::FromModelData(simpler ? *simpler : *cpuModel, defaultShader, resolveTexture));
        return gpuModels_.emplace(key, std::move(gpu)).first->second;
    }

    std::shared_ptr<Graphics::Material> AssetGpuCache::GetOrBuildMaterial(
        std::uint64_t cacheKey, const std::shared_ptr<Graphics::Shader>& shader,
        const std::shared_ptr<Graphics::Texture>& albedo)
    {
        const std::uint64_t key = MakeKey(kKindMaterial, static_cast<std::uint32_t>(cacheKey & 0xFFFFFFFFu),
                                          static_cast<std::uint32_t>(cacheKey >> 32));
        auto it = gpuMaterials_.find(key);
        if (it != gpuMaterials_.end())
        {
            return it->second;
        }

        auto mat = std::make_shared<Graphics::Material>();
        mat->SetShader(shader);
        mat->SetAlbedoTexture(albedo);
        return gpuMaterials_.emplace(key, std::move(mat)).first->second;
    }
}
