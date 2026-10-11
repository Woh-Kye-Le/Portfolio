/*
@file       instructions.c
@author     Celest Chang (c.qixuancelest)
@course     CSD1401
@section    a
@project    projCavebound
@date       11/11/24
@brief      This file contains the function definition of instructions_init, instructions_update and instructions_exit. This page allows user
to view how to play and be able to go back to main menu or start the game.
*/

#include "cprocessing.h"
//#include "instructions.h"
#include "game.h"
//#include "mainmenu.h"

//initialise font and colour
CP_Font font;
CP_Color black;
CP_Color lightgrey;
CP_Color lightyellow;
CP_Color brownred;
CP_Color white;
CP_Color fadewhite;

//graphics
CP_Image player1JumpRight;
CP_Image player1WalkLeft;
CP_Image player2WalkLeft;
CP_Image rangedEnemy;
CP_Image gemsInstruct;
CP_Image mainBg;

//alignment
CP_TEXT_ALIGN_HORIZONTAL horizontal = CP_TEXT_ALIGN_H_CENTER;
CP_TEXT_ALIGN_VERTICAL vertical = CP_TEXT_ALIGN_V_MIDDLE;

//audio
CP_Sound instructionsMenu;

void instructions_init(void) {
	//set sound
	instructionsMenu = CP_Sound_Load("Assets/instructionMusic.wav");
	CP_Sound_PlayAdvanced(instructionsMenu, 0.2f, 1.0f, TRUE, CP_SOUND_GROUP_MUSIC);

	//clear background
	CP_Graphics_ClearBackground(CP_Color_Create(128, 128, 128, 255));
	//load font
	font = CP_Font_Load("./Assets/HanaleiFill-Regular.ttf");
	//set colours
	black = CP_Color_Create(0, 0, 0, 255);
	lightgrey = CP_Color_Create(194, 197, 204, 255);
	lightyellow = CP_Color_Create(243, 245, 179, 255);
	brownred = CP_Color_Create(161, 61, 45, 255);
	white = CP_Color_Create(223, 228, 240, 255);
	fadewhite = CP_Color_Create(223, 228, 240, 200);

	//set rectmode and text alignment
	CP_Settings_RectMode(CP_POSITION_CENTER);
	horizontal = CP_TEXT_ALIGN_H_CENTER;
	vertical = CP_TEXT_ALIGN_V_MIDDLE;
	CP_Settings_TextAlignment(horizontal, vertical);
	//load images
	mainBg = CP_Image_Load("./Assets/MainMenu.jpg");
	rangedEnemy = CP_Image_Load("./Assets/monsterwalk1.png");
	player1JumpRight = CP_Image_Load("./Assets/player1jump.png");
	gemsInstruct = CP_Image_Load("./Assets/GEM2.png");
	player2WalkLeft = CP_Image_Load("./Assets/player2walk1LEFT.png");
	player1WalkLeft = CP_Image_Load("./Assets/player1walk1LEFT.png");


}
/*
	* Done By: Celest
	* This basically writes the text for the various instructions that provides a brief explanation of game mechanics.
	* On button press B, the player returns back to the main menu, on button M, the player starts the game.
*/
void instructions_update(void) {
	int windowWidth = CP_System_GetWindowWidth();
	int windowHeight = CP_System_GetWindowHeight();
	

	CP_Font_Set(font);
	//CP_Image_Draw(mainBg, CP_System_GetWindowWidth() / 2.0f, CP_System_GetWindowHeight() / 2.0f,CP_System_GetWindowWidth(), CP_System_GetWindowHeight(), 255); //background

	//chatbox to for jump on enemy instruction
	CP_Settings_Fill(fadewhite);
	CP_Graphics_DrawRect((windowWidth) * 0.35f, (windowHeight) * 0.37f, (windowWidth) * 0.25f, (windowHeight) * 0.08f); //textbox
	CP_Settings_Fill(brownred);
	CP_Settings_TextSize(30.0f);
	CP_Font_DrawText("Jump on enemy to defeat them!", (windowWidth) * 0.35f, (windowHeight) * 0.37f); //text
	CP_Settings_Fill(fadewhite);
	CP_Graphics_DrawRect((windowWidth) * 0.15f, (windowHeight) * 0.4f + 45, (windowWidth) * 0.18f, (windowHeight) * 0.17f); //textbox for boss note
	CP_Settings_Fill(brownred);
	CP_Settings_TextSize(23.0f);
	CP_Font_DrawText("Note:", (windowWidth) * 0.15f, (windowHeight) * 0.4f); //text for boss note
	CP_Font_DrawText("Boss has a different mechanic", (windowWidth) * 0.15f, (windowHeight) * 0.4f + 30); //text
	CP_Font_DrawText("from normal monsters.", (windowWidth) * 0.15f, (windowHeight) * 0.4f + 60); //text
	CP_Font_DrawText("Learn the mechanics as you play!", (windowWidth) * 0.15f, (windowHeight) * 0.4f + 90); //text
	CP_Image_Draw(rangedEnemy, windowWidth * 0.35f, windowHeight * 0.57f, 125.0f, 125.0f, 255); //enemy, below player1
	CP_Image_Draw(player1JumpRight, windowWidth * 0.34f, windowHeight * 0.48f, 100.0f, 100.0f, 255); //player1, above enemy


	//chatbox to collect gems instruction
	CP_Settings_Fill(fadewhite);
	CP_Graphics_DrawRect((windowWidth) * 0.65f, (windowHeight) * 0.37f, (windowWidth) * 0.23f, (windowHeight) * 0.08f); //textbox
	CP_Settings_Fill(brownred);
	CP_Settings_TextSize(30.0f);
	CP_Font_DrawText("Collect gems to earn points!", (windowWidth) * 0.65f, (windowHeight) * 0.37f); //text
	CP_Image_Draw(gemsInstruct, windowWidth * 0.61f, windowHeight * 0.5f, 50.0f, 50.0f, 255); //gem, left of player2
	CP_Image_Draw(player2WalkLeft, windowWidth * 0.65f, windowHeight * 0.5f, 100.0f, 110.0f, 255); //player2 facing left, right of gem

	//header for keys
	CP_Settings_Fill(fadewhite);
	CP_Graphics_DrawRect((windowWidth) / 2.0f, (windowHeight) * 0.80f, (windowWidth) / 1.2f, (windowHeight) * 0.3f);
	CP_Settings_Fill(brownred);
	CP_Settings_TextSize(60.0f);
	CP_Font_DrawText("Keys", (windowWidth) / 2.45f, (windowHeight) * 0.70f);

	//Q - change character
	CP_Settings_Fill(lightgrey);
	CP_Graphics_DrawRect((windowWidth) * 0.15f, (windowHeight) * 0.83f, (windowWidth) * 0.04f, (windowHeight) * 0.07f);
	CP_Settings_Fill(black);
	CP_Settings_TextSize(40.0f);
	CP_Font_DrawText("Q", (windowWidth) * 0.15f, (windowHeight) * 0.83f);
	CP_Settings_TextSize(30.0f);
	CP_Font_DrawText("Change", (windowWidth) * 0.15f, (windowHeight) * 0.89f);
	CP_Font_DrawText("Character", (windowWidth) * 0.15f, (windowHeight) * 0.89f + 25);

	CP_Image_Draw(player1WalkLeft, windowWidth * 0.12f, windowHeight * 0.73f, 60.0f, 65.0f, 255); //player1 facing left
	CP_Image_Draw(player2WalkLeft, windowWidth * 0.18f, windowHeight * 0.73f, 60.0f, 65.0f, 255); //player2 facing left

	//A - move left
	CP_Settings_Fill(lightgrey);
	CP_Graphics_DrawRect((windowWidth) * 0.3f, (windowHeight) * 0.83f, (windowWidth) * 0.04f, (windowHeight) * 0.07f);
	CP_Settings_Fill(black);
	CP_Settings_TextSize(40.0f);
	CP_Font_DrawText("A", (windowWidth) * 0.3f, (windowHeight) * 0.83f);
	CP_Settings_TextSize(30.0f);
	CP_Font_DrawText("Move Left", (windowWidth) * 0.3f, (windowHeight) * 0.89f);

	//D - move right
	CP_Settings_Fill(lightgrey);
	CP_Graphics_DrawRect((windowWidth) * 0.39f, (windowHeight) * 0.83f, (windowWidth) * 0.04f, (windowHeight) * 0.07f);
	CP_Settings_Fill(black);
	CP_Settings_TextSize(40.0f);
	CP_Font_DrawText("D", (windowWidth) * 0.39f, (windowHeight) * 0.83f);
	CP_Settings_TextSize(30.0f);
	CP_Font_DrawText("Move Right", (windowWidth) * 0.39f, (windowHeight) * 0.89f);

	//Space - jump
	CP_Settings_Fill(lightgrey);
	CP_Graphics_DrawRect((windowWidth) * 0.7f, (windowHeight) * 0.83f, (windowWidth) * 0.3f, (windowHeight) * 0.07f);
	CP_Settings_Fill(black);
	CP_Settings_TextSize(40.0f);
	CP_Font_DrawText("Space", (windowWidth) * 0.7f, (windowHeight) * 0.83f);
	CP_Settings_TextSize(30.0f);
	CP_Font_DrawText("Jump", (windowWidth) * 0.7f, (windowHeight) * 0.89f);

	//header
	CP_Settings_Fill(white);
	CP_Graphics_DrawRect((windowWidth) / 2.0f, (windowHeight) / 5.0f, (windowWidth) * 0.45f, (windowHeight) * 0.2f);
	CP_Settings_Fill(brownred);
	CP_Settings_TextSize(60.0f);
	CP_Font_DrawText("> How to Play <", (windowWidth) / 2.0f, (windowHeight) / 6.5f);

	//prompt user to start game or go back to main menu
	CP_Settings_Fill(brownred);
	CP_Settings_TextSize(40.0f);
	CP_Font_DrawText("Press 'M' to start game.", (windowWidth) / 2.0f, (windowHeight) / 4.0f);
	CP_Font_DrawText("Press 'B' to go back to main menu.", (windowWidth) / 2.0f, (windowHeight) / 4.8f);

	//if user presses 'M', start game by going to game.c
	if (CP_Input_KeyDown(KEY_M)) {
		CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
	}
	//if user presses 'B', go back to mainmenu.c
	if (CP_Input_KeyDown(KEY_B)) {
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
	}

	/*---------------if implementing key testing by player-------------*/
	//if user pressed 'Q', light up Q key
	if (CP_Input_KeyDown(KEY_Q)) {
		CP_Settings_Fill(lightyellow);
		CP_Graphics_DrawRect((windowWidth) * 0.15f, (windowHeight) * 0.83f, (windowWidth) * 0.04f, (windowHeight) * 0.07f);
		CP_Settings_Fill(black);
		CP_Settings_TextSize(50.0f);
		CP_Font_DrawText("Q", (windowWidth) * 0.15f, (windowHeight) * 0.83f);
	}

	//if user pressed 'A', light up A key
	if (CP_Input_KeyDown(KEY_A)) {
		CP_Settings_Fill(lightyellow);
		CP_Graphics_DrawRect((windowWidth) * 0.3f, (windowHeight) * 0.83f, (windowWidth) * 0.04f, (windowHeight) * 0.07f);
		CP_Settings_Fill(black);
		CP_Settings_TextSize(50.0f);
		CP_Font_DrawText("A", (windowWidth) * 0.3f, (windowHeight) * 0.83f);
	}

	//if user pressed 'D', light up D key
	if (CP_Input_KeyDown(KEY_D)) {
		CP_Settings_Fill(lightyellow);
		CP_Graphics_DrawRect((windowWidth) * 0.39f, (windowHeight) * 0.83f, (windowWidth) * 0.04f, (windowHeight) * 0.07f);
		CP_Settings_Fill(black);
		CP_Settings_TextSize(50.0f);
		CP_Font_DrawText("D", (windowWidth) * 0.39f, (windowHeight) * 0.83f);
	}

	//if user pressed 'space', light up space key
	if (CP_Input_KeyDown(KEY_SPACE)) {
		CP_Settings_Fill(lightyellow);
		CP_Graphics_DrawRect((windowWidth) * 0.7f, (windowHeight) * 0.83f, (windowWidth) * 0.3f, (windowHeight) * 0.07f);
		CP_Settings_Fill(black);
		CP_Settings_TextSize(50.0f);
		CP_Font_DrawText("Space", (windowWidth) * 0.7f, (windowHeight) * 0.83f);
	}

}

void instructions_exit(void) {
	//reset rectmode and text alignment to default
	CP_Settings_RectMode(CP_POSITION_CORNER);
	CP_TEXT_ALIGN_HORIZONTAL horizontal = CP_TEXT_ALIGN_H_LEFT;
	CP_TEXT_ALIGN_VERTICAL vertical = CP_TEXT_ALIGN_V_BASELINE;
	CP_Settings_TextAlignment(horizontal, vertical);
	CP_Sound_Free(&instructionsMenu);
}