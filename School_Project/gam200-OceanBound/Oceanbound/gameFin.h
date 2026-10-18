#pragma once
#ifndef GAMEFINISH_H
#define GAMEFINISH_H

/* Start Header ************************************************************************/
/*!
\file       gamefin.h
\project    Ocean Bound
\author     Jethro Sung, Kye Le
\date       April, 06, 2025
\brief      Jethro - Load Game Fin, Initialize Game Fin, Update GameFin, Display Game Fin
            Kye - Initialize Game Fin
            This header defines the interface for the Game Over and Game End screens.
            It includes function declarations for initializing, updating, rendering,
            and unloading the final game state screen. These screens are shown when the
            player finishes the game or dies, providing visual feedback and options to continue.

            Kye Le had helped with the initialization of the game fin screen

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "player.h"  // Include the player header to access player health and properties

// Function to display the game over screen
void loadGameFin();

void initializeGameFin();

void displayGameFinScreen(s8 font, AEGfxVertexList* BackgroundColour, AEGfxVertexList* ButtonColour, AEGfxTexture* PlayerTex);

void updateGameFin(int* current_state);

void unload_GameFin();

#endif // GAMEFINISH_H#pragma once
