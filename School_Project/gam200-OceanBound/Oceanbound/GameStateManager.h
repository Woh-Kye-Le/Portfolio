/* Start Header ************************************************************************/
/*!
\file       GameStateManager.hpp
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This header provides declarations related to core game state constants.

            Specifically, it declares the external constant `groundLevel`, which is used
            across multiple game systems such as room initialization and default placements,
            especially in base-level logic that predates or operates independently of the
            room-based vertical positioning system.

            The actual definition of `groundLevel` is expected to be provided in the main source file.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef GAMESTATEMANAGER_HPP
#define GAMESTATEMANAGER_HPP
	
extern const float groundLevel; //Here, we declare the const for ground level, to be defined in main. Although most have switched to room, for some base inits, we still use this.

#endif