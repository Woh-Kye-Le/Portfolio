/* !
@file    	    TextureGL.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		29/09/2026
@brief
    Provides the TextureGL class, an OpenGL 2D-texture implementation of the
    abstract Texture base. It supports raw-pixel creation (images are decoded by
    Assets::LoadTextureFromFile), sampler configuration (wrap / filter / mipmap
    generation), per-slot binding / unbinding, and explicit destruction of the
    underlying glTexture name.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Texture/Texture.hpp"

#include <cstdint>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          OpenGL 2D-texture implementation; owns a single GL_TEXTURE_2D name on the GPU.
                    Supports upload via raw pixel data (Create), plus sampler-state
                    configuration and per-unit bind/unbind.
    *************************************************************************/
    class TextureGL final : public Texture
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor that creates an uninitialized TextureGL with no GPU resource.
        *************************************************************************/
        TextureGL() = default;
        /*!***********************************************************************
        @brief          Named constructor that creates an empty TextureGL with the given identifier.
        @param          name          Human-readable name identifier for this texture.
        *************************************************************************/
        explicit TextureGL(std::string name);
        /*!***********************************************************************
        @brief          Destructor that deletes the underlying OpenGL texture if it exists.
        *************************************************************************/
        ~TextureGL() override;

        /*!***********************************************************************
        @brief          Deleted copy constructor (GPU resources have a single owner).
        *************************************************************************/
        TextureGL(const TextureGL&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (GPU resources have a single owner).
        @return         Reference to this TextureGL.
        *************************************************************************/
        TextureGL& operator=(const TextureGL&) = delete;

        /*!***********************************************************************
        @brief          Move constructor that transfers ownership of another TextureGL's GPU resource.
        @param          other         The TextureGL instance whose GPU texture will be transferred.
        *************************************************************************/
        TextureGL(TextureGL&& other) noexcept;
        /*!***********************************************************************
        @brief          Move-assignment operator that transfers ownership of another TextureGL's GPU resource.
        @param          other         The TextureGL instance whose GPU texture will be transferred.
        @return         Reference to the newly assigned TextureGL instance.
        *************************************************************************/
        TextureGL& operator=(TextureGL&& other) noexcept;

        /*!***********************************************************************
        @brief          Allocates a GPU texture with given dimensions and optionally uploads initial pixel data.
        @param          width         Width of the texture in pixels.
        @param          height        Height of the texture in pixels.
        @param          spec          Texture specification describing format and sampler state.
        @param          data          Optional pointer to initial pixel data to upload (may be nullptr).
        @return         True if allocation and optional upload succeeded, false otherwise.
        *************************************************************************/
        bool Create(uint32_t width, uint32_t height, const TextureSpec& spec, const void* data = nullptr) override;
        /*!***********************************************************************
        @brief          Binds the texture to the specified texture unit slot for sampling.
        @param          slot          Zero-based index of the texture unit to bind to.
        *************************************************************************/
        void Bind(uint32_t slot = 0) const override;
        /*!***********************************************************************
        @brief          Unbinds the texture from its currently bound texture unit.
        *************************************************************************/
        void Unbind() const override;

        /*!***********************************************************************
        @brief          Returns the underlying OpenGL texture ID.
        @return         The OpenGL renderer ID (texture name) of this texture.
        *************************************************************************/
        std::uint32_t GetRendererID() const { return rendererID_; }

    private:
        /*!***********************************************************************
        @brief          Destroys the underlying OpenGL texture if it exists, resetting the ID to zero.
        *************************************************************************/
        void Destroy() noexcept;
        /*!***********************************************************************
        @brief          Converts a TextureWrap enumeration to its corresponding OpenGL wrap-mode constant.
        @param          w             TextureWrap value (Repeat / ClampToEdge / MirroredRepeat).
        @return         The GL_REPEAT-family enumerator for use with glTexParameteri.
        *************************************************************************/
        static std::int32_t WrapToGL(TextureWrap w);
        /*!***********************************************************************
        @brief          Converts a TextureFilter enumeration to the matching OpenGL filter constant.
        @param          f             Requested filter mode.
        @param          isMin         True when generating the minification constant, false for magnification.
        @return         The GL_NEAREST/GL_LINEAR-family enumerator for the sampler state.
        *************************************************************************/
        static std::int32_t FilterToGL(TextureFilter f, bool isMin);
        /*!***********************************************************************
        @brief          Returns the OpenGL sized-internal-format constant for a TextureFormat.
        @param          f             The logical pixel format of the texture.
        @return         GL_RGBA8 / GL_RGBA16F / GL_R8 / GL_DEPTH24_STENCIL8 constant family.
        *************************************************************************/
        static std::int32_t FormatToGLInternal(TextureFormat f);
        /*!***********************************************************************
        @brief          Returns the OpenGL pixel-transfer format constant for a TextureFormat.
        @param          f             The logical pixel format of the source data.
        @return         GL_RGBA / GL_RED / GL_DEPTH_STENCIL constant family.
        *************************************************************************/
        static std::int32_t FormatToGL(TextureFormat f);

        std::uint32_t rendererID_ = 0;
    };
}
