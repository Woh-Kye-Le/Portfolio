/* Start Header ************************************************************************/
/*!
\file       trapSwitch.hpp
\project    Ocean Bound
\author     Jethro
\date       April 06, 2025
\brief      Header file defining the TrapSwitch struct and types for trap activation logic.

            This file introduces a switch mechanism for toggling traps in the environment.
            - `TrapSwitch` struct holds positional data, toggle state, cooldown, and a list
              of trap indices it affects.
            - The switch is used to control traps interactively during gameplay.
            - Designed to work with multiple trap types (e.g., spikes, timed, proximity).
            - Supports toggling traps with cooldown logic to avoid accidental rapid reactivation.

            Used extensively in the level management system for dynamic trap interaction.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/

#ifndef TRAPS_SWITCH_HPP
#define TRAPS_SWITCH_HPP
#pragma once
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#define MAX_TRAPS_AFFECTED 10

enum TRAP_SWITCH_TYPE {
    TRAP_SWITCH_TOGGLE  // A simple on/off switch
};

typedef struct TrapSwitch {
    f32 scaleX = 0.f, scaleY=0.f;
    f32 posX = 0.f, posY=0.f;
    bool isTriggered =false;            // Current state of the switch
    int trapIndices[MAX_TRAPS_AFFECTED] = {}; // Indices of traps in the level that this switch affects
    int numTrapsAffected = 0;        // How many traps are in the trapIndices array
    float cooldown = 1.f;              // Cooldown timer to prevent accidental multiple toggles
    f32 rotation = 0.f;
} TrapSwitch;

#endif