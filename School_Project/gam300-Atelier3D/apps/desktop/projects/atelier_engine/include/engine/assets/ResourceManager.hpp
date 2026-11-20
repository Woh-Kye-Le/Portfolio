/* !
@file    	    ResourceManager.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares ResourceManager, the handle-based asset cache: each file is read
    from disk once, shared by reference count, released when unused, and
    replaced by a placeholder when missing.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/Resources.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Engine::Assets
{
    // The five kinds of file the ResourceManager can load
    enum class AssetType : std::uint8_t
    {
        Texture,
        Mesh,
        Shader,
        Model,
        Audio
    };

    const char* ToString(AssetType type);

    // Index into the manager's slot table plus the slot's generation when the handle was made.
    // Freeing a slot bumps its generation, so old handles to it stop resolving instead of pointing at a reused slot.
    template <class T> struct Handle
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0; // 0 = never valid

        bool IsValid() const { return generation != 0; }
    };

    using TextureHandle = Handle<Texture>;
    using MeshHandle = Handle<Mesh>;
    using ShaderHandle = Handle<Shader>;
    using ModelHandle = Handle<Model>;
    using AudioHandle = Handle<AudioClip>;

    // One row of the editor's Assets window
    struct AssetInfo
    {
        std::string path;
        AssetType type = AssetType::Texture;
        std::uint32_t refCount = 0;
        std::uint32_t diskReads = 0;
        bool isPlaceholder = false;
    };

    // Loads assets by path and shares them: loading a path that is already loaded only adds a reference,
    // so a file used by many objects is read from disk once. Release() drops a reference and frees the asset
    // when none are left. Missing/broken files get a placeholder (magenta texture, cube, ...) and a warning.
    class ResourceManager
    {
    public:
        // Relative paths are resolved against rootDir (a game project folder)
        explicit ResourceManager(std::filesystem::path rootDir = {});
        const std::filesystem::path& RootDir() const { return rootDir_; }

        TextureHandle LoadTexture(std::string_view path);
        MeshHandle LoadMesh(std::string_view path);
        ShaderHandle LoadShader(std::string_view path);
        ModelHandle LoadModel(std::string_view path);
        AudioHandle LoadAudio(std::string_view path);

        void AddRef(TextureHandle handle);
        void AddRef(MeshHandle handle);
        void AddRef(ShaderHandle handle);
        void AddRef(ModelHandle handle);
        void AddRef(AudioHandle handle);

        void Release(TextureHandle handle);
        void Release(MeshHandle handle);
        void Release(ShaderHandle handle);
        void Release(ModelHandle handle);
        void Release(AudioHandle handle);

        // nullptr for stale or invalid handles. Never null for a live handle (placeholders included).
        Texture* Get(TextureHandle handle);
        Mesh* Get(MeshHandle handle);
        Shader* Get(ShaderHandle handle);
        Model* Get(ModelHandle handle);
        AudioClip* Get(AudioHandle handle);

        // The path a live handle was loaded under, as displayed ("" for a stale handle).
        // Lets a caller resolve files that sit next to an asset, e.g. a model's textures.
        std::string PathOf(ModelHandle handle) const;

        // How many times a live handle's file has been read (0 for a stale handle): it goes up when a saved shader or
        // texture is read again, so what was made from it on the GPU knows to be made again
        std::uint32_t VersionOf(ShaderHandle handle) const;
        std::uint32_t VersionOf(TextureHandle handle) const;

        // Calls CheckFiles() about once a second
        void Update(double deltaSeconds);
        // Swaps assets whose file disappeared to their placeholder, and reloads ones whose file came back
        void CheckFiles();

        std::vector<AssetInfo> ListAssets() const;

        // GPU objects of freed/replaced assets, for the MeshRenderer to delete (the manager itself never calls OpenGL)
        std::vector<std::uint32_t> TakeDeadTextureIds();
        std::vector<std::uint32_t> TakeDeadShaderPrograms();
        std::vector<std::uint32_t> TakeDeadMeshIds();

    private:
        template <class T> struct Slot
        {
            std::string path; // as displayed, e.g. "assets/models/crate/crate.png"
            std::filesystem::path fullPath;
            T data{};
            std::uint32_t refCount = 0;
            std::uint32_t generation = 1;
            std::uint32_t diskReads = 0;
            bool isPlaceholder = false;
            bool onDisk = false;                       // file existed at the last check
            std::filesystem::file_time_type written{}; // when its file (both of a shader's) was saved, when read
        };

        template <class T> struct Pool
        {
            std::vector<Slot<T>> slots;
            std::vector<std::uint32_t> freeSlots;
            std::unordered_map<std::string, std::uint32_t> byPath; // fullPath -> slot index
        };

        template <class T> Handle<T> Acquire(Pool<T>& pool, std::string_view path);
        template <class T> void Reload(Slot<T>& slot);
        template <class T> void Discard(T& data);
        template <class T> Slot<T>* Find(Pool<T>& pool, Handle<T> handle);
        template <class T> void ReleaseIn(Pool<T>& pool, Handle<T> handle);
        template <class T> void CheckFilesIn(Pool<T>& pool);
        template <class T> std::uint32_t VersionIn(const Pool<T>& pool, Handle<T> handle) const;
        template <class T> static void ListIn(const Pool<T>& pool, std::vector<AssetInfo>& out);

        std::filesystem::path rootDir_;
        Pool<Texture> textures_;
        Pool<Mesh> meshes_;
        Pool<Shader> shaders_;
        Pool<Model> models_;
        Pool<AudioClip> audio_;
        std::vector<std::uint32_t> deadTextureIds_;
        std::vector<std::uint32_t> deadShaderPrograms_;
        std::vector<std::uint32_t> deadMeshIds_;
        double secondsSinceCheck_ = 0.0;
    };
}
