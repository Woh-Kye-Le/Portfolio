/* !
@file    	    Application.cpp
@author  	    c.kojat@digipen.edu [77%]
         	    k.thantwin@digipen.edu [13%]
         	    sanni.go@digipen.edu [10%]
@date    		22/09/2026
@brief
    Implements Application::Run, the main loop: update, audio update, present
    and event polling each frame, redraw while resizing, and a periodic average-
    FPS log line.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/core/Application.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/Time.hpp"
#include "engine/messenger/Events.hpp"

#include <algorithm>
#include <format>
#include <string>

namespace Engine::Core
{
    Application::Application(ApplicationSpec spec)
        : spec_(std::move(spec)), window_(Platform::CreateWindow(spec_.window))
    {
    }

    Application::~Application() = default;

    // The main loop. Returns when the window closes or Close() is called
    void Application::Run()
    {
        // Before OnInit() so game code can start sounds right away.
        audio_.Init();

        OnInit();

        lastWidth_ = window_->Width();
        lastHeight_ = window_->Height();

        auto last = Time::Now();

        // The average frame rate, logged every few seconds so the log file shows whether the game holds 60 FPS
        constexpr double kFpsReportSeconds = 5.0;
        double fpsSeconds = 0.0;
        int fpsFrames = 0;
        double slowestFrame = 0.0; // a single long frame (a hitch) does not show in the average
        int refreshes = 0;         // frames the window asked for from inside its own messages

        // The window calls this while it's being resized (when our loop below is stuck inside Windows),
        // so the picture keeps updating. The flag stops it running inside itself.
        bool refreshing = false;
        window_->SetRefreshCallback(
            [&]
            {
                if (refreshing) return;
                refreshing = true;
                ++refreshes;
                const auto now = Time::Now();
                const double dt = Time::SecondsBetween(last, now);
                last = now;
                OnUpdate(dt);
                window_->Present();
                refreshing = false;
            });

        while (running_ && !window_->ShouldClose())
        {
            const auto now = Time::Now();
            const double dt = Time::SecondsBetween(last, now);
            last = now;

            events_.DispatchQueued();

            // one frame: game/editor update, FMOD update, show the frame, then read new input for next time.
            // The flag is set here too: resizing the window from inside the frame (F11 fullscreen) sends a
            // resize message straight away, and the refresh callback must not start a second frame inside this one.
            refreshing = true;
            OnUpdate(dt);
            refreshing = false;
            audio_.Update();
            window_->Present();

            window_->PollEvents();

            if (window_->Width() != lastWidth_ || window_->Height() != lastHeight_)
            {
                lastWidth_ = window_->Width();
                lastHeight_ = window_->Height();
                events_.Emit(Events::WindowResized{lastWidth_, lastHeight_});
            }

            ++frameCount_;
            fpsSeconds += dt;
            ++fpsFrames;
            slowestFrame = std::max(slowestFrame, dt);
            if (fpsSeconds >= kFpsReportSeconds)
            {
                LOG_INFO(Performance,
                         "Average {:.1f} FPS ({:.2f} ms per frame, slowest {:.0f} ms) over the last {:.0f} s{}",
                         fpsFrames / fpsSeconds, 1000.0 * fpsSeconds / fpsFrames, 1000.0 * slowestFrame, fpsSeconds,
                         refreshes > 0 ? std::format(", {} redraws from window messages", refreshes) : "");
                fpsSeconds = 0.0;
                fpsFrames = 0;
                slowestFrame = 0.0;
                refreshes = 0;
            }
            if (spec_.maxFrames > 0 && frameCount_ >= spec_.maxFrames)
            {
                Close();
            }
        }

        window_->SetRefreshCallback(nullptr);

        // Called here rather than in ~Application(), where the derived override would no longer be reachable
        OnShutdown();
        audio_.Shutdown();
    }

    // Ends the loop after the current frame
    void Application::Close()
    {
        running_ = false;
        if (window_)
        {
            window_->RequestClose();
        }
    }

    Platform::Window& Application::GetWindow()
    {
        return *window_;
    }

    const Platform::Window& Application::GetWindow() const
    {
        return *window_;
    }

    Audio::AudioSystem& Application::GetAudio()
    {
        return audio_;
    }

    const Audio::AudioSystem& Application::GetAudio() const
    {
        return audio_;
    }
}
