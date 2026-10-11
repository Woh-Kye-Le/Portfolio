/* Start Header ************************************************************************/
/*!
\file       platforms.cpp
\project    Ocean Bound
\author     Kye Le, Jethro, Reginald
\date       April, 06, 2025
\brief      This file defines and initializes platform layouts for different game levels, enabling platform-based navigation and interactions. 
			Based on the current level, it configures positions, sizes, and properties (such as phase-through behavior) of multiple platforms used for collision detection and movement control.
			Logic: Kye Le, Jethro
			Platform Initialization: Reginald

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "platforms.h"
#include "pch.h"
#include <math.h>
#include "GameStateManager.h"


//This switches the platform displayed based on the current level;
int initialise_platforms_level(Platform platforms[], int currLevel) {
	switch (currLevel)
	{
	case 0:	  platforms[0] = { 200.0f, 80.0f, -400.0f, groundLevel + 40.0f };//Level 1
		  platforms[1] = { 200.0f, 80.0f, -200.0f, groundLevel + 120.0f };//Level 1
		  platforms[2] = { 75.0f, 80.0f, 0.0f, groundLevel + 40.0f };//Level 1
		  platforms[3] = { 75.0f, 80.0f, 300.0f, groundLevel + 40.0f };//Level 1
		  platforms[4] = { 75.0f, 80.0f, 700.0f, groundLevel + 40.0f };//Level 1
		  platforms[5] = { 75.0f, 80.0f, 575.0f, groundLevel + 120.0f };//Level 1
		  platforms[6] = { 75.0f, 190.0f, 155.0f, groundLevel + 95.0f };//Level 1
		  platforms[7] = { 200.0f, 80.0f, -650.0f, 100.0f };//Level 2
		  platforms[8] = { 75.0f, 80.0f, 150.0f, 300.0f };//Level 2
		  platforms[9] = { 75.0f, 330.0f, 350.0f, 165.0f };//Level 2
		  platforms[10] = { 75.0f, 330.0f, 500.0f, 300.0f };//Level 2
		  platforms[11] = { 75.0f, 330.0f, 650.0f, 165.0f };//Level 2
		  return 12;
	case 2:   platforms[0] = { 200.0, 20.0, -695.0f, groundLevel + 200.0f};
		  platforms[1] = { 60.0, 200.0, -570.0f, groundLevel + 290.0f };
		  platforms[2] = { 110.0, 20.0, -740.0f, groundLevel + 720.0f };
		  platforms[3] = { 110.0, 20.0, -505.0f, groundLevel + 720.0f };
		  platforms[4] = { 110.0, 20.0, -385.0f, groundLevel + 750.0f };
		  platforms[5] = { 110.0, 20.0, -385.0f, groundLevel + 400.0f };
		  platforms[6] = { 110.0, 20.0, -80.0f, groundLevel + 435.0f };
		  platforms[7] = { 110.0, 20.0, -385.0f, groundLevel + 160.0f };
		  platforms[8] = { 600.0, 20.0, 200.0f, groundLevel + 160.0f };
		  platforms[9] = { 20.0, 125.0, 510.0f, groundLevel + 212.0f };
		  platforms[10] = { 20.0, 148.0, 40.0f, groundLevel + 76.0f };
		  platforms[11] = { 250.0, 50.0, 150.0f, groundLevel + 250.0f };
		  platforms[12] = { 110.0, 20.0, 435.0f, groundLevel + 435.0f };
		  platforms[13] = { 110.0, 20.0, 555.0f, groundLevel + 405.0f };
		  platforms[14] = { 110.0, 20.0, 740.0f, groundLevel + 160.0f };
		  return 15;
	case 4:	  platforms[0] = { 180.0f, 20.0f, -705.0f, 300.0f };
		  platforms[1] = { 150.0f, 20.0f, -585.0f, 150.0f };
		  platforms[2] = { 5.0f, 445.0f, -375.0f, 225.0f, true };
		  platforms[3] = { 600.0f, 20.0f, -70.0f, 220.0f };
		  platforms[4] = { 450.0f, 20.0f, 570.0f, 220.0f };
		  platforms[5] = { 5.0f, 220.0f, -102.0f, 338.0f, true };
		  platforms[6] = { 5.0f, 205.0f, -50.0f, 108.0f };
		  platforms[7] = { 5.0f, 205.0f, 200.0f, 108.0f, true };
		  platforms[8] = { 5.0f, 205.0f, 450.0f, 108.0f , true };
		  platforms[9] = { 250.0f, 20.0f, -670.0f, -150.0f };
		  platforms[10] = { 5.0f, 440.0f, 350.0f, -225.0f , true };
		  return 11;
	case 6:	  platforms[0] = { 5.0f, 445.0f, -650.0f, 225.0f, true };
		  platforms[1] = { 750.0f, 125.0f, -150.0f, 65.0f };
		  platforms[2] = { 5.0f, 200.0f, -400.0f, 345.0f };
		  platforms[3] = { 250.0f, 5.0f, -278.0f, 245.0f };
		  platforms[4] = { 200.0f, 5.0f, 50.0f, 245.0f };
		  platforms[5] = { 5.0f, 200.0f, 145.0f, 345.0f };
		  platforms[6] = { 450.0f, 20.0f, 130.0f, groundLevel + 330.0f };
		  platforms[7] = { 250.0f, 20.0f, -673.0f, groundLevel + 190.0f };
		  return 8;
	default:
		return 0;
	}
}
