/* !
@file    	    FixedSteps.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    Turns the time of a frame into a number of steps of a fixed length, so what runs in those steps goes at the same
    speed at any frame rate.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Core
{
    /*!
    \brief   Adds a frame's time to the time not stepped yet, and takes whole steps out of it.
    \param   seconds      Time not stepped yet; less than one step when this returns.
    \param   deltaSeconds The frame's time.
    \param   step         The length of a step, in seconds.
    \param   maxSteps     The most to take: a backlog over this is dropped, so a long hitch does not make the next
                          frames longer still.
    \return  How many steps to run: 0 when less than one step of time has built up.
    */
    inline int TakeFixedSteps(double& seconds, double deltaSeconds, double step, int maxSteps)
    {
        // Absorbs rounding, so that e.g. three frames of 1/180 s still make one step
        constexpr double kEpsilon = 1e-9;

        seconds += deltaSeconds > 0.0 ? deltaSeconds : 0.0;
        int steps = 0;
        while (seconds + kEpsilon >= step && steps < maxSteps)
        {
            seconds = seconds > step ? seconds - step : 0.0;
            ++steps;
        }
        if (seconds + kEpsilon >= step)
        {
            seconds = 0.0;
        }
        return steps;
    }
}
