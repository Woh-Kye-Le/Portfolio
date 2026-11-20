/* !
@file    	    View2D.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		30/09/2026
@brief
    View2D, the camera of the Viewport's 2D front view: world/screen conversion,
    pan and zoom, and the adaptive grid spacing, shared by drawing, picking and
    dragging so everything stays lined up.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace Editor::Camera
{
    // The camera of the Viewport's 2D view, which looks at the scene from the front: world X to the right, world Y up.
    // It is a world point shown at the centre of the view plus a zoom (screen units per world unit).
    //
    // Every conversion between the world and the screen goes through WorldToScreen / ScreenToWorld: the grid, the
    // axis lines, objects, picking, dragging and snapping all use them, so they stay lined up at any zoom, pan or view
    // size. Screen positions are in ImGui's screen coordinates, the units of its mouse position and draw lists (the
    // editor scales the UI for the display's DPI through ImGui's style and fonts, so these are window pixels).
    // Resizing the view keeps the same world point at its centre.
    // Header-only so the unit tests can use it without the editor executable.
    class View2D
    {
    public:
        static constexpr float kDefaultZoom = 48.0f;
        static constexpr float kMinZoom = 2.0f;
        static constexpr float kMaxZoom = 4000.0f;

        // Where the view is on screen this frame: its top-left corner and size
        void SetRect(const glm::vec2& position, const glm::vec2& size)
        {
            rectPosition_ = position;
            rectSize_ = glm::max(size, glm::vec2(1.0f));
        }

        glm::vec2 RectPosition() const { return rectPosition_; }
        glm::vec2 RectSize() const { return rectSize_; }
        glm::vec2 RectCenter() const { return rectPosition_ + rectSize_ * 0.5f; }

        glm::vec2 WorldToScreen(const glm::vec2& world) const
        {
            const glm::vec2 offset = (world - center_) * zoom_;
            const glm::vec2 screenCenter = RectCenter();
            return {screenCenter.x + offset.x, screenCenter.y - offset.y}; // screen Y grows downwards
        }

        glm::vec2 ScreenToWorld(const glm::vec2& screen) const
        {
            const glm::vec2 screenCenter = RectCenter();
            return center_ + glm::vec2(screen.x - screenCenter.x, screenCenter.y - screen.y) / zoom_;
        }

        // The world rectangle the view shows
        glm::vec2 VisibleMin() const { return ScreenToWorld({rectPosition_.x, rectPosition_.y + rectSize_.y}); }
        glm::vec2 VisibleMax() const { return ScreenToWorld({rectPosition_.x + rectSize_.x, rectPosition_.y}); }

        // Slides the view so the world moves with the mouse (screenDelta: the mouse movement)
        void Pan(const glm::vec2& screenDelta) { center_ -= glm::vec2(screenDelta.x, -screenDelta.y) / zoom_; }

        // Zooms in (factor > 1) or out, keeping the world point under screenPoint where it is on screen
        void ZoomAt(const glm::vec2& screenPoint, float factor)
        {
            const glm::vec2 anchor = ScreenToWorld(screenPoint);
            zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
            center_ += anchor - ScreenToWorld(screenPoint);
        }

        void SetCenter(const glm::vec2& world) { center_ = world; }
        glm::vec2 Center() const { return center_; }
        void SetZoom(float zoom) { zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom); }
        float Zoom() const { return zoom_; }

        void Reset()
        {
            center_ = glm::vec2(0.0f);
            zoom_ = kDefaultZoom;
        }

        // World distance between grid lines: 1, 2 or 5 times a power of ten, the smallest that keeps the lines at
        // least minScreenSpacing apart on screen. Grid lines sit at whole multiples of it, so one always passes
        // through the origin.
        float GridStep(float minScreenSpacing) const
        {
            const float minWorld = std::max(minScreenSpacing, 1e-3f) / zoom_;
            const float power = std::pow(10.0f, std::floor(std::log10(minWorld)));
            for (const float multiple : {1.0f, 2.0f, 5.0f, 10.0f})
            {
                if (power * multiple >= minWorld)
                {
                    return power * multiple;
                }
            }
            return power * 10.0f;
        }

        // The nearest grid point (a whole multiple of step on each axis)
        static glm::vec2 Snap(const glm::vec2& world, float step) { return glm::round(world / step) * step; }

    private:
        glm::vec2 rectPosition_{0.0f};
        glm::vec2 rectSize_{1.0f};
        glm::vec2 center_{0.0f};
        float zoom_ = kDefaultZoom;
    };
}
