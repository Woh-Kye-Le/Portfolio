/* !
@file    	    TextureGL.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		29/09/2026
@brief
    Implements the TextureGL class methods, which manage an OpenGL GL_TEXTURE_2D
    resource on the GPU. Supports construction with a name, destruction, move
    ownership, format/wrap/filter conversion to GL constants, raw-pixel upload
    through Create. Provides per-texture-unit Bind/Unbind for sampling in
    shaders.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/OpenGL/TextureGL.hpp"

// Third-Party Headers
#include <GL/glew.h>

// Standard Library Headers
#include <utility>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Named constructor that creates an empty TextureGL with the given identifier.
    @param          name          Human-readable name identifier for this texture.
    *************************************************************************/
    TextureGL::TextureGL(std::string name)
    {
        name_ = std::move(name);
    }

    /*!***********************************************************************
    @brief          Destructor that deletes the underlying OpenGL texture if it has been created.
    *************************************************************************/
    TextureGL::~TextureGL()
    {
        Destroy();
    }

    /*!***********************************************************************
    @brief          Move constructor that transfers ownership of another TextureGL's GPU resource.
    @param          other         The TextureGL instance whose GPU texture will be transferred.
    *************************************************************************/
    TextureGL::TextureGL(TextureGL&& other) noexcept
    {
        *this = std::move(other);
    }

    /*!***********************************************************************
    @brief          Move-assignment operator that transfers ownership of another TextureGL's GPU resource.
    @param          other         The TextureGL instance whose GPU texture will be transferred.
    @return         Reference to the newly assigned TextureGL instance.
    *************************************************************************/
    TextureGL& TextureGL::operator=(TextureGL&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            std::swap(rendererID_, other.rendererID_);
            std::swap(width_, other.width_);
            std::swap(height_, other.height_);
            std::swap(spec_, other.spec_);
            std::swap(name_, other.name_);
        }
        return *this;
    }

    /*!***********************************************************************
    @brief          Destroys the underlying OpenGL texture if it exists, resetting the ID to zero.
    *************************************************************************/
    void TextureGL::Destroy() noexcept
    {
        if (rendererID_ != 0)
        {
            const GLuint id = static_cast<GLuint>(rendererID_);
            glDeleteTextures(1, &id);
            rendererID_ = 0;
        }
    }

    /*!***********************************************************************
    @brief          Converts a TextureWrap enumeration to its corresponding OpenGL wrap-mode constant.
    @param          w             TextureWrap value (Repeat / ClampToEdge / MirroredRepeat).
    @return         The GL_REPEAT-family enumerator for use with glTexParameteri.
    *************************************************************************/
    std::int32_t TextureGL::WrapToGL(TextureWrap w)
    {
        switch (w)
        {
        case TextureWrap::Repeat: return GL_REPEAT;
        case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        }
        return GL_CLAMP_TO_EDGE;
    }

    /*!***********************************************************************
    @brief          Converts a TextureFilter enumeration to the matching OpenGL filter constant.
    @param          f             Requested filter mode.
    @param          isMin         True when generating the minification constant, false for magnification.
    @return         The GL_NEAREST/GL_LINEAR-family enumerator for the sampler state.
    *************************************************************************/
    std::int32_t TextureGL::FilterToGL(TextureFilter f, bool isMin)
    {
        switch (f)
        {
        case TextureFilter::Nearest: return GL_NEAREST;
        case TextureFilter::Linear: return GL_LINEAR;
        case TextureFilter::NearestMipmapNearest: return isMin ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
        case TextureFilter::LinearMipmapLinear: return isMin ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
        }
        return GL_LINEAR;
    }

    /*!***********************************************************************
    @brief          Returns the OpenGL sized-internal-format constant for a TextureFormat.
    @param          f             The logical pixel format of the texture.
    @return         GL_RGBA8 / GL_RGBA16F / GL_R8 / GL_DEPTH24_STENCIL8 constant family.
    *************************************************************************/
    std::int32_t TextureGL::FormatToGLInternal(TextureFormat f)
    {
        switch (f)
        {
        case TextureFormat::RGBA8: return GL_RGBA8;
        case TextureFormat::RGBA16F: return GL_RGBA16F;
        case TextureFormat::R8: return GL_R8;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        }
        return GL_RGBA8;
    }

    /*!***********************************************************************
    @brief          Returns the OpenGL pixel-transfer format constant for a TextureFormat.
    @param          f             The logical pixel format of the source data.
    @return         GL_RGBA / GL_RED / GL_DEPTH_STENCIL constant family.
    *************************************************************************/
    std::int32_t TextureGL::FormatToGL(TextureFormat f)
    {
        switch (f)
        {
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA16F: return GL_RGBA;
        case TextureFormat::R8: return GL_RED;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH_STENCIL;
        }
        return GL_RGBA;
    }

    /*!***********************************************************************
    @brief          Allocates a GPU texture with given dimensions and optionally uploads initial pixel data.
    @param          width         Width of the texture in pixels.
    @param          height        Height of the texture in pixels.
    @param          spec          Texture specification describing format and sampler state.
    @param          data          Optional pointer to initial pixel data to upload (may be nullptr).
    @return         True if allocation and optional upload succeeded, false otherwise.
    *************************************************************************/
    bool TextureGL::Create(uint32_t width, uint32_t height, const TextureSpec& spec, const void* data)
    {
        Destroy();
        width_ = width;
        height_ = height;
        spec_ = spec;

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, WrapToGL(spec.wrapS));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, WrapToGL(spec.wrapT));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FilterToGL(spec.minFilter, true));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FilterToGL(spec.magFilter, false));
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, FormatToGLInternal(spec.format), static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), 0, FormatToGL(spec.format),
                     spec.format == TextureFormat::Depth24Stencil8 ? GL_UNSIGNED_INT_24_8 : GL_UNSIGNED_BYTE, data);
        if (spec.hasMipmaps)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        rendererID_ = static_cast<std::uint32_t>(id);
        return rendererID_ != 0;
    }

    /*!***********************************************************************
    @brief          Binds this texture to the specified texture unit slot for sampling.
    @param          slot          Zero-based index of the texture unit to bind to.
    *************************************************************************/
    void TextureGL::Bind(uint32_t slot) const
    {
        if (rendererID_ == 0)
        {
            return;
        }
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, rendererID_);
        // Unit 0 is left active again: code that binds without choosing a unit (ImGui's renderer, texture creation)
        // would otherwise land on this unit and replace a model's normal map, or draw the UI with the wrong texture
        if (slot != 0)
        {
            glActiveTexture(GL_TEXTURE0);
        }
    }

    /*!***********************************************************************
    @brief          Unbinds the current GL_TEXTURE_2D target from the OpenGL context.
    *************************************************************************/
    void TextureGL::Unbind() const
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
