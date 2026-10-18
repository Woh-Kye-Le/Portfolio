/* Start Header ************************************************************************/
/*!
\file       special_abilities.hpp
\project    Ocean Bound
\author     Kye Le
\date       April 06, 2025
\brief      Header file declaring functions and logic related to the player’s special abilities.

            This file supports power-up mechanics such as:
            - Shield (temporary invulnerability)
            - Speed boost (increased movement speed)
            - Oxygen replenishment (survival aid for water navigation)

            Includes:
            - Time-based ability checks and duration tracking
            - Conditional modification of player state and movement
            - Resetting ability effects after expiration

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef SPECIAL_ABILITIES_HPP
#define SPECIAL_ABILITIES_HPP

#include "objects.hpp"

int ability_one(int* activeHotkey);
int ability_two(int* activeHotKey);
int ability_three(int* activeHotkey);
void handle_Speed_Ability(int* activeHotkey, Player* player);
void revert_activeHotkey(int* activeHotkey);

#endif