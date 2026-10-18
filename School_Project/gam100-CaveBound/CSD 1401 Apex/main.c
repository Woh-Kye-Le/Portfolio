//---------------------------------------------------------
// file:	main.c
// author:	Jethro Sung	
// email:	sung.h@digipen.edu
//
// brief:	Main entry point for the sample project
//			of the CProcessing library
//
// Copyright © 2020 DigiPen, All rights reserved.
//---------------------------------------------------------

#include "cprocessing.h"
#include "mainmenu.h"
#include "game.h"

GameState currentGameState = GAME_STATE_MAIN_MENU;

int main(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
	CP_System_SetWindowSize(1600, 900);
	CP_Engine_Run(0);
	return 0;
}