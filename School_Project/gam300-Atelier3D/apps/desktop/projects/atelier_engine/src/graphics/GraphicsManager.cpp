/* !
@file    	    GraphicsManager.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements the GraphicsManager class methods, which initialize GLEW and the
    RenderAPI, manage frame lifetime, and delegate rendering configuration to
    the renderer it owns.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/GraphicsManager.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"

// Third-Party Headers
#include <GL/glew.h>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Initializes GLEW, RenderAPI and the renderer if not already initialized.
    *************************************************************************/
    void GraphicsManager::Init()
    {
        if (initialized_)
        {
            return;
        }
        const GLenum glewError = glewInit();
        static_cast<void>(glewError);

        RenderAPI::Init();
        renderer_.Init();
        initialized_ = true;
        frameActive_ = false;
    }

    /*!***********************************************************************
    @brief          Shuts down the renderer and RenderAPI, releasing graphics resources.
    *************************************************************************/
    void GraphicsManager::Shutdown()
    {
        if (!initialized_)
        {
            return;
        }
        if (frameActive_)
        {
            renderer_.EndFrame();
            frameActive_ = false;
        }
        renderer_.Shutdown();
        RenderAPI::Shutdown();
        initialized_ = false;
    }

    /*!***********************************************************************
    @brief          Prepares the graphics pipeline for a new frame with the given viewport dimensions.
    @param          width         Viewport width in pixels.
    @param          height        Viewport height in pixels.
    *************************************************************************/
    void GraphicsManager::BeginFrame(int width, int height)
    {
        BeginFrame(0, 0, width, height);
    }

    /*!***********************************************************************
    @brief          Prepares a new frame drawn into part of the window; the camera's aspect follows the area.
    @param          x             Left edge of the area in pixels.
    @param          y             Bottom edge of the area in pixels (OpenGL convention).
    @param          width         Area width in pixels.
    @param          height        Area height in pixels.
    *************************************************************************/
    void GraphicsManager::BeginFrame(int x, int y, int width, int height)
    {
        if (!initialized_)
        {
            Init();
        }
        RenderAPI::SetViewport(x, y, width, height);
        RenderAPI::EnableDepthTest(true);
        RenderAPI::EnableBlend(true);
        renderer_.BeginFrame(width, height);
        frameActive_ = true;
    }

    /*!***********************************************************************
    @brief          Finalizes rendering for the current frame by delegating to the renderer.
    *************************************************************************/
    void GraphicsManager::EndFrame()
    {
        if (!initialized_ || !frameActive_)
        {
            return;
        }
        renderer_.EndFrame();
        frameActive_ = false;
    }

    /*!***********************************************************************
    @brief          Sets the RGBA color used to clear the framebuffer each frame.
    @param          r             Red channel value in [0,1].
    @param          g             Green channel value in [0,1].
    @param          b             Blue channel value in [0,1].
    @param          a             Alpha channel value in [0,1].
    *************************************************************************/
    void GraphicsManager::SetClearColor(float r, float g, float b, float a) const
    {
        RenderAPI::SetClearColor(r, g, b, a);
    }

    /*!***********************************************************************
    @brief          Clears the color and depth buffers using the current clear color.
    *************************************************************************/
    void GraphicsManager::Clear() const
    {
        RenderAPI::Clear();
    }
}
