/* !
@file    	    FramebufferGL.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		29/09/2026
@brief
    Implements the FramebufferGL class methods, which manage an OpenGL FBO
    complete with one RGBA8 color attachment (TextureGL) and one combined depth-
    stencil attachment (Depth24Stencil8 TextureGL).  The constructor with
    dimensions calls Resize; Resize tears down any existing resources before
    building the new FBO and glCheckFramebufferStatus-verifying it.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/OpenGL/FramebufferGL.hpp"
#include "engine/core/Log.hpp"
#include "engine/graphics/Texture/Texture.hpp"

// Third-Party Headers
#include <GL/glew.h>

// Standard Library Headers
#include <utility>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Constructs and immediately allocates an FBO sized (w,h).
    @param          width         Initial color-buffer width in pixels (>0).
    @param          height        Initial color-buffer height in pixels (>0).
    *************************************************************************/
    FramebufferGL::FramebufferGL(int width, int height)
    {
        if (width > 0 && height > 0)
        {
            Resize(width, height);
        }
    }

    /*!***********************************************************************
    @brief          Destructor that releases the FBO and both texture attachments.
    *************************************************************************/
    FramebufferGL::~FramebufferGL()
    {
        Destroy();
    }

    /*!***********************************************************************
    @brief          Move constructor that transfers ownership of another FBO's GPU resources.
    @param          other         FramebufferGL whose resources are taken over.
    *************************************************************************/
    FramebufferGL::FramebufferGL(FramebufferGL&& other) noexcept
        : fboID_(0), width_(0), height_(0), colorTex_(), depthTex_()
    {
        *this = std::move(other);
    }

    /*!***********************************************************************
    @brief          Move-assignment that transfers ownership of another FBO's GPU resources.
    @param          other         FramebufferGL whose resources are taken over.
    @return         Reference to this FramebufferGL.
    *************************************************************************/
    FramebufferGL& FramebufferGL::operator=(FramebufferGL&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            std::swap(fboID_, other.fboID_);
            std::swap(width_, other.width_);
            std::swap(height_, other.height_);
            colorTex_ = std::move(other.colorTex_);
            depthTex_ = std::move(other.depthTex_);
        }
        return *this;
    }

    /*!***********************************************************************
    @brief          Destroys the GL FBO and its attachments if allocated.
    *************************************************************************/
    void FramebufferGL::Destroy() noexcept
    {
        if (fboID_ != 0)
        {
            const GLuint id = static_cast<GLuint>(fboID_);
            glDeleteFramebuffers(1, &id);
            fboID_ = 0;
        }
        width_ = 0;
        height_ = 0;
    }

    /*!***********************************************************************
    @brief          (Re-)allocates the FBO and its two attachments to the given pixel size.
    @param          width         New color-buffer width in pixels (>0).
    @param          height        New color-buffer height in pixels (>0).
    @return         True if the FBO completed successfully (GL_FRAMEBUFFER_COMPLETE).
    *************************************************************************/
    bool FramebufferGL::Resize(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            Destroy();
            return false;
        }

        if (glGenFramebuffers == nullptr)
        {
            Destroy();
            width_ = width;
            height_ = height;
            return false;
        }

        Destroy();

        width_ = width;
        height_ = height;

        // --- Create the FBO itself --------------------------------------------------
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // --- Color attachment: RGBA8, Linear, Clamp ---------------------------------
        {
            TextureSpec spec{};
            spec.format = TextureFormat::RGBA8;
            spec.minFilter = TextureFilter::Linear;
            spec.magFilter = TextureFilter::Linear;
            spec.wrapS = TextureWrap::ClampToEdge;
            spec.wrapT = TextureWrap::ClampToEdge;
            spec.hasMipmaps = false;
            spec.width = static_cast<uint32_t>(width);
            spec.height = static_cast<uint32_t>(height);
            colorTex_.Create(spec.width, spec.height, spec, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   static_cast<GLuint>(colorTex_.GetRendererID()), 0);
        }

        // --- Depth + Stencil attachment: Depth24Stencil8, Clamp ---------------------
        {
            TextureSpec spec{};
            spec.format = TextureFormat::Depth24Stencil8;
            spec.minFilter = TextureFilter::Nearest;
            spec.magFilter = TextureFilter::Nearest;
            spec.wrapS = TextureWrap::ClampToEdge;
            spec.wrapT = TextureWrap::ClampToEdge;
            spec.hasMipmaps = false;
            spec.width = static_cast<uint32_t>(width);
            spec.height = static_cast<uint32_t>(height);
            depthTex_.Create(spec.width, spec.height, spec, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   static_cast<GLuint>(depthTex_.GetRendererID()), 0);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            LOG_ERROR(Graphics, "FramebufferGL: incomplete FBO (status={:#x}, size={}x{})",
                      static_cast<unsigned>(status), width, height);
            Destroy();
            return false;
        }

        fboID_ = static_cast<std::uint32_t>(fbo);
        return true;
    }

    /*!***********************************************************************
    @brief          Binds this FBO as the GL_FRAMEBUFFER target so subsequent draws
                    land in its attachments.
    *************************************************************************/
    void FramebufferGL::Bind() const
    {
        if (fboID_ != 0 && glBindFramebuffer != nullptr)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fboID_));
        }
    }

    /*!***********************************************************************
    @brief          Binds FBO id 0, returning rendering to the default backbuffer.
    *************************************************************************/
    void FramebufferGL::Unbind() const
    {
        if (glBindFramebuffer != nullptr)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
}
