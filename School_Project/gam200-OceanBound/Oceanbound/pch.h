/* Start Header ************************************************************************/
/*!
\file       pch.h
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This precompiled header file includes essential headers that are used
            across multiple source files to improve compilation time and ensure
            consistent dependencies throughout the game.

            Currently includes:
            - `GameStateList.h`: Provides enumerated states for all game screens
              and level transitions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef PCH_H
#define PCH_H

#include "GameStateList.h" // Set the game state list to be used throughout the game for game states
#endif //PCH_H