/* !
@file    	AssetGpuCache.hpp
@author  	w.kyele@digipen.edu [100%]
@date    	29/09/2026
@brief
    Declares AssetGpuCache, which uploads ResourceManager assets (meshes,
    textures, shaders, models) to the GPU once and hands out the shared GPU
    copies.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/ResourceManager.hpp"
#include "engine/graphics/Material/Material.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Model/Model.hpp"
#include "engine/graphics/Shader/Shader.hpp"
#include "engine/graphics/Texture/Texture.hpp"

#include "engine/assets/data/ModelData.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief  CPU-asset -> Graphics-wrapper cache. Owns the GPU upload step.

            The clean lifecycle:
              on-disk file -> Loader (Load*File) -> Assets::* blob
                                              -> ResourceManager owns refcount
                                              -> AssetGpuCache::GetOrUpload*()
                                                 (builds shared_ptr<Graphics::*>)
                                              -> GPU

            This keeps MeshRenderer stateless (it only Draws pre-uploaded
            resources) while keeping the GPU creation step testable and
            reusable by any caller (GameRuntime, the editor, load-screen
            precache, etc).

    *************************************************************************/
    class AssetGpuCache
    {
    public:
        AssetGpuCache() = default;
        ~AssetGpuCache() { Shutdown(); }

        AssetGpuCache(const AssetGpuCache&) = delete;
        AssetGpuCache& operator=(const AssetGpuCache&) = delete;

        /*!***********************************************************************
        @brief  Builds the three always-available fallback resources:
                fallbackShader_ (builtin unlit textured program),
                whiteTexture_ (1x1 solid white RGBA8), and
                placeholderCubeMesh_ (unit cube built from PlaceholderMesh()).
        *************************************************************************/
        void Init();

        /*!***********************************************************************
        @brief  Releases every cached Graphics object and clears the fallback
                resources. Safe to call even when already shut down.
        *************************************************************************/
        void Shutdown();

        /*!***********************************************************************
        @brief  Returns the GPU Mesh for the given handle, uploading it on first
                access using Graphics::Mesh::FromAssetData().
        @param  handle        Loaded CPU mesh handle (0 -> placeholder cube).
        @param  resources     ResourceManager that owns the CPU Assets::Mesh.
        @return Shared pointer to a GPU-ready Graphics::Mesh.
        *************************************************************************/
        std::shared_ptr<Graphics::Mesh> GetOrUploadMesh(MeshHandle handle, ResourceManager& resources);

        /*!***********************************************************************
        @brief  Returns the GPU Shader for the given handle, compiling it on
                first access; on any failure returns the builtin fallback shader.
        @param  handle        Loaded CPU shader handle (0 -> fallback shader).
        @param  resources     ResourceManager that owns the CPU Assets::Shader.
        @return Shared pointer to a compiled Graphics::Shader (never nullptr).
        *************************************************************************/
        std::shared_ptr<Graphics::Shader> GetOrUploadShader(ShaderHandle handle, ResourceManager& resources);

        /*!***********************************************************************
        @brief  Returns the GPU Texture for the given handle, uploading its
                rgba byte blob on first access; empty handle -> white texture.
        @param  handle        Loaded CPU texture handle (0 -> white texture).
        @param  resources     ResourceManager that owns the CPU Assets::Texture.
        @return Shared pointer to a bound-ready Graphics::Texture (never nullptr).
        *************************************************************************/
        std::shared_ptr<Graphics::Texture> GetOrUploadTexture(TextureHandle handle, ResourceManager& resources);

        /*!***********************************************************************
        @brief  Returns the GPU Model for the given handle, uploading it (its
                meshes, materials and the textures those materials reference)
                on first access. The textures are loaded through the
                ResourceManager, so a texture shared with another model, or with
                an object's own texture slot, is read and uploaded once.
        @param  handle        Loaded CPU model handle (invalid -> nullptr).
        @param  resources     ResourceManager that owns the CPU Assets::Model;
                              also the root the material texture paths resolve against.
        @param  defaultShader Shader given to every material of the model.
        @return Shared pointer to a GPU-ready Graphics::Model, nullptr for a
                stale handle.
        *************************************************************************/
        /*!***********************************************************************
        @brief  A clip of an animation file made to move a model with the same skeleton (Assets::Retarget), made
                once and kept.
        @param  model         The model it moves.
        @param  from          The file the clip is in.
        @param  index         Which of its animations.
        @param  resources     ResourceManager that owns both CPU models.
        @return The clip, or nullptr for a stale handle or a clip that is not there.
        *************************************************************************/
        const Assets::AnimationClip* RetargetedClip(ModelHandle model, ModelHandle from, int index,
                                                    ResourceManager& resources);

        // level: 0 for the model itself, 1 or 2 for its simpler versions (ModelData::lods); a level it does not have
        // gives the model itself. Each level is uploaded once, the first time it is drawn.
        std::shared_ptr<Graphics::Model> GetOrUploadModel(ModelHandle handle, ResourceManager& resources,
                                                          const Graphics::Shader& defaultShader, int level = 0);

        /*!***********************************************************************
        @brief  Builds (or fetches from cache) a Material combining one mesh +
                texture + shader triplet so identical triplets share one GPU
                material instance across frames.
        *************************************************************************/
        std::shared_ptr<Graphics::Material> GetOrBuildMaterial(std::uint64_t cacheKey,
                                                               const std::shared_ptr<Graphics::Shader>& shader,
                                                               const std::shared_ptr<Graphics::Texture>& albedo);

        // Accessors for the built-in stand-ins (always valid after Init())
        std::shared_ptr<Graphics::Shader> FallbackShader() const { return fallbackShader_; }
        std::shared_ptr<Graphics::Texture> WhiteTexture() const { return whiteTexture_; }
        std::shared_ptr<Graphics::Mesh> PlaceholderCube() const { return placeholderCubeMesh_; }
        const Assets::Mesh& PlaceholderCubeAsset() const { return texturedCube_; }

    private:
        // 64-bit cache keys: upper 32 bits = kind discriminator, lower 32 = XOR of index + generation.
        static constexpr std::uint64_t MakeKey(std::uint32_t kind, std::uint32_t idx, std::uint32_t gen)
        {
            return (static_cast<std::uint64_t>(kind) << 32) |
                   (static_cast<std::uint64_t>(idx) ^ (static_cast<std::uint64_t>(gen) << 16));
        }
        template <class T> static constexpr std::uint64_t MakeKey(std::uint32_t kind, const Handle<T>& h)
        {
            return MakeKey(kind, h.index, h.generation);
        }
        static constexpr std::uint32_t kKindMesh = 1;
        static constexpr std::uint32_t kKindTexture = 2;
        static constexpr std::uint32_t kKindShader = 3;
        static constexpr std::uint32_t kKindMaterial = 4;
        static constexpr std::uint32_t kKindModel = 5;
        static constexpr std::uint32_t kKindModelLod = 8; // + level - 1: a model's simpler versions

        template <class K, class V> using Cache = std::unordered_map<K, std::shared_ptr<V>>;

        Cache<std::uint64_t, Graphics::Mesh> gpuMeshes_;
        Cache<std::uint64_t, Graphics::Shader> gpuShaders_;
        Cache<std::uint64_t, Graphics::Texture> gpuTextures_;
        Cache<std::uint64_t, Graphics::Material> gpuMaterials_;
        Cache<std::uint64_t, Graphics::Model> gpuModels_;
        // The file version each shader/texture was made from
        std::unordered_map<std::uint64_t, std::uint32_t> versions_;
        // Clips of animation files made to move a model (RetargetedClip), by (model, file, clip)
        std::map<std::tuple<std::uint64_t, std::uint64_t, int>, Assets::AnimationClip> retargeted_;

        std::shared_ptr<Graphics::Shader> fallbackShader_;
        std::shared_ptr<Graphics::Texture> whiteTexture_;
        std::shared_ptr<Graphics::Mesh> placeholderCubeMesh_;
        Assets::Mesh texturedCube_ = Assets::PlaceholderMesh();
        bool initialized_ = false;
    };
}
