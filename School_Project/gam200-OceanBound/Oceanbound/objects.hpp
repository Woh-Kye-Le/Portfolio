/* Start Header ************************************************************************/
/*!
\file       objects.hpp
\project    Ocean Bound
\author     Kye Le, Jethro, Rou Shyan , Dillon
\date       April 06, 2025
\brief      This header file provides declarations for managing object interactions
            within the game, including collision checks and gameplay mechanics tied to
            platforms, water, levers, and player-based UI elements.

            It declares core functions used in handling:
            - AABB collision between the player and platforms, water, or levers
            - Oxygen level logic and hotkey-based powerups (shield, speed, oxygen)
            - Drawing and managing UI elements like health and oxygen bars
            - Lever-triggered water interactions across levels
            - Hotkey UI item activation and cooldown tracking

            Key types and variables declared:
            - `square`: UI-related game object (used for oxygen, hotkeys)
            - Global variables for timers and fonts
            - Forward declarations for `Level` and `Room`
            Hotkeys/Cooldown Txt/ AABB Collisions / Base Water/ Oxygen handling -> Kye Le
            Water/Water Lever related -> Jethro
            Platform - player collision ->Rou Shyan
            Obstacle - player collision ->Dillon 

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef OBJECTS_HPP
#define OBJECTS_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "player.h"
#include "platforms.h"
#include "water.h"
#include "levers.h"
//constexpr auto groundlevel = (-450.0f);

typedef struct square {
	f32 scaleX, scaleY;
	f32 posX, posY;
	f32 rotation;
	f32 oxygen_level;
	f32 percentage, percentage_increment;
	f32 original_scaleX, original_posX;
	s32 hotkey_ID;
}square;


extern square oxygen_level;
extern double* time;
extern double lastPressTime;
extern double lastSpeedTime;
extern double lastShieldTime;
extern double lastOxygenTime;

extern s8 font;

struct Level; //Forward Declaration of Level, definition is retrieved from level include in .cpp.
struct Room;


bool check_Collision_Player_Objects(Player* player, square* objects);

bool check_Collision_Player_Platform(Player* player, Platform* platforms);

bool check_Collision_Player_Water(Player* player, Water* WATER);

bool check_Collision_Player_Lever(Player* player, Lever* lever);

void handle_Collision_Player_Platform(Player* player, square* oxygenLevel, square* oxygen_level_bg, square* hotkey_item, Platform* platforms);

bool check_Collision_Player_Platforn_While_Crouching(Player* player, Platform* platforms);

void handle_Collision_Player_Water_Lever(Lever* waterLever, Level* levels, float* leverCooldown);

void update_Collision_Player_Water_Lever(Player* player, Level levels[], Lever levers[], int numLevers, int* currentLevel, float* leverCooldown);

void handle_Collision_Player_Water(Player* player, Room* room, Water* water, float dt, float water_top, float water_bottom, int* activeHotkey);

void update_Collision_Player_Water(Player* player, Level* levels, int* activeHotkey, float dt);

void display_Hotkey_Items(square* hotkey_item, Player* player, int* activeHotkey, AEGfxVertexList* color, AEGfxVertexList* color1, AEGfxVertexList* color2, AEGfxTexture* shield, AEGfxTexture* speed, AEGfxTexture* oxy, s8 cooldownFont);

void handle_Oxygen_Level(square* oxygenLevel, Player* player, Room* room, int* activeHotkey, float* dt);

void align_Player_object(Player* player, square* oxygenLevel, square* oxygen_level_bg, square* hotkey_item);

#endif 