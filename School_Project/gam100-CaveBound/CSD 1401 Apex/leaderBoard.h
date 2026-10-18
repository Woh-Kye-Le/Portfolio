#pragma once

//#define MAX_PLAYERS 10 
#define MAX_NAME_LENGTH 15

typedef struct {
    char name[MAX_NAME_LENGTH];
    int points;
} Player;

void draw_Popup(const char* message);
void promptName();
void savePlayerPoints(const char* playerName, int playerPoints);
int loadPlayerPoints(Player *players, int maxPlayers);

/*
    * Extern vars are declared here, where any source file will be able to access them.
*/
extern int leaderboard_Visibility;
extern int isNameEntered;
extern int promptname_Visibility;
extern char playerName[MAX_NAME_LENGTH];
