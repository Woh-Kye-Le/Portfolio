/* !
@file    	    EditorCamera.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		22/09/2026
@brief
    EditorCamera, the editor's fly/orbit camera: right mouse + WASD to fly, Alt
    + drag to orbit, middle drag to pan and wheel to zoom, with smoothed
    movement. Kept separate from the game camera.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/input/GameInput.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace Editor::Camera
{
    // The editor's viewport camera, controlled like a commercial engine's scene view:
    //   Right mouse (Look) + drag        look around from where the camera is
    //   Right mouse + WASD / Q E         fly (MoveForward / MoveBack / MoveLeft / MoveRight / MoveDown / MoveUp);
    //                                    Shift (MoveFast) 3x faster; the scroll wheel changes the fly speed
    //   Alt + left mouse drag            orbit around the pivot (the point the camera looks at)
    //   Middle mouse drag                pan: slide the view and the pivot sideways / up and down
    //   Alt + right mouse drag           zoom in / out
    //   Scroll wheel                     zoom towards / away from the pivot
    //   Focus()  (F in the editor)       frame an object;  SetView()  snap to Front / Top / ... (view gizmo)
    // Movement is eased (see SetSmoothing), so the camera glides instead of jumping. The fly keys come from named
    // actions in the game's configs/input_actions.json; everything is read through a GameInput, so keys do nothing
    // while typing in an editor text field and the mouse does nothing while it is over an editor panel.
    // Header-only so the unit tests can use it without the editor executable.
    class EditorCamera
    {
    public:
        static constexpr float kLookDegreesPerPixel = 0.2f;
        static constexpr float kOrbitDegreesPerPixel = 0.3f;
        static constexpr float kFastMultiplier = 3.0f;
        static constexpr float kMinSpeed = 0.5f;
        static constexpr float kMaxSpeed = 50.0f;
        static constexpr float kMaxPitch = 89.0f;
        static constexpr float kMinDistance = 0.3f;
        static constexpr float kMaxDistance = 2000.0f;
        static constexpr float kZoomPerScrollStep = 0.85f;
        static constexpr float kStartDistance = 5.0f;
        static constexpr float kFieldOfViewDegrees = 60.0f; // matches Renderer's camera, for panning at mouse speed
        // How quickly the eased camera catches up (per second): higher is snappier
        static constexpr float kSmoothing = 18.0f;

        // Takes over a camera: starts from its current position and view direction, pivoting on a point in front
        void Attach(const Engine::Graphics::Camera3D& camera)
        {
            const glm::vec3 forward = glm::normalize(camera.GetForward());
            target_.yaw = glm::degrees(std::atan2(forward.x, -forward.z));
            target_.pitch = glm::degrees(std::asin(std::clamp(forward.y, -1.0f, 1.0f)));
            target_.distance = kStartDistance;
            target_.pivot = camera.GetPosition() + forward * kStartDistance;
            current_ = target_;
            attached_ = true;
        }

        void Detach() { attached_ = false; }
        bool IsAttached() const { return attached_; }

        // Eased movement (the default) or immediate (tests, and anything that needs exact positions)
        void SetSmoothing(bool enabled) { smoothing_ = enabled; }
        // Height of the view in pixels, so middle-mouse panning moves the scene with the mouse
        void SetViewportHeight(float pixels) { viewportHeight_ = std::max(1.0f, pixels); }

        // Moves and turns from this frame's input and applies the result to camera
        void Update(double deltaSeconds, const Engine::Input::GameInput& input, Engine::Graphics::Camera3D& camera)
        {
            namespace Action = Engine::Input::Action;
            using Engine::Input::Mouse_Left;
            using Engine::Input::Mouse_Middle;
            if (!attached_)
            {
                Attach(camera);
            }

            const Engine::Input::Vec2 delta = input.GetMouseDelta();
            const bool alt = input.IsAltDown();
            const bool look = input.IsActionDown(Action::Look);
            flying_ = look && !alt;

            if (alt && input.IsMouseButtonDown(Mouse_Left))
            {
                // Orbit: turn around the pivot
                target_.yaw += delta.x * kOrbitDegreesPerPixel;
                target_.pitch = std::clamp(target_.pitch - delta.y * kOrbitDegreesPerPixel, -kMaxPitch, kMaxPitch);
            }
            else if (alt && look)
            {
                // Zoom by dragging
                Zoom(std::exp(-delta.x * 0.01f + delta.y * 0.01f));
            }
            else if (flying_)
            {
                // Look around from where the camera is: turn, then put the pivot back in front of it
                const glm::vec3 eye = EyeOf(target_);
                target_.yaw += delta.x * kLookDegreesPerPixel;
                target_.pitch = std::clamp(target_.pitch - delta.y * kLookDegreesPerPixel, -kMaxPitch, kMaxPitch);
                target_.pivot = eye + ForwardOf(target_) * target_.distance;
            }
            else if (input.IsMouseButtonDown(Mouse_Middle))
            {
                // Pan: the point under the mouse follows the mouse
                const float worldPerPixel =
                    2.0f * current_.distance * std::tan(glm::radians(kFieldOfViewDegrees) * 0.5f) / viewportHeight_;
                target_.pivot += (-RightOf(target_) * delta.x + UpOf(target_) * delta.y) * worldPerPixel;
            }

            const float scroll = input.GetScrollDelta();
            if (scroll != 0.0f)
            {
                if (flying_)
                {
                    speed_ = std::clamp(speed_ * std::pow(1.25f, scroll), kMinSpeed, kMaxSpeed);
                }
                else
                {
                    Zoom(std::pow(kZoomPerScrollStep, scroll));
                }
            }

            if (flying_)
            {
                // Fly: the whole view (camera and pivot) moves
                const auto axis = [&input](std::string_view positive, std::string_view negative)
                { return (input.IsActionDown(positive) ? 1.0f : 0.0f) - (input.IsActionDown(negative) ? 1.0f : 0.0f); };
                const float multiplier = input.IsActionDown(Action::MoveFast) ? kFastMultiplier : 1.0f;
                const float step = speed_ * multiplier * static_cast<float>(deltaSeconds);
                target_.pivot += ForwardOf(target_) * (axis(Action::MoveForward, Action::MoveBack) * step);
                target_.pivot += RightOf(target_) * (axis(Action::MoveRight, Action::MoveLeft) * step);
                target_.pivot.y += axis(Action::MoveUp, Action::MoveDown) * step;
            }

            Ease(static_cast<float>(deltaSeconds));
            Apply(camera);
        }

        // Frames a point: looks at center from far enough away to see a sphere of the given radius
        void Focus(const glm::vec3& center, float radius)
        {
            target_.pivot = center;
            const float halfFov = glm::radians(kFieldOfViewDegrees) * 0.5f;
            target_.distance =
                std::clamp(std::max(radius, 0.1f) / std::sin(halfFov) * 1.2f, kMinDistance, kMaxDistance);
        }

        // Turns around the pivot by a mouse movement in pixels (as Alt + left drag does; the view cube uses it too)
        void Orbit(float dxPixels, float dyPixels)
        {
            target_.yaw += dxPixels * kOrbitDegreesPerPixel;
            target_.pitch = std::clamp(target_.pitch - dyPixels * kOrbitDegreesPerPixel, -kMaxPitch, kMaxPitch);
        }

        // Turns to look from a direction (the view gizmo's Front / Back / Left / Right / Top / Bottom), keeping the
        // pivot. Takes the short way round.
        void SetView(float yawDegrees, float pitchDegrees)
        {
            const float turns = std::round((target_.yaw - yawDegrees) / 360.0f);
            target_.yaw = yawDegrees + turns * 360.0f;
            target_.pitch = std::clamp(pitchDegrees, -kMaxPitch, kMaxPitch);
        }

        // Yaw 0 looks down -Z; positive yaw turns right, positive pitch looks up
        glm::vec3 Forward() const { return ForwardOf(current_); }
        glm::vec3 Position() const { return EyeOf(current_); }
        const glm::vec3& Pivot() const { return current_.pivot; }
        float Distance() const { return current_.distance; }
        float YawDegrees() const { return current_.yaw; }
        float PitchDegrees() const { return current_.pitch; }
        float Speed() const { return speed_; }
        // True while the right mouse button flies the camera (the editor then leaves W / E / R to the fly keys)
        bool IsFlying() const { return flying_; }

    private:
        // The camera orbits around pivot at distance, looking in the yaw/pitch direction. We keep a target View
        // and a current one and ease current towards target every frame, which is what makes it glide.
        struct View
        {
            glm::vec3 pivot{0.0f};
            float yaw = 0.0f;
            float pitch = 0.0f;
            float distance = kStartDistance;
        };

        static glm::vec3 ForwardOf(const View& view)
        {
            const float yaw = glm::radians(view.yaw);
            const float pitch = glm::radians(view.pitch);
            return glm::normalize(
                glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)));
        }
        static glm::vec3 RightOf(const View& view)
        {
            return glm::normalize(glm::cross(ForwardOf(view), glm::vec3(0.0f, 1.0f, 0.0f)));
        }
        static glm::vec3 UpOf(const View& view) { return glm::cross(RightOf(view), ForwardOf(view)); }
        static glm::vec3 EyeOf(const View& view) { return view.pivot - ForwardOf(view) * view.distance; }

        // Changes the distance to the pivot; closer than the minimum, the pivot moves ahead instead, so scrolling in
        // never gets stuck
        void Zoom(float factor)
        {
            const float wanted = target_.distance * factor;
            if (wanted < kMinDistance)
            {
                target_.pivot += ForwardOf(target_) * (kMinDistance - wanted);
                target_.distance = kMinDistance;
                return;
            }
            target_.distance = std::min(wanted, kMaxDistance);
        }

        void Ease(float deltaSeconds)
        {
            if (!smoothing_)
            {
                current_ = target_;
                return;
            }
            // The same amount of catching up per second at any frame rate
            const float t = 1.0f - std::exp(-kSmoothing * std::max(deltaSeconds, 0.0f));
            current_.pivot += (target_.pivot - current_.pivot) * t;
            current_.yaw += (target_.yaw - current_.yaw) * t;
            current_.pitch += (target_.pitch - current_.pitch) * t;
            current_.distance += (target_.distance - current_.distance) * t;
        }

        void Apply(Engine::Graphics::Camera3D& camera) const
        {
            camera.SetPosition(EyeOf(current_));
            camera.SetOrientation(ForwardOf(current_));
        }

        View target_;        // where the controls have put the camera
        View current_;       // where it is shown (eases towards target_)
        float speed_ = 3.0f; // fly speed, units per second
        float viewportHeight_ = 720.0f;
        bool smoothing_ = true;
        bool flying_ = false;
        bool attached_ = false;
    };
}
