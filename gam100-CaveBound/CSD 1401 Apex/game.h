/*
@file       this is file game.h
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    player_state_game
@date       10/2/24
@brief      this file contains a collection of functions to be called, by declaring game.h, other source files will be able to call upon the the functions in
			game.h to be used. Furthermore, the definitions of the functions can be assigned via declaring game.h in other source files, eg. game.c.
*/
//#pragma once
#ifndef _GAME_H_
#define _GAME_H_
#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"
#include "player.h"
#include "points.h"
#include "enemies.h"
#include "boss.h"
#include "platform.h"
#include "bullet.h"
#include "leaderBoard.h"
#include "gameover.h"
#include "mainmenu.h"
#include "credits.h"

#define groundlevel (850.0f)

// game state enumeration
typedef enum {
	GAME_STATE_MAIN_MENU,
	GAME_STATE_OPTIONS,
	GAME_STATE_CREDITS,
	GAME_STATE_PLAY
} GameState;


// declare the global variable for the current game state
extern GameState currentGameState;

/*
	The function below provides the declaration of a function Game_Init. function Game_Init takes in and outputs void. It initializes the game.
*/
void Game_Init(void);
/*
	The function below provides the declaration of a function Game_Update. function Game_Update takes in and outputs void. It updates the game state.
*/
void Game_Update(void);
/*
	The function below provides the declaration of a function Game_Exit. function Game_Exit takes in and outputs void. It exits the game state of the game.
*/
void Game_Exit(void);

void Options_Init(void);
void Options_Update(void);
void Options_Exit(void);

void checkLevel(int currentLevel, int* numOfBoss, int* numberOfRemainingEnemies, int* numberOfRangedEnemies, int* numberOfMeleeEnemies, int* numberOfPlatforms, Boss arrayOfBoss[], Platform arrayOfPlatforms[], Enemies arrayOfRangedEnemies[], Enemies arrayOfMeleeEnemies[], Character* player);

void apply_Gravity(int const* numberOfMeleeEnemies, int const* numberOfRangedEnemies, Character* player, Enemies arrayOfMeleeEnemies[], Enemies arrayOfRangedEnemies[], float dt);

void check_Boundary(int* numberOfMeleeEnemies, int* numberOfRangedEnemies, Character* player, Enemies arrayOfMeleeEnemies[], Enemies arrayOfRangedEnemies[]);

void SetGameState(GameState state);
#endif