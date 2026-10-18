#pragma once

#include <stdbool.h>
#include "player.h"


#define NUM_COLLECTABLES 10
typedef struct {
	float x;
	float y;
	float radius;
	int type; //0 - speed boost , 1 - restore health
	bool isCollected;
} Collectable;

extern Collectable collectables[NUM_COLLECTABLES];

void loadImage(void);
bool checkCollisionWithCollectables(Character* player, Collectable* collectables, int numCollectables);
void initialiseCollectables(Collectable* collectables);
void initialiseCollectablesLvl2(Collectable* collectables);
void spawnCollectables(Collectable* collectables);
void drawCollectables(Collectable* collectables, int numCollectables);

