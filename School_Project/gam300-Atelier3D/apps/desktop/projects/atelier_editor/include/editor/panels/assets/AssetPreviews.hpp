/* !
@file    	    AssetPreviews.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    Declares AssetPreviews, the GPU pictures the Assets browser shows for its
    images and models: each image uploaded as a texture, each model rendered
    once into its own offscreen framebuffer.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/assets/AssetCatalog.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Graphics
{
    class Shader;
}

namespace Engine::Graphics::OpenGL
{
    class FramebufferGL;
    class TextureGL;
}

namespace Editor::Panels
{
    /*!
    \brief  Previews of a game's images and models, made the first time they are asked for and kept until
            the file changes or disappears. Making one reads the file and uploads (or renders) it, so only
            a few are made per frame and the rest follow on the next frames. Needs the editor's OpenGL
            context: call Clear before the context goes away.
    */
    class AssetPreviews
    {
    public:
        // A finished preview: an OpenGL texture stored bottom row first (draw it with uv0 = (0, 1), uv1 = (1, 0))
        struct Preview
        {
            std::uint32_t textureId = 0; // 0 when the file could not be read
            int width = 0;               // the picture's size in pixels
            int height = 0;
            std::string details; // "512 x 512", "8 vertices, 12 triangles", or why the file could not be read
        };

        AssetPreviews();
        ~AssetPreviews();
        AssetPreviews(const AssetPreviews&) = delete;
        AssetPreviews& operator=(const AssetPreviews&) = delete;

        /*!
        \brief   Starts a frame: allows kMaxNewPerFrame new previews to be made before the next call. The
                 Assets window can be drawn before anything has loaded OpenGL's functions (the Viewport does
                 that on its first render), so this loads them first and makes none until they are there.
        */
        void BeginFrame();

        /*!
        \brief   The preview of an image or model, made now if this frame's budget allows.
        \param   projectDir The game folder asset.path is relative to.
        \param   asset A model or texture (audio has no preview).
        \return  The preview, or nullptr while it waits for a later frame (and always for audio).
        */
        const Preview* Get(const std::filesystem::path& projectDir, const AssetEntry& asset);

        /*!
        \brief   Drops the previews of files that changed on disk or are no longer listed, so they are made
                 again from the new file.
        \param   projectDir The game folder.
        \param   assets The assets the browser lists now.
        */
        void Prune(const std::filesystem::path& projectDir, const std::vector<AssetEntry>& assets);

        // Frees every preview and the model shader (for a different game, or before the GL context goes)
        void Clear();

    private:
        static constexpr int kMaxNewPerFrame = 2;
        static constexpr int kModelPreviewSize = 256;

        struct Entry
        {
            Preview preview;
            std::filesystem::file_time_type writeTime{};
            std::unique_ptr<Engine::Graphics::OpenGL::TextureGL> texture;    // images
            std::unique_ptr<Engine::Graphics::OpenGL::FramebufferGL> render; // models
        };

        /*!
        \brief   Reads an image and uploads it as a mipmapped texture.
        \param   file The image file.
        \param   entry Where the texture and its details go.
        */
        static void MakeTexturePreview(const std::filesystem::path& file, Entry& entry);

        /*!
        \brief   Reads a model and renders it once, lit from above, into a framebuffer of its own: the
                 camera looks at it from the front right, far enough back to see all of it.
        \param   file The model file.
        \param   entry Where the framebuffer and the model's details go.
        */
        void MakeModelPreview(const std::filesystem::path& file, Entry& entry);

        // The model shader, compiled on first use; nullptr if it does not compile
        const Engine::Graphics::Shader* ModelShader();

        std::unordered_map<std::string, Entry> entries_; // by asset path
        std::unique_ptr<Engine::Graphics::Shader> modelShader_;
        bool modelShaderFailed_ = false;
        int budget_ = 0;
    };
}
