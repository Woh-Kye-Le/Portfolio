/* Start Header ************************************************************************/
/*!
\file       water.hpp
\project    Ocean Bound
\author     Jethro
\date       April 06, 2025
\brief      Definitions related to water mechanics, including dynamic water blocks and tidal behavior.

			This header defines the `Water` struct used throughout the game for simulating
			rising, falling, and static water. It includes support for different water types
			such as default, poison, and lava, as well as configurable behavior through
			tidal controls and lever-based interactions.

			Key features include:
			- Linked water blocks to simulate cascading effects.
			- Flags to manage tidal systems, permanent flow, and manual drainage.
			- State tracking for advanced control and transitions (e.g., tidal reversals).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef WATER_HPP
#define WATER_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
//constexpr auto groundlevel = (-450.0f);

enum WATER_STATE {
	WATER_RISING,
	WATER_STATIC,
	WATER_FALLING
};

enum WATER_TYPE {
	WATER_DEFAULT,
	WATER_POISON,
	WATER_LAVA
};


typedef struct Water {
	f32 scaleX, scaleY;
	f32 posX, posY;
	f32 originalPosY; //This original PosY is needed so that in the case of tidal Reset (when tidal for room is affected by lever), we can revert the water to the original pos.
	WATER_STATE state;
	f32 maxTidalHeight;        // Maximum height this block should reach
	bool maxTidalReached;
	WATER_TYPE type;
	int linkedID; // ID of the water block that this one is linked to.
	bool LinkedAbove; // True if this block is above linked block, else false
	bool waterMustBeDrained; // if true, we drain water
	bool permaFlow; //If true, water will be perma rising or falling until conditions are met.
	WATER_STATE prevState; //records last state of water ONLY for water lever triple switch
	f32 rotation;
}Water;

//NOTE: Drainage system needs to be implemented for Tidal Reset to allow for the water to drain smoothly, rather than just disappearing.

#endif