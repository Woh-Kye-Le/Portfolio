/* Start Header ************************************************************************/
/*!
\file       mainmenu.hpp
\project    Ocean Bound
\author     Kye Le
\date       April 06, 2025
\brief      This header provides declarations for managing the Main Menu system.

            The main menu serves as the entry point to the game, providing access to core
            navigation options such as starting the game, viewing the leaderboard, and
            entering the player's name. It also supports displaying popups for player
            input and leaderboard viewing.

            Functions included:
            - load_mainMenu: Loads resources such as meshes and textures.
            - initialise_mainMenu: Sets up and renders menu components.
            - update_mainMenu: Handles user input and interactions.
            - unload_mainMenu: Frees any loaded resources.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef MAINMENU_HPP
#define MAINMENU_HPP

#include "utils.hpp"


void load_mainMenu();

void initialise_mainMenu();

void update_mainMenu(int* gameRun, bool* leaderboardPopup, bool* playerPopup);

void unload_mainMenu();

#endif