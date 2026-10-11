#pragma once

/* Start Header ************************************************************************/
/*!
\file       GameStateManager.h
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This header defines the various game states used to control transitions between
			levels, menus, and special states like game over or game end.

			The GS_STATES enum is used as the primary state machine controller for the game,
			allowing the game loop and state management system to determine what logic
			and rendering should occur based on the current context.

			Values range from level states (GS_LEVEL1 to GS_LEVEL10) to utility and meta-states
			such as GS_GAMEOVER, GS_MAINMENU, and GS_QUIT.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/


//An enumeration to represent the various game states
enum GS_STATES
{
	GS_LEVEL1 = 0,// Game state for Level 1
	GS_LEVEL2,// Game state for Level 2, implicit value of 1
	GS_LEVEL3,
	GS_LEVEL4,
	GS_LEVEL5,
	GS_LEVEL6,
	GS_LEVEL7,
	GS_LEVEL8,
	GS_LEVEL9,
	GS_LEVEL10,

	GS_GAMEOVER,
	GS_QUIT,// Game state to quit the game, implicit value of 2
	GS_RESTART,// Game state to restart the game, implicit value of 3
	GS_MAINMENU,
	GS_GAME_END
};
