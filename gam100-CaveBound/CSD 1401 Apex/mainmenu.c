/*
@file       this is file mainmenu.c
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    projCavebound
@date       21/10/24
@brief      this file contains a collection of definitions of a series of functions in mainmenu.h, by including mainmenu.h in the file, and mainmenu.h in main.c,
			main.c is able to call on the functions in mainmenu.h, which processes the input according to the definitions in mainmenu.c
*/

#include "cprocessing.h"//To be able to access the CProcessing functions
//#include "mainmenu.h" //To be able to access the functions in mainmenu.h
#include "game.h"//To be able to access the functions in game.h
#include "utils.h" //To be able to access the functions in utils.h
//#include "leaderBoard.h"
#include "options.h"
//#include "credits.h"
#include <stdio.h>

CP_Font myFont;
CP_Image mainBg; // Variable to hold background image
CP_Image trophy;
CP_Color translucent;
CP_Color myColor;
CP_Image logo;
CP_Color fontColour;
CP_Sound menuMusic;
CP_Sound click;


void Main_Menu_Init(void)
{
	logo = CP_Image_Load("./Assets/DigiPen_BLACK.png");
	myFont = CP_Font_Load("./Assets/HanaleiFill-Regular.ttf");
	//CP_Settings_RectMode(CP_POSITION_CENTER);
	mainBg = CP_Image_Load("./Assets/MainMenu.jpg"); // Variable to hold background image
	trophy = CP_Image_Load("./Assets/trophy.png");
	translucent = CP_Color_Create(255, 255, 255, 100);
	myColor = CP_Color_Create(80, 115, 130, 130);
	fontColour = CP_Color_Create(223, 228, 240, 255);

	menuMusic = CP_Sound_Load("Assets/mainmenu2.wav"); //main menu music here
	click = CP_Sound_Load("Assets/clicking.wav");

}

/*
	*Done By: Lumi, Jiahui, Jethro
	* This basically just draws the main menu buttons, background as well as the theme music. Furthermore, we highlight the button the player is hovering over, as well as handle when the various buttons are clicked.
	* Note drawPopup and promptName which runs regardless of each iteration, for more details, go to points.c
*/
void Main_Menu_Update(void)
{
	CP_Sound_PlayAdvanced(menuMusic, 0.2f, 1.0f, TRUE, CP_SOUND_GROUP_MUSIC);

	// Draw the background image, scaled to fill the screen
	double time = CP_System_GetSeconds();
	CP_Graphics_ClearBackground(CP_Color_Create(0, 0, 0, 255));
	CP_Image_Draw(logo, (float)CP_System_GetWindowWidth() / 2, (float)CP_System_GetWindowHeight() / 2, (float)CP_System_GetWindowWidth(), (float)CP_System_GetWindowHeight(), 255);
	if (time >= 2.0)
	{
		CP_Image_Draw(mainBg, (float)CP_System_GetWindowWidth() / 2, (float)CP_System_GetWindowHeight() / 2,
			(float)CP_System_GetWindowWidth(), (float)CP_System_GetWindowHeight(), 255);

		float screenWidth = (float)CP_System_GetWindowWidth();
		float screenHeight = (float)CP_System_GetWindowHeight();

		//Button Size
		float buttonWidth = 200.0f;
		float buttonHeight = 70.0f;

		//Play Button
		float playX = screenWidth / 2.225f;
		float playY = screenHeight / 2.3f;

		//Options Button
		float optionsX = screenWidth / 2.225f;
		float optionsY = screenHeight / 1.8f;

		//Credits Button
		float creditsX = screenWidth / 2.225f;
		float creditsY = screenHeight / 2.5f + 250.0f;

		//Exit Button
		float exitX = screenWidth / 2.225f;
		float exitY = screenHeight / 1.27f;

		// Mouse Position
		float mousex = CP_Input_GetMouseX();
		float mousey = CP_Input_GetMouseY();

		//trophy rect post
		float trophyX = (screenWidth / 2.225f) + 225.0f;
		float trophyY = (screenHeight / 2.3f);
		float trophyButtonWidth = 60.0f;


		// Button Position
		CP_Color myColor = CP_Color_Create(80, 115, 130, 130); // Button Colour
		CP_Settings_Fill(myColor);
		CP_Graphics_DrawRect(playX, playY, buttonWidth, buttonHeight); // Draw "Play" Button
		CP_Graphics_DrawRect(optionsX, optionsY, buttonWidth, buttonHeight); // Draw "Options" Button
		CP_Graphics_DrawRect(creditsX, creditsY, buttonWidth, buttonHeight); // Draw "Credits" Button
		CP_Graphics_DrawRect(exitX, exitY, buttonWidth, buttonHeight); // Draw "Exit" Button

		CP_Settings_Fill(myColor);
		CP_Graphics_DrawRect(trophyX, trophyY, trophyButtonWidth, buttonHeight);
		CP_Image_Draw(trophy, 975.0, 425.0, 60, 60, 255);

		{
			// Buttton Text
			//CP_Color myColor = CP_Color_Create(255, 255, 255, 255); // Text Colour (Black)
			CP_Font_Set(myFont);
			CP_Settings_Fill(fontColour);
			CP_Font_DrawText("Play", playX + 65.0f, playY + 45.0f);
			CP_Font_DrawText("Options", optionsX + 30.0f * 1.5f, optionsY + 45.0f);
			CP_Font_DrawText("Credits", creditsX + 48.0f, creditsY + 45.0f);
			CP_Font_DrawText("Exit", exitX + 70.0f, exitY + 45.0f);
			CP_Settings_TextSize(40.0f);
		}

		// Check for mouse click
		if (CP_Input_MouseClicked())
		{
			// Only start the game if the click is inside the "Play" button area
			if (IsAreaClicked(playX + buttonWidth / 2, playY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
			{
				CP_Sound_Play(click);
				promptname_Visibility = 1;
				//CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
			}
			else if (IsAreaClicked(optionsX + buttonWidth / 2, optionsY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
			{
				CP_Sound_Play(click);
				CP_Engine_SetNextGameState(Options_Init, Options_Update, Options_Exit);
			}
			else if (IsAreaClicked(creditsX + buttonWidth / 2, creditsY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
			{
				CP_Sound_Play(click);
				CP_Engine_SetNextGameState(Credits_Init, Credits_Update, Credits_Exit);
			}
			// Only exit if the click is inside the "Exit" button area
			else if (IsAreaClicked(exitX + buttonWidth / 2, exitY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
			{
				CP_Sound_Play(click);
				CP_Engine_Terminate();  // Exit the game when "Exit" is clicked
			}
			else if (IsAreaClicked(trophyX + trophyButtonWidth / 2, trophyY + buttonHeight / 2, trophyButtonWidth, buttonHeight, mousex, mousey))
			{
				CP_Sound_Play(click);
				leaderboard_Visibility = 1;
				printf("hey, leaderbaord is at state: %d\n", leaderboard_Visibility);

			}
		}

		CP_Color myColor1 = CP_Color_Create(80, 115, 130, 180); // highlighted Button Colour
		CP_Settings_Fill(myColor1);
		if (IsAreaClicked(playX + buttonWidth / 2, playY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey)) {
			CP_Graphics_DrawRect(playX, playY, buttonWidth, buttonHeight);
		}
		else if (IsAreaClicked(optionsX + buttonWidth / 2, optionsY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
		{
			CP_Graphics_DrawRect(optionsX, optionsY, buttonWidth, buttonHeight);
		}
		else if (IsAreaClicked(creditsX + buttonWidth / 2, creditsY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
		{
			CP_Graphics_DrawRect(creditsX, creditsY, buttonWidth, buttonHeight);
		}
		else if (IsAreaClicked(exitX + buttonWidth / 2, exitY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey))
		{
			CP_Graphics_DrawRect(exitX, exitY, buttonWidth, buttonHeight);
		}
		else if (IsAreaClicked(trophyX + trophyButtonWidth / 2, trophyY + buttonHeight / 2, trophyButtonWidth, buttonHeight, mousex, mousey))
		{
			CP_Graphics_DrawRect(trophyX, trophyY, trophyButtonWidth, buttonHeight);
		}


		draw_Popup("data here");
		promptName();
	}

}

void Main_Menu_Exit(void)
{
	// Unload the background image when the main menu exits
	CP_Image_Free(&mainBg);
	CP_Sound_Free(&menuMusic);
	CP_Sound_Free(&click);
}