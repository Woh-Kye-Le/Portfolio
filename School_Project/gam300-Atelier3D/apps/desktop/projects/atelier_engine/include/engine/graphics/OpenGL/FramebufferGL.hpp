/* !
@file    	    FramebufferGL.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		29/09/2026
@brief
    Provides the FramebufferGL class, a FBO with a color attachment (RGBA8
    TextureGL) and a combined depth-stencil attachment (Depth24Stencil8
    TextureGL).  Used as an offscreen render target for editor viewports; the
    resulting color texture is then blitted into an ImGui panel via
    ImGui::Image.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/OpenGL/TextureGL.hpp"

#include <cstdint>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          A complete framebuffer object owning one color and one depth-stencil
                    texture attachment, both sized to the viewport extents of the
                    caller (for example, an ImGui panel content region).
    *************************************************************************/
    class FramebufferGL final
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor that creates a zero-sized, unallocated FBO.
        *************************************************************************/
        FramebufferGL() = default;
        /*!***********************************************************************
        @brief          Constructs and immediately allocates an FBO sized (w,h).
        @param          width         Initial color-buffer width in pixels (>0).
        @param          height        Initial color-buffer height in pixels (>0).
        *************************************************************************/
        FramebufferGL(int width, int height);
        /*!***********************************************************************
        @brief          Destructor that releases the FBO and both texture attachments.
        *************************************************************************/
        ~FramebufferGL();

        /*!***********************************************************************
        @brief          Deleted copy constructor (GPU resources have a single owner).
        *************************************************************************/
        FramebufferGL(const FramebufferGL&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (GPU resources have a single owner).
        @return         Reference to this FramebufferGL.
        *************************************************************************/
        FramebufferGL& operator=(const FramebufferGL&) = delete;

        /*!***********************************************************************
        @brief          Move constructor that transfers ownership of another FBO's GPU resources.
        @param          other         FramebufferGL whose resources are taken over.
        *************************************************************************/
        FramebufferGL(FramebufferGL&& other) noexcept;
        /*!***********************************************************************
        @brief          Move-assignment that transfers ownership of another FBO's GPU resources.
        @param          other         FramebufferGL whose resources are taken over.
        @return         Reference to this FramebufferGL.
        *************************************************************************/
        FramebufferGL& operator=(FramebufferGL&& other) noexcept;

        /*!***********************************************************************
        @brief          (Re-)allocates the FBO and its two attachments to the given pixel size.
                        Safe to call on an already-allocated FBO (old resources are destroyed first).
        @param          width         New color-buffer width in pixels (>0).
        @param          height        New color-buffer height in pixels (>0).
        @return         True if the FBO completed successfully (GL_FRAMEBUFFER_COMPLETE).
        *************************************************************************/
        bool Resize(int width, int height);

        /*!***********************************************************************
        @brief          Binds this FBO as the GL_FRAMEBUFFER target so subsequent draws
                        land in its attachments.
        *************************************************************************/
        void Bind() const;
        /*!***********************************************************************
        @brief          Binds FBO id 0, returning rendering to the default backbuffer.
        *************************************************************************/
        void Unbind() const;

        /*!***********************************************************************
        @brief          Returns the raw GL name of the color-attachment texture,
                        suitable for casting to (ImTextureID) for ImGui::Image.
        @return         OpenGL texture name of the RGBA8 color buffer.
        *************************************************************************/
        std::uint32_t GetColorTextureID() const { return colorTex_.GetRendererID(); }

        /*!***********************************************************************
        @brief          Current color-buffer width in pixels (0 when unallocated).
        @return         Width of color attachment in pixels.
        *************************************************************************/
        int Width() const { return width_; }
        /*!***********************************************************************
        @brief          Current color-buffer height in pixels (0 when unallocated).
        @return         Height of color attachment in pixels.
        *************************************************************************/
        int Height() const { return height_; }
        /*!***********************************************************************
        @brief          True after a successful Resize / sized constructor and before
                        destruction / move.
        @return         True if the FBO owns a valid GL framebuffer name.
        *************************************************************************/
        bool IsValid() const { return fboID_ != 0; }

    private:
        /*!***********************************************************************
        @brief          Destroys the GL FBO and its attachments if allocated.
        *************************************************************************/
        void Destroy() noexcept;

        std::uint32_t fboID_ = 0;
        int width_ = 0;
        int height_ = 0;
        TextureGL colorTex_;
        TextureGL depthTex_;
    };
}
