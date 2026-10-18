/* Start Header ************************************************************************/
/*!
\file       specialAbilities.cpp
\project    Ocean Bound
\author     Kye Le
\date       April 06, 2025
\brief      This file handles logic related to temporary power-up abilities granted to the player
			such as shield, speed burst, and oxygen regeneration. It checks for active states
			based on duration and resets them accordingly. It also adjusts movement behavior
			dynamically based on current active ability.

			Hotkey Ability Logic / Cooldowns / Effects -> Kye Le

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "specialAbilities.h"

int ability_one(int* activeHotkey) {
	if (AEGetTime(time) - lastShieldTime <= 8.0 && *activeHotkey == 1) {
		return 1;
	}
	else 
		return 0;	
}

int ability_two(int* activeHotkey) {
	if (AEGetTime(time) - lastSpeedTime <= 3.0 && *activeHotkey == 2) {
		return 1;
	}
	else
		return 0;
}

int ability_three(int* activeHotkey) {
	if (AEGetTime(time) - lastOxygenTime <= 3.0 && *activeHotkey == 3) {
		return 1;
	}
	else
		return 0;
}

void handle_Speed_Ability(int* activeHotkey, Player* player ) {
	if (ability_two(activeHotkey) && !player->isCrouching) {
		player->max_speed = 600.0f;
	}
	else if (player->isCrouching) {
		player->max_speed = 200.0f;
	}
	else {
		player->max_speed = 400.0f;
	}

}

/*
*	this function checks for the current activeHotkey, if the duration of the power ups has elapsed, 
*	the activeHotkey variable will reset to 0
*/
void revert_activeHotkey (int *activeHotkey){
	if (!ability_two(activeHotkey) && !(ability_three(activeHotkey)) && !(ability_one(activeHotkey)) ){
		*activeHotkey = 0;
	}
}
