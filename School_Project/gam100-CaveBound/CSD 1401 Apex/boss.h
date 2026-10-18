//#pragma once
#ifndef _BOSS_H_
#define _BOSS_H_

#include<cprocessing.h>
#include<stdbool.h>
//#include "player.h"



#define buffer (3.0f) //ensure collision is smoother 

struct Character;

typedef struct Boss { //structure of Boss
	float x; //x coordinate of Boss
	float y; //y coordinate of Boss
	float width; //width of Boss
	float height; //height of Boss
	float speedX; //horizontal speed of Boss
	float speedY; //vertical speed of Boss
	float verticalVelocity; //vertical velocity of Boss
	int fireRate; //fire rate/attack rate
	int counterToNextShot; //counter for fire/attack rate 
	float health; //health of Boss
	bool isAlive; //check if Boss is alive
	int state;  // 0 Patrolling, 1 Attacking, 2 React Time 
	int attackType; // 1 - Blunt, 2 - Sharp, 3- EM, 4-Kinetic, 5- Thermal, 6-Explosive
	//int reactionCounter;//counter for enemies to react to player
}Boss; //end structure



int initialise_Level1_Boss(Boss boss[]);

int initialise_Level2_Boss(Boss boss[]);

void switch_Boss_state(Boss* arrayOfBoss);

void detect_Player_Boss(int numberOfBoss, Character* player, Boss* boss, float* dt);

bool check_Collision_Player_Boss(Character* player, Boss* boss);
bool check_Collision_Player_Boss_Top(Character* player, Boss* boss);
bool check_Collision_Player_Boss_Bottom(Character* player, Boss* boss);
void handle_Collision_Player_Boss(Character* player, Boss* boss, float const* dt);

void create_Boss(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour);
void drawBossHealthBar(Boss* arrayofBoss);
#endif 
