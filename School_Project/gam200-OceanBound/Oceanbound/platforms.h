/* Start Header ************************************************************************/
/*!
\file       platforms.hpp
\project    Ocean Bound
\author     Kye Le, Jethro, Reginald
\date       April 06, 2025
\brief      Declares the Platform structure and related functionality for platform
            management in each game level. Includes the initializer function that
            populates platform configurations based on the current level context.

            Features:
            - Struct definition for Platform: includes position, scale, and phase.
            - Level-based platform initializer for dynamic platform placement.

            Logic: Kye Le, Jethro
            Platform Initialization: Reginald

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef PLATFORMS_HPP
#define PLATFORMS_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"

typedef struct Platform {
	f32 scaleX, scaleY;
	f32 posX, posY;
	bool phaseOk;
	f32 rotation;
}Platform;

int initialise_platforms_level(Platform platforms[], int currLevel);

#endif