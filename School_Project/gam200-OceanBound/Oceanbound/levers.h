/* Start Header ************************************************************************/
/*!
\file       Levers.hpp
\project    Ocean Bound
\author     Jethro Sung, Reginald Lew
\date       April 06, 2025
\brief      This header defines the Lever system and its associated types and functions.

			Levers act as interactive environmental toggles that influence water behavior
			and room mechanics across multiple levels. Each lever can be linked to one or
			more rooms in multiple levels and can trigger one of two types of interactions:
			- LEVER_TIDAL: Toggles the presence of tidal mechanics (enabling/disabling rising/falling water).
			- LEVER_WATER_FLOW: Directly initiates or halts water movement in designated water bodies.

			The `Lever` struct stores lever configuration, affected levels/rooms/water blocks,
			and its current state. The `initializeLevers` function sets up all lever instances
			required for the game.
			Logic-Jethro
			Implementation/Intialization Of Levers: Reginald

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef LEVERS_HPP
#define LEVERS_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"

#define MAX_AFFECTED_ROOMS 5
#define MAX_AFFECTED_LEVELS 10
#define MAX_AFFECTED_WATER 5

enum LEVER_TYPE {
	LEVER_TIDAL,      // This lever toggles the room’s tidal state (hasWater and hasTidal)
	LEVER_WATER_FLOW  // This lever only triggers the water cycle (water rising, stopping, falling)
};

typedef struct Lever {
	f32 scaleX, scaleY;
	f32 posX, posY;
	f32 rotation;
	int locationOfLever; // level lever is in
	bool isTriggered; // check if the lever has been triggered (needed to enable tidal etc for a room as well as disable it)
	/*
		Essentially, levelafffected affects based on the rooms. So when looping through currlevel and currlevel+1, we would pass in i, in order to reach the level we want to affect.
		also, assuming that we are cycling through rooms via a room counter, we loop through the affected rooms per level
	*/
	int levelAffected[MAX_AFFECTED_LEVELS][MAX_AFFECTED_ROOMS], numAffectedRoomsPerLevel[MAX_AFFECTED_LEVELS];
	int leverType;
	int waterAffected[MAX_AFFECTED_WATER];
	//f32 original_scaleX, original_posx;
}Lever;

/*
	Function to initialize Levers
*/
void initializeLevers(Lever levers[], int numLevers);
#endif