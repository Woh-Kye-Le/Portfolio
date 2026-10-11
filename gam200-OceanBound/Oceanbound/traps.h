/* Start Header ************************************************************************/
/*!
\file       traps.hpp
\project    Ocean Bound
\author     Jethro
\date       April 06, 2025
\brief      Header file for trap structure and types used in level interactions.

			This file defines:
			- Trap types: Default spike, timed, and proximity-based.
			- The `Trap` struct which encapsulates positional data, activation logic,
			  and configuration for different trap types.
			- Trap-specific fields such as activation intervals and proximity radius
			  used for gameplay logic and animations.

			Traps interact with the player and are updated per frame in-game to simulate
			environmental hazards and challenge elements.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef TRAPS_HPP
#define TRAPS_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"

#define MAX_AFFECTED_ROOMS 5
#define MAX_AFFECTED_LEVELS 10

enum TRAP_TYPES
{
	DEFAULT_SPIKE_TRAP,
	TIMED_TRAP,
	PROXIMITY_TRAP
};

typedef struct Trap {
	f32 scaleX, scaleY;
	f32 posX, posY;
	f32 rotation;
	bool isTriggered; // check if the lever has been triggered (needed to enable tidal etc for a room as well as disable it)
	TRAP_TYPES trapType; //Determines how trap will interact when triggered
	bool active; //Check if trap has already been used or activated.
	f32 originalScaleY;
	f32 originalPosY;
	bool deactivated;
	//f32 original_scaleX, original_posx;
	//AEMtx33 transform;

	/*
		Fields For Non-Default Trap Types
	*/
	float proximityDist; //50 units left 50 right
	float timer;
	float activationInterval;
}Trap;


#endif