/* Start Header ************************************************************************/
/*!
\file       gameover.cpp
\project    Ocean Bound
\author     Jethro Sung
\date       April, 06, 2025
\brief      This file handles the logic and rendering of the Game Over screen.

            Includes:
            - Loading assets for the Game Over scene
            - Initializing UI elements and layout
            - Handling user input for "Try Again" and "Main Menu" options
            - Rendering the screen and managing transitions
            - Unloading Game Over assets

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "gameover.h"
#include "pch.h"
#include "utils.hpp"

extern AEGfxVertexList* GameOverPageMesh{};	  //  variable for 
extern AEGfxVertexList* GameOverButtonMesh{}; //  variable for 
extern AEGfxTexture* GameOverTex{};           //  variable for player texture
extern s8 Font{};						      //  variable for button font 

void load_GameOver(){
    GameOverPageMesh = CreateSquareMesh(0xFF00008B);			    // dark blue mesh 
    GameOverButtonMesh = CreateSquareMesh(0xFFADD8E6);			    // light blue mesh
    GameOverTex = AEGfxTextureLoad("Assets/BarebearsPlayer2.png");  // texture for gameover
    Font = AEGfxCreateFont("Assets/buggy-font.ttf", 100);           // button font 
}

void initialise_GameOver() {
    mesh_Draw(0.0f, 1600.0f, 900.0f, 0.0f, 0.0f, GameOverPageMesh);   // whole gameover screen
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 170.0f, GameOverButtonMesh); // play button 
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 70.0f, GameOverButtonMesh);  // exit button 
    AEGfxPrint(Font, "Ocean Bound", -0.55f, 0.566f, 2.0f, 0.356f, 0.694f, 0.804f, 1.f);
    AEGfxPrint(Font, "Try Again", -0.17f, 0.35f, 0.7f, 0, 0, 0, 1.0f);
    AEGfxPrint(Font, "Main Menu", -0.17f, 0.15f, 0.7f, 0, 0, 0, 1.0f);

    mesh_Draw_With_Texture(0.f, 64.0f, 64.0f, 0.f, -200.f, GameOverButtonMesh, GameOverTex);
}

void updateGameOver(int* currentState)
{
    if (checkCursorCollisionWithRectangle(800.0f, 280.f,325.f, 75.0f)) {
        if (AEInputCheckTriggered(AEVK_LBUTTON)) {
            *currentState = (int)GS_LEVEL1;
        }
    }

    if (checkCursorCollisionWithRectangle(800.0f, 380.f, 325.f, 75.0f)) {
        if (AEInputCheckTriggered(AEVK_LBUTTON)) {
            *currentState = (int)GS_MAINMENU;
        }
    }
}

// Function to display the game over screen
void displayGameOverScreen(s8 font, AEGfxVertexList* BackgroundColour, AEGfxVertexList* ButtonColour, AEGfxTexture* PlayerTex) {

    mesh_Draw(0.0f, 1600.0f, 900.0f, 0.0f, 0.0f, BackgroundColour); // whole main menu screen
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 170.0f, ButtonColour); // play button 
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 70.0f, ButtonColour);// exit button 
    AEGfxPrint(font, "Ocean Bound", -0.55f, 0.566f, 2.0f, 0.356f, 0.694f, 0.804f, 1.f);
    AEGfxPrint(font, "Try Again", -0.17f, 0.35f, 0.7f, 0, 0, 0, 1.0f);
    AEGfxPrint(font, "Main Menu", -0.17f, 0.15f, 0.7f, 0, 0, 0, 1.0f);

    mesh_Draw_With_Texture(0.f, 64.0f, 64.0f, 0.f, -200.f, ButtonColour, PlayerTex);
}

void unload_GameOver() {
    // Free 
    AEGfxMeshFree(GameOverPageMesh);
    AEGfxMeshFree(GameOverButtonMesh);
    AEGfxDestroyFont(Font);
    AEGfxTextureUnload(GameOverTex);
}