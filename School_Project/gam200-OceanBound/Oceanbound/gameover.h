#ifndef GAMEOVER_H
#define GAMEOVER_H

/* Start Header ************************************************************************/
/*!
\file       gameover.h
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This header provides declarations for managing the Game Over screen.

            Functions include:
            - Loading resources (meshes, textures, fonts)
            - Initializing and rendering UI elements for retry/menu selection
            - Updating user interactions during the Game Over state
            - Cleaning up allocated assets

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "player.h"  // Include the player header to access player health and properties

// Function to display the game over screen
void load_GameOver();

void initialise_GameOver();

void displayGameOverScreen(s8 font, AEGfxVertexList* BackgroundColour, AEGfxVertexList* ButtonColour, AEGfxTexture* PlayerTex);

void updateGameOver(int* current_state);

void unload_GameOver();

#endif // GAMEOVER_H#pragma once
