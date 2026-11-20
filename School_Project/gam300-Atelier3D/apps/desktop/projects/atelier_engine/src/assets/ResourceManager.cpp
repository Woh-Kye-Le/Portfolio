/* !
@file    	    ResourceManager.cpp
@author  	    j.cai@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements ResourceManager: path-keyed slots with generation-checked
    handles, reference counting, placeholder fallback with warnings, periodic
    file checks and the asset list for the editor.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/ResourceManager.hpp"

#include "engine/assets/MeshSimplifier.hpp"
#include "engine/assets/loaders/GltfLoader.hpp"

#include "engine/core/FileSystem.hpp"
#include "engine/core/Log.hpp"

#include <filesystem>
#include <system_error>
#include <type_traits>
#include <utility>

namespace Engine::Assets
{
    namespace
    {
        // One Traits per asset type: how to load it and what to use when it's missing.
        // This way Acquire/Reload/etc. below are written once for all four types.
        template <class T> struct Traits;

        template <> struct Traits<Texture>
        {
            static constexpr AssetType kType = AssetType::Texture;
            static std::optional<Texture> Load(const std::filesystem::path& path) { return LoadTextureFromFile(path); }
            static Texture Placeholder() { return PlaceholderTexture(); }
        };

        template <> struct Traits<Mesh>
        {
            static constexpr AssetType kType = AssetType::Mesh;
            static std::optional<Mesh> Load(const std::filesystem::path& path) { return LoadMeshFromFile(path); }
            static Mesh Placeholder() { return PlaceholderMesh(); }
        };

        template <> struct Traits<Shader>
        {
            static constexpr AssetType kType = AssetType::Shader;
            static std::optional<Shader> Load(const std::filesystem::path& path) { return LoadShaderFromFile(path); }
            static Shader Placeholder() { return PlaceholderShader(); }
        };

        template <> struct Traits<Model>
        {
            static constexpr AssetType kType = AssetType::Model;
            // With its simpler versions for level of detail, made here so every model has them (MeshSimplifier.hpp)
            static std::optional<Model> Load(const std::filesystem::path& path)
            {
                std::optional<Model> model = LoadModelFromFile(path);
                if (model)
                {
                    LoadOrGenerateLods(*model, path);
                }
                return model;
            }
            static Model Placeholder() { return PlaceholderModel(); }
        };

        template <> struct Traits<AudioClip>
        {
            static constexpr AssetType kType = AssetType::Audio;
            static std::optional<AudioClip> Load(const std::filesystem::path& path) { return LoadAudioFile(path); }
            static AudioClip Placeholder() { return PlaceholderAudio(); }
        };

        template <class T> std::string Describe(const std::string& path)
        {
            return std::string(ToString(Traits<T>::kType)) + " '" + path + "'";
        }
    }

    // Shown in the Loaded Assets table and the log
    const char* ToString(AssetType type)
    {
        switch (type)
        {
        case AssetType::Texture: return "Texture";
        case AssetType::Mesh: return "Mesh";
        case AssetType::Shader: return "Shader";
        case AssetType::Model: return "Model";
        case AssetType::Audio: return "Audio";
        }
        return "Unknown";
    }

    ResourceManager::ResourceManager(std::filesystem::path rootDir) : rootDir_(std::move(rootDir)) {}

    // clang-format off
    TextureHandle ResourceManager::LoadTexture(std::string_view path) { return Acquire(textures_, path); }
    MeshHandle ResourceManager::LoadMesh(std::string_view path) { return Acquire(meshes_, path); }
    ShaderHandle ResourceManager::LoadShader(std::string_view path) { return Acquire(shaders_, path); }
    ModelHandle ResourceManager::LoadModel(std::string_view path) { return Acquire(models_, path); }
    AudioHandle ResourceManager::LoadAudio(std::string_view path) { return Acquire(audio_, path); }

    void ResourceManager::AddRef(TextureHandle handle) { if (auto* s = Find(textures_, handle)) { ++s->refCount; } }
    void ResourceManager::AddRef(MeshHandle handle) { if (auto* s = Find(meshes_, handle)) { ++s->refCount; } }
    void ResourceManager::AddRef(ShaderHandle handle) { if (auto* s = Find(shaders_, handle)) { ++s->refCount; } }
    void ResourceManager::AddRef(ModelHandle handle) { if (auto* s = Find(models_, handle)) { ++s->refCount; } }
    void ResourceManager::AddRef(AudioHandle handle) { if (auto* s = Find(audio_, handle)) { ++s->refCount; } }

    void ResourceManager::Release(TextureHandle handle) { ReleaseIn(textures_, handle); }
    void ResourceManager::Release(MeshHandle handle) { ReleaseIn(meshes_, handle); }
    void ResourceManager::Release(ShaderHandle handle) { ReleaseIn(shaders_, handle); }
    void ResourceManager::Release(ModelHandle handle) { ReleaseIn(models_, handle); }
    void ResourceManager::Release(AudioHandle handle) { ReleaseIn(audio_, handle); }

    Texture* ResourceManager::Get(TextureHandle handle) { auto* s = Find(textures_, handle); return s ? &s->data : nullptr; }
    Mesh* ResourceManager::Get(MeshHandle handle) { auto* s = Find(meshes_, handle); return s ? &s->data : nullptr; }
    Shader* ResourceManager::Get(ShaderHandle handle) { auto* s = Find(shaders_, handle); return s ? &s->data : nullptr; }
    Model* ResourceManager::Get(ModelHandle handle) { auto* s = Find(models_, handle); return s ? &s->data : nullptr; }
    AudioClip* ResourceManager::Get(AudioHandle handle) { auto* s = Find(audio_, handle); return s ? &s->data : nullptr; }
    // clang-format on

    // Same generation/refCount check as Find(), but returning the slot's displayed path
    std::string ResourceManager::PathOf(ModelHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= models_.slots.size())
        {
            return {};
        }
        const Slot<Model>& slot = models_.slots[handle.index];
        return slot.generation == handle.generation && slot.refCount > 0 ? slot.path : std::string{};
    }

    namespace
    {
        // When a file was saved; a shader's .vert counts with the .frag beside it
        template <class T> std::filesystem::file_time_type SavedAt(const std::filesystem::path& path)
        {
            std::error_code ec;
            auto written = std::filesystem::last_write_time(GltfContainerOf(path), ec);
            if constexpr (std::is_same_v<T, Shader>)
            {
                std::filesystem::path fragment = path;
                fragment.replace_extension(".frag");
                const auto fragmentWritten = std::filesystem::last_write_time(fragment, ec);
                if (!ec && fragmentWritten > written)
                {
                    written = fragmentWritten;
                }
            }
            return written;
        }

        // Shaders and textures are read again when saved (the GPU copies follow, see AssetGpuCache); models, meshes
        // and sounds only when they come back after being removed
        template <class T> constexpr bool kReadAgainWhenSaved = std::is_same_v<T, Shader> || std::is_same_v<T, Texture>;
    }

    std::uint32_t ResourceManager::VersionOf(ShaderHandle handle) const
    {
        return VersionIn(shaders_, handle);
    }

    std::uint32_t ResourceManager::VersionOf(TextureHandle handle) const
    {
        return VersionIn(textures_, handle);
    }

    template <class T> std::uint32_t ResourceManager::VersionIn(const Pool<T>& pool, Handle<T> handle) const
    {
        if (!handle.IsValid() || handle.index >= pool.slots.size())
        {
            return 0;
        }
        const Slot<T>& slot = pool.slots[handle.index];
        return slot.generation == handle.generation && slot.refCount > 0 ? slot.diskReads + 1 : 0;
    }

    // Checking the disk every frame would be slow, so we only look once a second.
    // This is what makes renaming a texture while the game runs turn it magenta.
    void ResourceManager::Update(double deltaSeconds)
    {
        secondsSinceCheck_ += deltaSeconds;
        if (secondsSinceCheck_ >= 1.0)
        {
            secondsSinceCheck_ = 0.0;
            CheckFiles();
        }
    }

    // Same check for all five kinds of asset
    void ResourceManager::CheckFiles()
    {
        CheckFilesIn(textures_);
        CheckFilesIn(meshes_);
        CheckFilesIn(shaders_);
        CheckFilesIn(models_);
        CheckFilesIn(audio_);
    }

    std::vector<AssetInfo> ResourceManager::ListAssets() const
    {
        std::vector<AssetInfo> assets;
        ListIn(meshes_, assets);
        ListIn(models_, assets);
        ListIn(textures_, assets);
        ListIn(shaders_, assets);
        ListIn(audio_, assets);
        return assets;
    }

    // GPU objects can only be deleted on the GL thread, so unloaded assets leave their ids here and the
    // renderer (AssetGpuCache) collects and deletes them later
    std::vector<std::uint32_t> ResourceManager::TakeDeadTextureIds()
    {
        return std::exchange(deadTextureIds_, {});
    }

    std::vector<std::uint32_t> ResourceManager::TakeDeadShaderPrograms()
    {
        return std::exchange(deadShaderPrograms_, {});
    }

    std::vector<std::uint32_t> ResourceManager::TakeDeadMeshIds()
    {
        return std::exchange(deadMeshIds_, {});
    }

    // Shared by LoadTexture/LoadMesh/LoadShader/LoadModel/LoadAudio
    template <class T> Handle<T> ResourceManager::Acquire(Pool<T>& pool, std::string_view path)
    {
        // Normalise the path first so "a/../b.png" and "b.png" count as the same file
        const std::filesystem::path relative = std::filesystem::path(path).lexically_normal();
        const std::filesystem::path fullPath = (rootDir_ / relative).lexically_normal();
        const std::string key = fullPath.generic_string();

        // Already loaded: no disk read, just one more reference. This is the "loaded once, ref count 10" part.
        if (const auto it = pool.byPath.find(key); it != pool.byPath.end())
        {
            Slot<T>& slot = pool.slots[it->second];
            ++slot.refCount;
            return Handle<T>{it->second, slot.generation};
        }

        // New file: reuse a free slot if there is one so the vector doesn't keep growing
        std::uint32_t index = 0;
        if (pool.freeSlots.empty())
        {
            index = static_cast<std::uint32_t>(pool.slots.size());
            pool.slots.emplace_back();
        }
        else
        {
            index = pool.freeSlots.back();
            pool.freeSlots.pop_back();
        }

        Slot<T>& slot = pool.slots[index];
        slot.path = relative.generic_string();
        slot.fullPath = fullPath;
        slot.refCount = 1;
        slot.diskReads = 0;
        pool.byPath.emplace(key, index);

        Reload(slot);
        return Handle<T>{index, slot.generation};
    }

    // (Re)reads the slot's file, or puts the placeholder in if it can't
    template <class T> void ResourceManager::Reload(Slot<T>& slot)
    {
        Discard(slot.data);
        slot.onDisk = Core::FileExists(GltfContainerOf(slot.fullPath)); // "car.glb#0" is in car.glb
        slot.written = SavedAt<T>(slot.fullPath);

        if (auto data = Traits<T>::Load(slot.fullPath))
        {
            slot.data = std::move(*data);
            slot.isPlaceholder = false;
            ++slot.diskReads;
            LOG_INFO(Resource, "Loaded {} from disk", Describe<T>(slot.path));
            return;
        }

        // Missing or broken file: use the placeholder (magenta texture / cube) so the game keeps running
        slot.data = Traits<T>::Placeholder();
        slot.isPlaceholder = true;
        LOG_WARN(Resource, "{}{}, using placeholder", slot.onDisk ? "Could not read " : "Missing ",
                 Describe<T>(slot.path));
    }

    // Empties the data and queues its GPU ids (if any) for deleting on the GL side
    template <class T> void ResourceManager::Discard(T& data)
    {
        if constexpr (std::is_same_v<T, Texture>)
        {
            if (data.glId != 0)
            {
                deadTextureIds_.push_back(data.glId);
            }
        }
        else if constexpr (std::is_same_v<T, Shader>)
        {
            if (data.program != 0)
            {
                deadShaderPrograms_.push_back(data.program);
            }
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            if (data.rendererCacheId != 0)
            {
                deadMeshIds_.push_back(data.rendererCacheId);
            }
        }
        data = T{};
    }

    // nullptr for invalid, released or out-of-date handles
    template <class T> ResourceManager::Slot<T>* ResourceManager::Find(Pool<T>& pool, Handle<T> handle)
    {
        if (!handle.IsValid() || handle.index >= pool.slots.size())
        {
            return nullptr;
        }
        // The generation check catches old handles: if the slot was freed and reused for another file,
        // its generation went up, so the old handle doesn't match anymore
        Slot<T>& slot = pool.slots[handle.index];
        return slot.generation == handle.generation && slot.refCount > 0 ? &slot : nullptr;
    }

    // One less user; unloads the file when nobody uses it anymore
    template <class T> void ResourceManager::ReleaseIn(Pool<T>& pool, Handle<T> handle)
    {
        Slot<T>* slot = Find(pool, handle);
        if (!slot || --slot->refCount > 0)
        {
            return;
        }

        // Last reference gone, so actually unload it
        LOG_INFO(Resource, "Unloaded {}", Describe<T>(slot->path));
        Discard(slot->data);
        pool.byPath.erase(slot->fullPath.generic_string());
        slot->generation = slot->generation == UINT32_MAX ? 1 : slot->generation + 1; // skip 0 (= invalid handle)
        pool.freeSlots.push_back(handle.index);
    }

    // Looks for files that disappeared or came back since the last check
    template <class T> void ResourceManager::CheckFilesIn(Pool<T>& pool)
    {
        for (Slot<T>& slot : pool.slots)
        {
            if (slot.refCount == 0)
            {
                continue;
            }
            const bool onDisk = Core::FileExists(GltfContainerOf(slot.fullPath));
            if (onDisk == slot.onDisk)
            {
                // Still there: a shader or texture saved since it was read is read again
                if constexpr (kReadAgainWhenSaved<T>)
                {
                    if (onDisk && SavedAt<T>(slot.fullPath) != slot.written)
                    {
                        Reload(slot);
                        LOG_INFO(Resource, "{} was saved: read again", Describe<T>(slot.path));
                    }
                }
                continue;
            }

            // The file disappeared (renamed/deleted) -> placeholder. It came back -> load it again.
            if (slot.onDisk)
            {
                slot.onDisk = false;
                Discard(slot.data);
                slot.data = Traits<T>::Placeholder();
                slot.isPlaceholder = true;
                LOG_WARN(Resource, "{} was removed from disk, using placeholder", Describe<T>(slot.path));
            }
            else
            {
                Reload(slot);
            }
        }
    }

    // Only assets that are still in use
    template <class T> void ResourceManager::ListIn(const Pool<T>& pool, std::vector<AssetInfo>& out)
    {
        for (const Slot<T>& slot : pool.slots)
        {
            if (slot.refCount > 0)
            {
                out.push_back(
                    AssetInfo{slot.path, Traits<T>::kType, slot.refCount, slot.diskReads, slot.isPlaceholder});
            }
        }
    }
}
