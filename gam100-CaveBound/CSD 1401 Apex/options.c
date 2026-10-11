#include "cprocessing.h"
#include "game.h"
#include "options.h"
#include "mainmenu.h"
#include "utils.h"

// Variables for background and text font
CP_Font optionFont;
CP_Image optionBg;

void Options_Init(void) {
    optionFont = CP_Font_Load("./Assets/Exo2-Regular.ttf");
    optionBg = CP_Image_Load("./Assets/MainMenu.jpg"); // Load a background image
    //CP_Settings_RectMode(CP_POSITION_CENTER);
}

/*
    * Done By: Lumi
    * This basically draws the background for options, and creates an exit button. When clicked, the player returns to main menu.
*/
void Options_Update(void) {
    // Draw the background image, scaled to fill the screen
    CP_Image_Draw(optionBg, (float)CP_System_GetWindowWidth() / 2, (float)CP_System_GetWindowHeight() / 2,
        (float)CP_System_GetWindowWidth(), (float)CP_System_GetWindowHeight(), 255);

    // Define "Back" button properties
    float screenWidth = (float)CP_System_GetWindowWidth();
    float screenHeight = (float)CP_System_GetWindowHeight();
    float backX = screenWidth / 2 -75.0f;
    float backY = screenHeight - 150.f;
    float buttonWidth = 200.0f;
    float buttonHeight = 60.0f;

    // Draw the "Back" button
    CP_Color myColor = CP_Color_Create(80, 115, 130, 130); // Button Colour
    CP_Settings_Fill(myColor);
    CP_Graphics_DrawRect(backX, backY, buttonWidth, buttonHeight);
    CP_Settings_TextSize(40.0f);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255)); // Black text color
    CP_Font_DrawText("Back", backX + 60, backY + 40);

    // Get mouse position
    float mousex = CP_Input_GetMouseX();
    float mousey = CP_Input_GetMouseY();

    // Check if the "Back" button is clicked
    if (CP_Input_MouseClicked() && IsAreaClicked(backX + buttonWidth / 2, backY + buttonHeight / 2, buttonWidth, buttonHeight, mousex, mousey)) {
        SetGameState(GAME_STATE_MAIN_MENU); // Transition back to main menu
        CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);  // Back to "Main menu" when clicked
        
    }
}

void Options_Exit(void) {
    CP_Image_Free(&optionBg); // Free the background image
}