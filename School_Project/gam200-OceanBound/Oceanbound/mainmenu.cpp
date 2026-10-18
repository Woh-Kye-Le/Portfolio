/* Start Header ************************************************************************/
/*!
\file       mainmenu.cpp
\project    Ocean Bound
\author     Jethro Sung
\date       April, 06, 2025
\brief      This file handles the main menu screen of the game, including loading assets, 
			rendering the menu layout and buttons, and processing user interactions like starting the game, 
			opening the leaderboard, or exiting. It also ensures proper cleanup of graphical resources when the menu is unloaded.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <stdbool.h>
#include "mainmenu.h"
#include "AEEngine.h"
#include "pch.h"
#include <crtdbg.h> // To check for memory leaks

extern AEGfxVertexList* mainMenuPageMesh{};	  //  variable for dark blue mesh 
extern AEGfxVertexList* logoPageMesh{};       //  variable for digipen logo
extern AEGfxVertexList* mainMenuButtonMesh{}; //  variable for light blue mesh 
extern s8 titleFont{};						  //  variable for title font
extern s8 buttonFont{};						  //  variable for button font 
extern AEGfxTexture* menuTex{};				  //  variable for menu texture
extern AEGfxTexture* Digipen_Logo{};

static f32 gametime = 0.f;
f32 dt{};

void load_mainMenu() {
	logoPageMesh = CreateSquareMesh(0xFF000000);				// white mesh
	mainMenuPageMesh = CreateSquareMesh(0xFF00008B);			// dark blue mesh 
	mainMenuButtonMesh = CreateSquareMesh(0xFFADD8E6);			// light blue mesh
	titleFont = AEGfxCreateFont("Assets/Robus-BWqod.ttf", 75);	// title font
	buttonFont = AEGfxCreateFont("Assets/buggy-font.ttf", 100); // button font 
	menuTex = AEGfxTextureLoad("Assets/mainMenu.jpg");			// Menu text
	Digipen_Logo = AEGfxTextureLoad("Assets/DigiPen_BLACK.png");// digipen text
}

void initialise_mainMenu() {
	// mesh draw & word print
	dt = (f32)AEFrameRateControllerGetFrameTime();
	gametime += dt;
	mesh_Draw_With_Texture(0.0f, 1600.0f, 900.0f, 0.0f, 0.0f, logoPageMesh, Digipen_Logo);
	if (gametime >= 2.0) {
		mesh_Draw_With_Texture(0.0f, 1600.0f, 900.0f, 0.0f, 0.0f, mainMenuPageMesh, menuTex);	// whole main menu screen
		mesh_Draw(0.0f, 410.0f, 75.0f, 540.0f, 0.0f, mainMenuButtonMesh);						// play button 
		mesh_Draw(0.0f, 410.0f, 75.0f, 540.0f, -112.5f, mainMenuButtonMesh);					// leader button 
		mesh_Draw(0.0f, 410.0f, 75.0f, 540.0f, -212.5f, mainMenuButtonMesh);					// exit button 
		AEGfxPrint(titleFont, "Ocean Bound", -0.4f, 0.566f, 2.0f, 0.356f, 0.694f, 0.804f, 1.f); // title text
		AEGfxPrint(buttonFont, "Play", 0.55f, -0.05f, 0.5f, 0.356f, 0.694f, 0.804f, 1.f);		// "play" text
		AEGfxPrint(buttonFont, "Leaderboard", 0.43f, -0.29f, 0.35f, 1.0f, 1.0f, 1.0f, 1.f);		// "leaderboard" text 
		AEGfxPrint(buttonFont, "Exit", 0.58f, -0.5f, 0.35f, 1.0f, 1.0f, 1.0f, 1.f);				// "exit" text 
	}
}

void update_mainMenu(int* gameRun, bool* leaderboardPopup, bool* playerPopup) {
	// interactions 
	if (checkCursorCollisionWithRectangle(1340.f, 450.0f, 410.f, 75.0f)) {
		if (AEInputCheckTriggered(AEVK_LBUTTON)) {
			*playerPopup = true;
		}
	}

	if (checkCursorCollisionWithRectangle(1340.f, 562.5f, 410.f, 75.0f)) {
		if (AEInputCheckTriggered(AEVK_LBUTTON)) {
			*leaderboardPopup = true;
		}
	}

	if (checkCursorCollisionWithRectangle(1340.0f, 662.5f, 410.f, 75.0f)) {
		if (AEInputCheckTriggered(AEVK_LBUTTON)) {
			*gameRun = 0;
		}
	}
}

void unload_mainMenu() {
	// Free 
	AEGfxMeshFree(logoPageMesh);
	AEGfxMeshFree(mainMenuPageMesh);
	AEGfxMeshFree(mainMenuButtonMesh);
	AEGfxDestroyFont(titleFont);
	AEGfxDestroyFont(buttonFont);
	AEGfxTextureUnload(menuTex);
	AEGfxTextureUnload(Digipen_Logo);
}