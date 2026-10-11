/*
@file       this is file utils.h
@author     jethro sung (sung.h)
@course     csd1401
@section    a
@project    projcavebound
@date       30/10/24
@brief      this file contains a collection of functions to be called, by declaring utils.h, other source files will be able to call upon the the functions in
            utils.h to be used. furthermore, the definitions of the functions can be assigned via declaring utils.h in other source files, eg. utils.c.
*/
//#pragma once
#ifndef _platform_h_
#define _platform_h_
#include "cprocessing.h"
#include <stdbool.h>
//#include "game.h"
//#include "player.h"
//#include "enemies.h"

// Forward declaration of Player
struct Character;

/*
* base struct for platform, x: top left x-coordinate of the rect, y:top left y coordinate of the rect
* width: horizontal length of rect, height: vertical length of rect
*/
typedef struct Platform {   // structure declaration
	float x;           // x coordinates of platform
	float y;       // y coordinates of platform
	float width; // width of platform
	float height; // height of platform
}Platform;// end the structure with a semicolon

/*Declaration Of func intialise Platform for level 1*/
int initialise_Platform(Platform platforms[]);

/*Declaration Of func intialise Platform for level 2*/
int initialise_Platform_Level2(Platform platforms[]);

/* declaration of func to check if player has collided with platform*/
bool check_Collision_Player_Platform(Character* player, Platform* dplatforms);

/* declaration of func to handle player platform collision*/
void handle_Collision_Player_Platform(Character* player, Platform* platforms);

/* declaration of func to check if enemy has collided with platform*/
void check_Collision_Enemies_Platform(Enemies* enemy, int numofplatforms, Platform platforms[], int platformsToHandleArray[]);

/* declaration of func to handle enemy platform collision*/
void handle_Collision_Enemies_Platform(Enemies* enemy, Platform platforms[], int* noOfPlatforms);

#endif _platform_h_