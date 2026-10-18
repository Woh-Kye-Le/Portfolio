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
#ifndef _BULLET_H_
#define _BULLET_H_
#include "cprocessing.h"
//#include "game.h"
//#include "enemies.h"
//#include "platform.h"
//#include "player.h"
////#include "utils.h"
//#include "boss.h"

// Forward declarations of necessary structures
struct Character;
struct Platform;
struct Enemy;
struct Boss;

/*
	* base struct for Bullet
	* x: top left x-coordinate of the Bullet, y:top left y coordinate of the Bullet
	* width: horizontal length of Bullet, height: vertical length of Bullet, speedy: x movement of bullet, speedy: y movement of bullet
	* originalFiringPointX: Original starting Point Of Bullet for X coords, originalFiringPointY: same as originalFiringPointX, but for y Coords
	* damageDealt: represents the damage dealt to the player. typeOfDamage: type of damage dealt by enemy, currently unused
*/
typedef struct Bullet {
	float x;
	float y;
	float width;
	float height;
	float speedx;
	float speedy;
	float originalFiringPointX;
	float originalFiringPointY;
	float damageDealt;
	int typeOfDamage;
	int bulletTag;
}Bullet;

void bullet_Fire(Bullet* bulletGeneric, CP_Image genericBullet);

void check_Counter_And_Fire_Bullets(int numOfRangedEnemies, int* numberOfBulletsPresent, Character* player1, Enemies arrayOfRangedEnemies[], Bullet arrayOfBullets[]);

void bullet_Collision_And_Damage(int numberOfPlatforms, int charindex, int* numberOfBulletsPresent, Character* player1, Platform platforms[], Bullet arrayOfBullets[], CP_Image genericBullet);


void check_Counter_And_Fire_Bullets_Boss(int* numberOfBulletsPresent, Character* player1, Boss* boss, Bullet arrayOfBullets[], int * berserkMode);

bool check_Collision_Player_Bullet(Character* player, Bullet* bullet);

void bullet_Collision_And_Damage_Boss(int* numberOfBulletsPresent, float * dt, Character* player1, Bullet arrayOfBullets[], CP_Image genericBullet);
//
//void check_Counter_And_Fire_Bullets_Boss(int numOfBoss, int* numberOfBulletsPresent, Character* player1, Boss arrayOfBoss[2], Bullet arrayOfBullets[100]);
//
//void bullet_Collision_And_Damage_Boss(int* numberOfBulletsPresent, Character* player1, Bullet arrayOfBullets[100]);

#endif