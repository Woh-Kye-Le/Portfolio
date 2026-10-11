/* Start Header ************************************************************************/
/*!
\file       ropes.hpp
\project    Ocean Bound
\author     Kye Le, Jethro, Reginald
\date       April 06, 2025
\brief      Header file defining the Rope structure and functions used for rope-based
			mechanics within the game.

			Features:
			- Definition of the `Rope` struct, including dimensions, position, rotation,
			  and level association.
			- Rope usage tracking and level-specific activation.
			- Declaration of `initializeRopes` for setting up ropes at the start of the game.

			Ropes Initialization and Level Allocation -> Reginald
			Logic -> Kye Le, Jethro

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef ROPES_HPP
#define ROPES_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "pch.h"

/*
	Each rope should have a level location bound to it. This is to ensure that the rope is only active in the level it is in.

*/

typedef struct Rope {
	f32 scaleX, scaleY;
	f32 posX, posY;
	f32 rotation;
	GS_STATES locationOfRope; // level rope is in
	bool beingUsed; // check if the rope is being used
}Rope;

/*
	Function to initialize Levers
*/
void initializeRopes(Rope ropes[]);
#endif
