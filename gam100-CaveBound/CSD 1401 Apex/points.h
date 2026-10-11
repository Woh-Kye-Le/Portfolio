#pragma once

#ifndef POINTS_H
#define POINTS_H
//#include "game.h"
#include <stdbool.h>

void Main_Menu1_Init(void);
void Main_Menu1_Update(void);
void Main_Menu1_Exit(void);

// Forward declaration of Player
struct Character;

typedef struct Gem {
	float x;
	float y;
	int width;
	int height;
	bool isCollected;

	int hitboxSqWidth; //middle section for gem hitbox
	int hitboxSqHeight; 
} Gem;

#define MAX_GEMS 50

extern Gem gems[MAX_GEMS];

extern CP_Image gemImage;
extern int playerPoints;

void initialisingPointSystem();
void drawPoints(int points);

void intialising_Gems();
void intialising_GemsLvl2();
void draw_Gems(int level);

bool checkCollisionWithGem(Character* player, Gem* gem, int* playerPoints);


#endif