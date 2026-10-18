/* Start Header ************************************************************************/
/*!
\file       gamefin.cpp
\project    Ocean Bound
\author     Jethro Sung, Kye Le
\date       April, 06, 2025
\brief      Jethro - Load Game Fin, Initialize Game Fin, Update GameFin, Display Game Fin
		    Kye - Initialize Game Fin
            This file implements the game-over screen for the game, handling asset loading, 
            rendering of UI elements, and input interactions to transition between game states. 
            It also ensures proper resource cleanup by unloading meshes, textures, and fonts.
			Kye Le had helped with the initialization of the game fin screen

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "gameFin.h"
#include "pch.h"
#include "utils.hpp"

extern AEGfxVertexList* GameFinPgMesh{};	  //  variable for 
extern AEGfxVertexList* GameFinButtonMesh{}; //  variable for 
extern AEGfxTexture* GameFinTex{};           //  variable for player texture
extern s8 FontB{};						      //  variable for button font 

void loadGameFin() {
    GameFinPgMesh = CreateSquareMesh(0xFF00008B);			    // dark blue mesh 
    GameFinButtonMesh = CreateSquareMesh(0xFFADD8E6);			    // light blue mesh
    GameFinTex = AEGfxTextureLoad("Assets/BarebearsPlayer2.png");  // texture for gameover
    FontB = AEGfxCreateFont("Assets/buggy-font.ttf", 100);           // button font 
}

void initializeGameFin() {
    mesh_Draw(0.0f, 1600.0f, 900.0f, 0.0f, 0.0f, GameFinPgMesh);   // whole gameover screen
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 170.0f, GameFinButtonMesh); // play button 
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 70.0f, GameFinButtonMesh);  // exit button 
    AEGfxPrint(FontB, "Ocean Bound", -0.55f, 0.566f, 2.0f, 0.356f, 0.694f, 0.804f, 1.f);
    AEGfxPrint(FontB, "Try Again", -0.17f, 0.35f, 0.7f, 0, 0, 0, 1.0f);
    AEGfxPrint(FontB, "Main Menu", -0.17f, 0.15f, 0.7f, 0, 0, 0, 1.0f);

    mesh_Draw_With_Texture(0.f, 64.0f, 64.0f, 0.f, -200.f, GameFinButtonMesh, GameFinTex);
}

void updateGameFin(int* currentState)
{
    if (checkCursorCollisionWithRectangle(800.0f, 280.f, 325.f, 75.0f)) {
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
void displayGameFinScreen(s8 font, AEGfxVertexList* BackgroundColour, AEGfxVertexList* ButtonColour, AEGfxTexture* PlayerTex) {

    mesh_Draw(0.0f, 1600.0f, 900.0f, 0.0f, 0.0f, BackgroundColour); // whole main menu screen
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 170.0f, ButtonColour); // play button 
    mesh_Draw(0.0f, 325.0f, 75.0f, 0.0f, 70.0f, ButtonColour);// exit button 
    AEGfxPrint(font, "Game Over,U Win!!!", -0.40f, 0.566f, 1.0f, 0.356f, 0.694f, 0.804f, 1.f);
    AEGfxPrint(font, "Again?", -0.12f, 0.35f, 0.7f, 0, 0, 0, 1.0f);
    AEGfxPrint(font, "Main Menu", -0.17f, 0.15f, 0.7f, 0, 0, 0, 1.0f);

    mesh_Draw_With_Texture(0.f, 64.0f, 64.0f, 0.f, -200.f, ButtonColour, PlayerTex);
}

void unload_GameFin() {
    // Free 
    AEGfxMeshFree(GameFinPgMesh);
    AEGfxMeshFree(GameFinButtonMesh);
    AEGfxDestroyFont(FontB);
    AEGfxTextureUnload(GameFinTex);
}