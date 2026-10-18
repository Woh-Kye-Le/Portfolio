/*
@file       this is file utils.h
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    projCavebound
@date       30/10/24
@brief      this file contains a collection of functions to be called, by declaring utils.h, other source files will be able to call upon the the functions in
			utils.h to be used. Furthermore, the definitions of the functions can be assigned via declaring utils.h in other source files, eg. utils.c.
*/
//#pragma once
#ifndef _ENEMY_H_
#define _ENEMY_H_
#include "cprocessing.h"
//#include "game.h"
//#include "player.h"

struct Character;

/*
	* base struct for Enemies
	* x: top left x-coordinate of the Enemy, y:top left y coordinate of the Enemy
	* width: horizontal length of Enemy, height: vertical length of Enemy
	* health: represents the remaining health of the enemy, fireRate: Represents the time taken for each shot or attack
	* counterToNextShot: Represents an incremnetal counter, once this matches the fire rate, the attack is made and the counter is reset to zero.
	* isAlive: Check if Enemy is Alive, else remove Enemy (unused for now), typeOfEnemy: determines if enemy is ranged, melee and potenitally more
*/

typedef struct Enemies {   // Structure declaration
	float x;           // X Coordinates of Enemies
	float y;       // Y Coordinates of Enemies
	float width; // width of Enemy
	float height; // height of Enemy
	float health; // Enemy Health
	int fireRate; // Fire Rate/AttackRate
	int counterToNextShot; //Counter for Fire/AttackRate
	bool isAlive; // Checks if Enemy is Alive
	int typeOfEnemy; // If enemy is melee or ranged, 0 if ranged, 1 if not
	int attackType; // 1 - Blunt, 2 - Sharp, 3- EM, 4-Kinetic, 5- Thermal, 6-Explosive
	int state;  // 0 Patrolling, 1 Attacking, 2 React Time
	float verticalvelocity; // vertical jumping speed of char
}Enemies;// End the structure with a semicolon

/*Declaration Of func intialise melee enemies for level 1*/
int initialise_Level1_Enemies_Melee(Enemies meleeEnemies[]);

/*Declaration Of func intialise ranged enemies for level 1*/
int initialise_Level1_Enemies_Ranged(Enemies rangedEnemies[]);

/*Declaration Of func intialise melee enemies for level 2*/
int initialise_Level2_Enemies_Melee(Enemies meleeEnemies[]);

/*Declaration Of func intialise ranged enemies for level 2*/
int initialise_Level2_Enemies_Ranged(Enemies rangedEnemies[]);

/*Declaration of func to create ranged enemy*/
void create_Ranged_Enemy(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour, CP_Image monsterRangedLeft);

/* declaration of func to check if player has collided with enemy*/
bool check_Collision_Player_Enemies(Character* player, Enemies* enemy);

/* declaration of func to handle player enemy collision*/
void handle_Collision_Player_Enemies(Character* player, Enemies* enemy, float* dt);

/* declaration of func to handle melee enemy patrolling, attacking, and reaction states*/
void detect_Player_Melee(int numberOfMeleeEnemies, int currentChar, float const* pauseTimer, float const* dt, float* reactionTimer, float meleeEnemyOriginalX[], float meleeEnemyOriginalY[], Character* player, Enemies arrayOfMeleeEnemies[], CP_Image monsterMeleeLeftAttack, CP_Image monsterMeleeRightAttack, CP_Image player1HurtRight, CP_Image player1HurtLeft, CP_Image player2HurtRight, CP_Image player2HurtLeft, CP_Image monsterMeleeLeftWalk, CP_Image monsterMeleeRightWalk);

#endif _ENEMY_H_