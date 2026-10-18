#include "cprocessing.h"
#include "game.h"
#include "utils.h"
#include <stdio.h>   
#include <stdlib.h>
#include <string.h>

#define MAX_CREDITS_LINES 50       // Maximum number of lines in credits
#define MAX_LINE_LENGTH 100        // Maximum length for each line in credits

char creditsText[MAX_CREDITS_LINES][MAX_LINE_LENGTH]; // Array to store each line of credits
int numLines = 0; // Number of lines read from the file

CP_Image creditBg; // Background image variable

// Function to load credits from a file
/*
    *This is relatively simple, we open the file containing the credits, check if the file is empty, if it is not, we retrive the string of chars until newline, saves it in creditsText[Linex][string of Chars], where x is the current line index
    * in the text file. By doing do, we have essentially stored the values of the file into the char array creditsText.
*/
void LoadCreditsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r"); // Open the file in read mode
    if (file == NULL) {
        printf("Error opening credits file: %s\n", filename);
        return;
    }

    numLines = 0;
    while (fgets(creditsText[numLines], MAX_LINE_LENGTH, file) != NULL && numLines < MAX_CREDITS_LINES) {
        // Remove newline character if present
        creditsText[numLines][strcspn(creditsText[numLines], "\n")] = '\0';
        numLines++;
    }

    fclose(file); // Close the file after reading
}
void Credits_Init(void) {
    LoadCreditsFromFile("./Assets/credits.txt"); // Load credits from specified path
    creditBg = CP_Image_Load("./Assets/MainMenu.jpg"); // Load background image
}

void Credits_Update(void) {
    /*
        *Done By: Lumi
        * This simply draws the background, and then proceeds to print the values in the char array of which is obtained from LoadCredits from a file into
        * draw text. For each line, we draw out the text and then move down by the spacing defined.
        * Lastly, to return to the main menu, the player simply needs to press the escape key.
    */
    float screenWidth = (float)CP_System_GetWindowWidth();
    float screenHeight = (float)CP_System_GetWindowHeight();
    // Draw the background image
       CP_Image_Draw(creditBg, screenWidth / 2, screenHeight / 2, screenWidth, screenHeight, 255);

    // Set text properties
    float textSize = 30.0f; // Set the desired text size
    CP_Settings_TextSize(textSize);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255)); // White text color

    // Set custom starting position for the text
    float customX = 710.0f; // Adjust this to set the horizontal position of the text
    float customY = 300.0f; // Adjust this to set the vertical position of the text
    float lineSpacing = 30.0f; // Spacing between lines

    // Display each line of credits at the specified position
    float yPosition = customY;
    for (int i = 0; i < numLines; i++) {
        // Draw each line of text starting at (customX, customY), then move down by lineSpacing
        CP_Font_DrawText(creditsText[i], customX, yPosition);
        yPosition += lineSpacing; // Move down for the next line
    }
    // Check if ESC key is pressed to go back to the main menu
    if (CP_Input_KeyTriggered(KEY_ESCAPE)) {
        CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
    }
}

// Cleanup function to free the background image
void Credits_Exit(void) {
    CP_Image_Free(&creditBg); // Free the background image
}