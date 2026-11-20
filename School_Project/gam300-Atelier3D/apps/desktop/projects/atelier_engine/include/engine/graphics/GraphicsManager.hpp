/* !
@file    	    GraphicsManager.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Provides the GraphicsManager class which manages the lifetime of the
    graphics subsystem, orchestrates frame begin/end, and exposes high-level
    rendering controls such as clear color and access to the renderer it
    owns.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Graphics Headers
#include "engine/graphics/Renderer/Renderer.hpp"

// Standard Library Headers
#include <cstdint>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          High-level manager that owns the renderer and drives the graphics subsystem lifecycle.
    *************************************************************************/
    class GraphicsManager
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor.
        *************************************************************************/
        GraphicsManager() = default;
        /*!***********************************************************************
        @brief          Deleted copy constructor.
        *************************************************************************/
        GraphicsManager(const GraphicsManager&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator.
        @return         Reference to this GraphicsManager.
        *************************************************************************/
        GraphicsManager& operator=(const GraphicsManager&) = delete;
        /*!***********************************************************************
        @brief          Default move constructor.
        *************************************************************************/
        GraphicsManager(GraphicsManager&&) = default;
        /*!***********************************************************************
        @brief          Default move assignment operator.
        @return         Reference to this GraphicsManager.
        *************************************************************************/
        GraphicsManager& operator=(GraphicsManager&&) = default;

        /*!***********************************************************************
        @brief          Initializes GLEW, RenderAPI and the renderer.
        *************************************************************************/
        void Init();
        /*!***********************************************************************
        @brief          Shuts down the renderer and RenderAPI, releasing graphics resources.
        *************************************************************************/
        void Shutdown();

        /*!***********************************************************************
        @brief          Prepares the graphics pipeline for a new frame with the given viewport dimensions.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        void BeginFrame(int width, int height);

        /*!***********************************************************************
        @brief          Prepares a new frame drawn into part of the window (e.g. an editor viewport panel).
        @param          x             Left edge of the area, in pixels from the window's left.
        @param          y             Bottom edge of the area, in pixels from the window's bottom (OpenGL convention).
        @param          width         Area width in pixels.
        @param          height        Area height in pixels.
        *************************************************************************/
        void BeginFrame(int x, int y, int width, int height);
        /*!***********************************************************************
        @brief          Finalizes rendering for the current frame.
        *************************************************************************/
        void EndFrame();

        /*!***********************************************************************
        @brief          Sets the RGBA color used to clear the framebuffer each frame.
        @param          r             Red channel value in [0,1].
        @param          g             Green channel value in [0,1].
        @param          b             Blue channel value in [0,1].
        @param          a             Alpha channel value in [0,1].
        *************************************************************************/
        void SetClearColor(float r, float g, float b, float a) const;
        /*!***********************************************************************
        @brief          Clears the color and depth buffers using the current clear color.
        *************************************************************************/
        void Clear() const;

        /*!***********************************************************************
        @brief          Returns a const reference to the renderer (2D + 3D sub-renderers and their state).
        @return         Const reference to the Renderer instance.
        *************************************************************************/
        const Renderer::Renderer& GetRenderer() const { return renderer_; }
        /*!***********************************************************************
        @brief          Returns a mutable reference to the renderer (2D + 3D sub-renderers and their state).
        @return         Mutable reference to the Renderer instance.
        *************************************************************************/
        Renderer::Renderer& GetRenderer() { return renderer_; }

        /*!***********************************************************************
        @brief          Checks whether the graphics manager has been initialized.
        @return         True if Init has completed and Shutdown has not been called, otherwise false.
        *************************************************************************/
        bool IsInitialized() const { return initialized_; }

    private:
        Renderer::Renderer renderer_;
        bool initialized_ = false;
        bool frameActive_ = false;
    };
}
