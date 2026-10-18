#include "cprocessing.h"
//#include "gameover.h"
#include "game.h"
#include "points.h"

CP_Font font; //set variable for font

/*
* Done By: Lumi and Jethro
* This is a relatively simple handling of the game over screen, where in the case where the player has basically reach 0 health, the game stops and shows that it is game over.
* E is used to return to the main menu, where the player points are reset back to 0, after being saved in game.c.

*/
void ShowGameOverScreen() {
    // Clear the background to black
    CP_Graphics_ClearBackground(CP_Color_Create(0, 0, 0, 255));
    //load font
    font = CP_Font_Load("./Assets/HanaleiFill-Regular.ttf");
    //set font
    CP_Font_Set(font);
    // Set text colour to white
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    for (int i = 0; playerName[i]; i++)
    {
        playerName[i] = '\0';
    }
    create_Rect((float)CP_System_GetWindowWidth() / 2 - 500.f, (float)CP_System_GetWindowHeight() / 2 - 100.f, 1000.f, 200.f, CP_Color_Create(255, 255, 255, 255));
    // Display "Game Over" message
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Settings_TextSize(50.0f);
    CP_Font_DrawText("GAME OVER, Press 'E' to Main Menu", (float)CP_System_GetWindowWidth() / 2 - 350.f, (float)CP_System_GetWindowHeight() / 2 - 20.f);

    //// Display restart prompt
    //CP_Font_DrawText("Press 'R' to Restart", CP_System_GetWindowWidth() / 2 - 150, CP_System_GetWindowHeight() / 2 + 50);
    
    //// Handle restart input
    //if (CP_Input_KeyTriggered(KEY_R)) {
    //    CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
    //    //ResetGameState(); // Reset the game when 'R' is pressed

    //}
    // Handle restart input
    if (CP_Input_KeyTriggered(KEY_E)) {
        promptname_Visibility = 0;
        playerPoints = 0;
        //ResetGameState(); // Reset the game when 'R' is pressed
        CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
    }
}
