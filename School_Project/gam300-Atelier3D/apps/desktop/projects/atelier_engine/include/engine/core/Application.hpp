/* !
@file    	    Application.hpp
@author  	    c.kojat@digipen.edu [77%]
         	    k.thantwin@digipen.edu [13%]
         	    sanni.go@digipen.edu [10%]
@date    		22/09/2026
@brief
    Declares Application, the base class for programs with a main loop (the
    editor and the game runtime): owns the window and audio system and calls the
    OnInit/OnUpdate/OnShutdown hooks.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/audio/AudioSystem.hpp"
#include "engine/platform/Window.hpp"
#include "engine/messenger/EventBus.hpp"

#include <memory>

namespace Engine::Core
{
    struct ApplicationSpec
    {
        Platform::WindowSpec window;
        int maxFrames = 0; // stop after this many frames, 0 = run until the window closes (handy for tests)
    };

    // Base class for anything with a main loop (the editor and the standalone game runtime).
    // Subclasses override the On* hooks; Run() owns the loop: update, audio, present, poll events.
    class Application
    {
    public:
        explicit Application(ApplicationSpec spec = {});
        virtual ~Application();

        void Run();
        void Close();

        Platform::Window& GetWindow();
        const Platform::Window& GetWindow() const;

        Audio::AudioSystem& GetAudio();
        const Audio::AudioSystem& GetAudio() const;

        // Event messaging system
        EventBus& GetEvents() { return events_; }
        const EventBus& GetEvents() const { return events_; }

    protected:
        // OnInit runs once before the first frame, OnUpdate every frame, OnShutdown after the loop ends
        virtual void OnInit() {}
        virtual void OnShutdown() {}
        virtual void OnUpdate(double /*deltaSeconds*/) {}

    private:
        ApplicationSpec spec_;
        std::unique_ptr<Platform::Window> window_;
        Audio::AudioSystem audio_;
        bool running_ = true;
        int frameCount_ = 0;

        EventBus events_;
        int lastWidth_ = 0;
        int lastHeight_ = 0;
    };
}
