/* !
@file    	    Time.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		22/09/2026
@brief
    Time helpers for frame timing based on std::chrono::steady_clock.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <chrono>

namespace Engine::Core
{
    // Small helper for frame timing. steady_clock because it never jumps backwards
    // (system_clock can, e.g. when Windows syncs the time).
    class Time
    {
    public:
        using Clock = std::chrono::steady_clock;

        static Clock::time_point Now() { return Clock::now(); }

        static double SecondsBetween(Clock::time_point a, Clock::time_point b)
        {
            return std::chrono::duration<double>(b - a).count();
        }
    };
}
