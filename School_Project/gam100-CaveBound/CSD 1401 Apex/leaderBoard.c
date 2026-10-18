//#include "points.h"
//#include "game.h"
#include <stdio.h>
#include "cprocessing.h"
#include <string.h>
#include "game.h"
#include "instructions.h"

//saves player points into text file, and display them at the leaderboard in main menu 

#define MAX_PLAYERS 5
#define MAX_TOTAL_PLAYERS 100
#define MAX_NAME_LENGTH 15

int leaderboard_Visibility = 0; // no pop up
int promptname_Visibility = 0;
char playerName[MAX_NAME_LENGTH] = ""; //player name stored here

//typedef struct {
//    char name[MAX_NAME_LENGTH];
//    int points;
//} Player;

CP_Color myColor;
CP_Color myColor2;
CP_Image boardImage;
CP_Sound click;

int nameAttempted = 0;

/*
    * Done By: Jia Hui, Jethro(Close Button)
    * this func is a continuosly running func in mainmenu, centering around the extern int promptname.
    * As long as promptname is not zero, we basically open up the box for the player to enter in their username for tracking the player points. Regarding the input, we check if the input
    * from the player is null, if it is, we will prompt again for the player to enter name. Unless a valid name is entered, the player would not be able to proceed. A player can return to the main menu
    * otherwise if they do not wish to continue via the x button. AFter successfully entering a name, the visibility of the box will be set to zero again via the extern int promptname_visibility.
    * The string of chars are saved into the extern char array playerName.
*/

void promptName() {

    myColor = CP_Color_Create(80, 115, 130, 130);
    myColor2 = CP_Color_Create(80, 115, 130, 250);

    if (promptname_Visibility) {
        click = CP_Sound_Load("Assets/clicking.wav");
        float popupWidthName = 500.0f;
        float popupHeightName = 200.0f;
        float popupNameX = (CP_System_GetWindowWidth() - popupWidthName) / 2;
        float popupNameY = (CP_System_GetWindowHeight() - popupHeightName) / 2;

        CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
        CP_Graphics_DrawRect(popupNameX, popupNameY, popupWidthName, popupHeightName);

        //input box
        float inputWidthName = 300.0f;
        float inputHeightName = 50.0f;
        float inputNameX = (CP_System_GetWindowWidth() - inputWidthName) / 2;
        float inputNameY = (CP_System_GetWindowHeight() - inputHeightName) / 2;

        CP_Settings_Fill(myColor);
        CP_Graphics_DrawRect(inputNameX, inputNameY, inputWidthName, inputHeightName);

        //input inside the box
        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Font_DrawText(playerName, inputNameX + 10, inputNameY + 35);

        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Font_DrawText("Enter your name:", inputNameX + (inputWidthName - 260) / 2, inputNameY - 30);

        //warning to player to enter name
        if (nameAttempted && strlen(playerName) == 0) {
            CP_Settings_Fill(CP_Color_Create(255, 0, 0, 255)); // red color for warning
            CP_Font_DrawText("Enter your name", inputNameX + (inputWidthName - 230) / 2 - 25, inputNameY + inputHeightName - 15);
        }

        //input
        int length = (int)strlen(playerName);

        //check backspace
        if (CP_Input_KeyTriggered(KEY_BACKSPACE) && length > 0) {
            playerName[--length] = '\0'; //remove last letter
        }
        //check enter
        else if (CP_Input_KeyTriggered(KEY_ENTER)) {
            if (length > 0) { //check if player has entered at least one letter
                CP_Engine_SetNextGameState(instructions_init, instructions_update, instructions_exit);
                playerName[length] = '\0'; //null-terminate the name
                promptname_Visibility = 0;
            }
            else {
                nameAttempted = 1;
            }
        }
        else {
            for (int key = 'A'; key <= 'Z'; ++key) { //uppercase
                if (CP_Input_KeyTriggered(key) && length < MAX_NAME_LENGTH - 1) {
                    playerName[length++] = key;
                    playerName[length] = '\0';
                }
            }
            for (int key = 'a'; key <= 'z'; ++key) { //lowercase
                if (CP_Input_KeyTriggered(key) && length < MAX_NAME_LENGTH - 1) {
                    playerName[length++] = key;
                    playerName[length] = '\0';
                }
            }
            if (CP_Input_KeyTriggered(KEY_SPACE) && length < MAX_NAME_LENGTH - 1) {
                playerName[length++] = ' ';
                playerName[length] = '\0';
            }
        }
        //close btn cords
        float closeButtonWidth = 50.0f;
        float closeButtonHeight = 30.0f;
        float closeButtonX = popupNameX + popupWidthName - closeButtonWidth - 10;
        float closeButtonY = popupNameY + 20;

        //close btn
        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Font_DrawText("x", closeButtonX + closeButtonWidth / 2, closeButtonY + closeButtonHeight / 2);

        if (CP_Input_MouseClicked()) {
            float mouseX = CP_Input_GetMouseX();
            float mouseY = CP_Input_GetMouseY();

            float closeButtonCenterX = closeButtonX + closeButtonWidth / 2;
            float closeButtonCenterY = closeButtonY + closeButtonHeight / 2;

            if (IsAreaClicked(closeButtonCenterX, closeButtonCenterY, closeButtonWidth, closeButtonHeight, mouseX, mouseY)) {
                CP_Sound_Play(click);
                promptname_Visibility = 0;
            }
        }
    }
}

/*
    * Done By: JiaHui
    * This function basically opens the player points text file, and appends the player name and points to it to update the file after the end of a game.
*/
void savePlayerPoints(const char* playerName, int playerPoints) {
    FILE* pointsFile = fopen("player_points.txt", "a");
    if (pointsFile == NULL) {
        //printf("error opening file\n");
        return;
    }
    fprintf(pointsFile, "%s ", playerName);
    fprintf(pointsFile, "%d\n", playerPoints);
    fclose(pointsFile);
    //printf("points saved successfully!\n");
}
/*
    *Done By: Jia Hui
    *This function basically loops through the struct of players, check the points of each player, and rearrange them first by points, and if both have the same points, rearrange them alphabetically.
*/
static void sortPlayersByPointsAndName(Player* players, int playerCount) {
    for (int i = 0; i < playerCount - 1; i++) {
        for (int j = 0; j < playerCount - i - 1; j++) {
            //sort by higher points
            if (players[j].points < players[j + 1].points) {
                // Swap players
                Player temp = players[j];
                players[j] = players[j + 1];
                players[j + 1] = temp;
            }
            //sort by name alphabetically
            else if (players[j].points == players[j + 1].points) {
                if (strcmp(players[j].name, players[j + 1].name) > 0) {
                    //changing positions if a letter has higher precedence
                    Player temp = players[j];
                    players[j] = players[j + 1];
                    players[j + 1] = temp;
                }
            }
        }
    }
}

/*
    *Done By: Jia Hui
    *This func basically opens the text file of the player points, reads the values in it, and loads them until all the players have been read together with the respective points, and returns the number of players with points.
*/

int loadPlayerPoints(Player* players, int maxPlayers) {
    FILE* pointsFile = fopen("player_points.txt", "r");
    if (pointsFile == NULL) {
        //printf("error opening file for reading.\n");
        return 0;
    }

    //printf("file successfully opened, reading from file\n");

    int count = 0;
    while (count < maxPlayers &&
        fscanf_s(pointsFile, "%s %d", players[count].name, (unsigned int)sizeof(players[count].name),
            &players[count].points) == 2) {
        //printf("Read player: %s, Points: %d\n", players[count].name, players[count].points);
        count++;
    }

    fclose(pointsFile);
    //printf("Leaderboard loaded successfully! %d players read.\n", count);

    return count;
}

/*
    * Done By: Jia Hui
    * this func is a continuosly running func in mainmenu, centering around the extern int leaderboard_Visibility.
    * As long as leaderboard_Visibility is not zero, we basically open up the leaderboard for the player to view the top 5 players in terms of points. As long as the extern int leaderboard_Visibility is 1, the leaderboard 
    * will be open. You can close this by clicking on the close button of the leaderboard defined by x, which sets leaderboard_Visibility to 0.
*/
void draw_Popup(const char* title) {
    if (!leaderboard_Visibility) return; //not drawn if state is 0
    click = CP_Sound_Load("Assets/clicking.wav");

    boardImage = CP_Image_Load("./Assets/leaderboard.png");

    Player allPlayers[MAX_TOTAL_PLAYERS];
    int totalPlayerCount = loadPlayerPoints(allPlayers, MAX_TOTAL_PLAYERS);

    //sorts player by points and name 
    sortPlayersByPointsAndName(allPlayers, totalPlayerCount);

    //top 5 players for display
    Player topPlayers[MAX_PLAYERS] = {0};
    int topCount = (totalPlayerCount < MAX_PLAYERS) ? totalPlayerCount : MAX_PLAYERS;
    for (int i = 0; i < topCount; i++) {
        topPlayers[i] = allPlayers[i];
    }

    //leaderboard popup
    float popupWidth = 600.0f;
    float popupHeight = 500.0f;
    float popupX = (CP_System_GetWindowWidth() - popupWidth) / 2;
    float popupY = (CP_System_GetWindowHeight() - popupHeight) / 2;

    CP_Image_Draw(boardImage, popupX + popupWidth / 2, popupY + popupHeight / 2, popupWidth, popupHeight, 255);

    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    //headers
    float headerY = popupY + 170;
    CP_Font_DrawText("Rank", popupX + 50, headerY);
    CP_Font_DrawText("Name", popupX + 200, headerY);
    CP_Font_DrawText("Points", popupX + popupWidth - 200, headerY);

    //top players rank, name, and points
    float startY = popupY + 230;
    float lineSpacing = 50;
    for (int i = 0; i < topCount; i++) {
        char rankEntry[5];
        snprintf(rankEntry, sizeof(rankEntry), "%d", i + 1);
        CP_Font_DrawText(rankEntry, popupX + 75, startY + i * lineSpacing);

        CP_Font_DrawText(topPlayers[i].name, popupX + 200, startY + i * lineSpacing);

        char pointsEntry[10];
        snprintf(pointsEntry, sizeof(pointsEntry), "%d", topPlayers[i].points);
        CP_Font_DrawText(pointsEntry, popupX + popupWidth - 180, startY + i * lineSpacing);
    }

    //close btn cords
    float closeButtonWidth = 50.0f;
    float closeButtonHeight = 30.0f;
    float closeButtonX = popupX + popupWidth - closeButtonWidth - 10;
    float closeButtonY = popupY + 20;

    //close btn
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Font_DrawText("x", closeButtonX + closeButtonWidth / 2, closeButtonY + closeButtonHeight / 2);

    if (CP_Input_MouseClicked()) {
        float mouseX = CP_Input_GetMouseX();
        float mouseY = CP_Input_GetMouseY();

        float closeButtonCenterX = closeButtonX + closeButtonWidth / 2;
        float closeButtonCenterY = closeButtonY + closeButtonHeight / 2;

        if (IsAreaClicked(closeButtonCenterX, closeButtonCenterY, closeButtonWidth, closeButtonHeight, mouseX, mouseY)) {
            CP_Sound_Play(click);
            leaderboard_Visibility = 0;
        }
    }
}
