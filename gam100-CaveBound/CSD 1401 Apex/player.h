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
#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "cprocessing.h"
#include "stdbool.h"



//#define groundlevel (850.0f)
extern float windowHeight;
extern float windowWidth;

// Forward declarations of structures
struct Platform;
struct Bullet;
struct Enemy;
struct Boss;
struct Points;

typedef struct Character {
    float x;           // X Coordinates of Character
    float y;           // Y Coordinates of Characters
    float width;       // Width of Char
    float height;      // Height of Char
    float speed_x;     // Speed x of char
    float speed_y;     // Speed y of char
    float verticalvelocity; // Vertical jumping speed of char
    float gravity;     // Gravity of character
    bool isJumping;    // Check if character is currently jumping or not
    int jumpsLeft;     // Represents how many jumps the character has left
    float health;      // Remaining health of char
    float maxHealth;   // Max Health of Char
    int direction;     // If player is facing left or right
    bool isHurt;        // Indicates if the player is in the "hurt" state
    float hurtTimer;    // Timer to track how long the player stays in the hurt state
    float hurtDuration; // Duration for which the hurt state persists
    float speedTimer; //timer for speed boost 
    bool isSpeed;
}Character;


//void create_Health_Bar(Character* player, struct Platform * healthBar);

void switch_Character(int* currentCharacterIndex);

void start_Jump_High(Character* player);

void update_Jump_Long(Character* player, float dt);

void start_Jump_Double(Character* player);

void update_Jump_Double(Character* player, float dt);

int player_Location_Set(int* playerstate, Character* player, int* level);


#endif