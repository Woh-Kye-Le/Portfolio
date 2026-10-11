/* Start Header ************************************************************************/
/*!
\file       objects.cpp
\project    Ocean Bound
\author     Kye Le, Jethro, Rou Shyan , Dillon
\date       April, 06, 2025
\brief      This file manages collision detection and interaction logic between the player and various in-game elements such as platforms, water, and levers. 
			It also handles gameplay mechanics like oxygen depletion, hotkey-based abilities (e.g., shield, speed, oxygen boost), and their cooldowns,
			integrating visual and gameplay feedback for a dynamic platforming experience.
			Hotkeys/Cooldown Txt/ AABB Collisions / Base Water/ Oxygen handling -> Kye Le
			Water/Water Lever related -> Jethro 
			Platform - player collision ->Rou Shyan 
			obstacle - player collision -> dillon 

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "objects.hpp"
#include <math.h>
#include "GameStateManager.h"
#include "pch.h"
#include "Room.hpp"
#include "utils.hpp"
#include "player.h"
#include "specialAbilities.h"

extern s8 pFont;

square water = { 500.0f, 0.0f, 112.5f + water.scaleX / 2, groundLevel + 240.0f };
square water_lever = { 25.0, 50.0, -500.0, -425.0 };

static bool showCooldownText = false;
static int cooldownTextHotkeyID = 0;

double* time;
double lastPressTime = 0.0; // for key Q
double lastShieldTime = 0.0;
double lastSpeedTime = 0.0;
double lastOxygenTime = 0.0;
double lastTextTime = 0.0;

const double SHIELD_COOLDOWN = 23.0; // this timing is the cooldown timing of 15secs and the ability timing of 8 secs
const double SPEED_COOLDOWN = 18.0;	 // this timing is the cooldown timing of 15secs and the ability timing of 3 secs
const double OXYGEN_COOLDOWN = 18.0; // this timing is the cooldown timing of 15secs and the ability timing of 3 secs

const double TEXT_COOLDOWN = 5.0;

s8 font;

// this function checks collision bewteen player and a square object 
// function is resuable by changing the input for the 2nd argument ONLY IF THE OBJECT IS TYPE STRUCT SQUARE 
bool check_Collision_Player_Objects(Player* player, square* objects) {
	f32 player_left = player->posX - player->scaleX / 2;
	f32 player_top = player->posY + player->scaleY / 2;
	f32 player_right = player->posX + player->scaleX / 2;
	f32 player_bottom = player->posY - player->scaleY / 2;

	f32 objects_left = objects->posX - objects->scaleX / 2;
	f32 objects_top = objects->posY + objects->scaleY / 2;
	f32 objects_right = objects->posX + objects->scaleX / 2;
	f32 objects_bottom = objects->posY - objects->scaleY / 2;

	return (player_left < objects_right && player_right > objects_left && player_top > objects_bottom && player_bottom < objects_top);
}


//Checks collision between Player Platform
bool check_Collision_Player_Platform(Player* player, Platform* platforms) {
	float collisionMargin = 5.f;
	f32 player_left = player->posX - player->scaleX / 2;
	f32 player_top = player->posY + player->scaleY / 2;
	f32 player_right = player->posX + player->scaleX / 2;
	f32 player_bottom = player->posY - player->scaleY / 2;

	f32 platforms_left = platforms->posX - platforms->scaleX / 2;
	f32 platforms_top = platforms->posY + platforms->scaleY / 2 + collisionMargin;
	f32 platforms_right = platforms->posX + platforms->scaleX / 2;
	f32 platforms_bottom = platforms->posY - platforms->scaleY / 2;

	return (player_left < platforms_right && player_right > platforms_left && player_top > platforms_bottom && player_bottom < platforms_top && !(platforms->phaseOk));
}

// checks for any collision with platforms if the player uncrouches while crouching 
bool check_Collision_Player_Platforn_While_Crouching(Player* player, Platform* platforms) {
	// Calculate the player's top if they were standing.
	float player_stand_top = player->posY + 25.0f + player->original_scaleY / 2;

	// Calculate the bottom and top of the platform.
	float platform_bottom = platforms->posY - platforms->scaleY / 2;
	float platform_top = platforms->posY + platforms->scaleY / 2;

	// Check if the platform is directly above the player and within vertical collision range.
	bool is_directly_above = (player->posX + player->scaleX / 2 > platforms->posX - platforms->scaleX / 2) &&
							 (player->posX - player->scaleX / 2 < platforms->posX + platforms->scaleX / 2);
	bool is_within_vertical_collision_range = (player_stand_top > platform_bottom) && (player_stand_top < platform_top);

	return is_directly_above && is_within_vertical_collision_range;
}

//Checks collision between Player Water
bool check_Collision_Player_Water(Player* player, Water* WATER) {
	f32 player_left = player->posX - player->scaleX / 2;
	f32 player_top = player->posY + player->scaleY / 2;
	f32 player_right = player->posX + player->scaleX / 2;
	f32 player_bottom = player->posY - player->scaleY / 2;

	f32 objects_left = WATER->posX - WATER->scaleX / 2;
	f32 objects_top = WATER->posY + WATER->scaleY / 2;
	f32 objects_right = WATER->posX + WATER->scaleX / 2;
	f32 objects_bottom = WATER->posY - WATER->scaleY / 2;

	return (player_left < objects_right && player_right > objects_left && player_top > objects_bottom && player_bottom < objects_top);
}

//Checks collision between Player Lever
bool check_Collision_Player_Lever(Player* player, Lever* lever) {
	f32 player_left = player->posX - player->scaleX / 2;
	f32 player_top = player->posY + player->scaleY / 2;
	f32 player_right = player->posX + player->scaleX / 2;
	f32 player_bottom = player->posY - player->scaleY / 2;

	f32 objects_left = lever->posX - lever->scaleX / 2;
	f32 objects_top = lever->posY + lever->scaleY / 2;
	f32 objects_right = lever->posX + lever->scaleX / 2;
	f32 objects_bottom = lever->posY - lever->scaleY / 2;
	return (player_left < objects_right && player_right > objects_left && player_top > objects_bottom && player_bottom < objects_top);
}

// this function includes oxygen_level 
void handle_Collision_Player_Platform(Player* player, square* oxygenLevel, square* oxygen_level_bg, square* hotkey_item, Platform* platforms) {
	// Calculate overlap along the X-axis
	float overlapX = fminf((player->posX + player->scaleX / 2) - (platforms->posX - platforms->scaleX / 2),
		(platforms->posX + platforms->scaleX / 2) - (player->posX - player->scaleX / 2));

	// Calculate overlap along the Y-axis
	float overlapY = fminf((player->posY + player->scaleY / 2) - (platforms->posY - platforms->scaleY / 2),
		(platforms->posY + platforms->scaleY / 2) - (player->posY - player->scaleY / 2));

	if (overlapX < overlapY) {
		// Horizontal collision
		if (player->posX < platforms->posX) {
			player->posX -= overlapX;  // Collision from the left of the platform
			oxygenLevel->posX -= overlapX;
			oxygen_level_bg->posX -= overlapX;
			hotkey_item->posX -= overlapX;
			player->velX = 0;
		}
		else {
			player->posX += overlapX;  // Collision from the right of the platform
			oxygenLevel->posX += overlapX;
			oxygen_level_bg->posX += overlapX;
			hotkey_item->posX += overlapX;
			player->velX = 0;
		}
	}
	else {
		// Vertical collision
		if (player->posY < platforms->posY) { // Collision from below the platform
			player->posY -= overlapY;
			oxygenLevel->posY -= overlapY;
			oxygen_level_bg->posY -= overlapY;
			hotkey_item->posY -= overlapY;
			player->vertical_velocity = 0;  // Stop downward movement
		}
		else { // Collision from above the platform
			player->posY += overlapY;
			oxygenLevel->posY += overlapY;
			oxygen_level_bg->posY += overlapY;
			hotkey_item->posY += overlapY;
			player->vertical_velocity = 0;  // Stop upward movement
			player->onGround = true;
			player->isJumping = false; // End jumping state
		}
	}

}

/*
	This handles collision for the water lever.
	This takes in the player, the current water lever that has collided with player, and the current level.
	As long as e is triggered, we will flip the boolean for the water lever.
	If the boolean resulting from this is true, we cycle through each of the array of affected rooms within the array of affected levers,
	and if a value there is -1, it means that there are no rooms that are being affected, in which we then break out of the room loop and move on to
	the next affected level. NOTE: -1 is needed because it is hard to differentiate between 0 and null for int arrays, meaning we need the -1 to check for empty
	arrays where a level is not affected.
	With this, if a room has been affected, the hasWater and hasTidal bools in the rooms are flipped to true, allowing for the processing of the tidal function to accomodate said room

	Conversely, if the triggered bool is false but E has been pressed, we change the tidal and water to false, to disable the tidal and water for the room.
*/
void handle_Collision_Player_Water_Lever(Lever* waterLever, Level* levels, float *leverCooldown) {
	if (AEInputCheckCurr(AEVK_E)&& *leverCooldown== 0.f)
	{
		for (int levelLevers = 0; levelLevers < 2; levelLevers++)
		{
			for (int i = 0; i < levels->roomCount; i++)
			{
				if (waterLever->levelAffected[levelLevers][i] != -1)
				{
					Room* r = &levels[levelLevers].rooms[waterLever->levelAffected[levelLevers][i]];
					// Check up to 3 water blocks
					for (int w = 0; w < r->waterCount; w++)
					{
						Water* wb = &r->water[w];
						if (wb->waterMustBeDrained)
						{
							return;
						}
					}
				}
			}
		}
		waterLever->isTriggered = !(waterLever->isTriggered);
		*leverCooldown = 1.f;
		if (waterLever->isTriggered)
		{
			for (int leverLevers = 0; leverLevers < 2; leverLevers++)
			{
				for (int i = 0; i < levels->roomCount; i++)
				{
					if (waterLever->levelAffected[leverLevers][i] != -1 && waterLever->leverType == LEVER_TIDAL)
					{
						levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].hasWater = true;
						levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].hasTidal = true;
					}
					else if (waterLever->levelAffected[leverLevers][i] != -1 && waterLever->leverType == LEVER_WATER_FLOW)
					{
						for (int affectedWater = 0; affectedWater < MAX_AFFECTED_WATER; affectedWater++)
						{
							for (int s = 0; s < MAX_AFFECTED_WATER; s++)
							{
								if (waterLever->waterAffected[s] == -1)
								{
									break;
								}
								if (affectedWater == waterLever->waterAffected[s]) {
									Water* currWater = &levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].water[affectedWater];
									if (currWater->state == WATER_RISING)
									{
										currWater->state = WATER_STATIC;
										currWater->prevState = WATER_RISING;
										currWater->permaFlow = false;
									}
									else if (currWater->state == WATER_STATIC && currWater->prevState == WATER_RISING)
									{
										currWater->state = WATER_FALLING;
										currWater->prevState = WATER_STATIC;
										currWater->permaFlow = true;
									}
									else if (currWater->state == WATER_FALLING)
									{
										currWater->state = WATER_STATIC;
										currWater->prevState = WATER_FALLING;
										currWater->permaFlow = false;
									}
									else if (currWater->state == WATER_STATIC && currWater->prevState == WATER_FALLING)
									{
										currWater->state = WATER_RISING;
										currWater->prevState = WATER_STATIC;
										currWater->permaFlow = true;
										currWater->maxTidalReached = false;
									}
									break;
								}
							}
						}
						levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].hasWater = true;
					}
					else
					{
						break;
					}
				}
			}
		}
		else
		{
			for (int leverLevers = 0; leverLevers < 2; leverLevers++)
			{
				for (int i = 0; i < levels->roomCount; i++)
				{
					if (waterLever->levelAffected[leverLevers][i] != -1 && waterLever->leverType == LEVER_TIDAL)
					{ 
						for (int affectedWater = 0; affectedWater < MAX_AFFECTED_WATER; affectedWater++)
						{
							if (levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].water[affectedWater].scaleY > 0.0f)
							{
								levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].water[affectedWater].state = WATER_FALLING;
								levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].water[affectedWater].waterMustBeDrained = true;
							}
						}
					}
					else if(waterLever->levelAffected[leverLevers][i] != -1 && waterLever->leverType == LEVER_WATER_FLOW)
					{
						for (int affectedWater = 0; affectedWater < 3; affectedWater++)
						{
							for (int s = 0; s < MAX_AFFECTED_WATER; s++)
							{
								if (waterLever->waterAffected[s] == -1)
								{
									break;
								}
								if (affectedWater == waterLever->waterAffected[s]) {

									Water* currWater = &levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].water[affectedWater];
									if (currWater->state == WATER_RISING)
									{
										currWater->state = WATER_STATIC;
										currWater->prevState = WATER_RISING;
										currWater->permaFlow = false;
									}
									else if (currWater->state == WATER_STATIC && currWater->prevState == WATER_RISING)
									{
										currWater->state = WATER_FALLING;
										currWater->prevState = WATER_STATIC;
										currWater->permaFlow = true;
									}
									else if (currWater->state == WATER_FALLING)
									{
										currWater->state = WATER_STATIC;
										currWater->prevState = WATER_FALLING;
										currWater->permaFlow = false;
									}
									else if (currWater->state == WATER_STATIC && currWater->prevState == WATER_FALLING)
									{
										currWater->state = WATER_RISING;
										currWater->prevState = WATER_STATIC;
										currWater->permaFlow = true;
										currWater->maxTidalReached = false;
									}
									break;
								}
							}
						}
						levels[leverLevers].rooms[waterLever->levelAffected[leverLevers][i]].hasWater = true;
					}
					else
					{
						break;
					}
				}
			}
		}
	}
}


/*
	Check each lever if lever is on the currently displayed levels, and if collision for them has occured.
	Logic is similar for the for loops to tidal.
	Next, we init a bool called leverCollision with default value of false
	THis is used to store the bool return from checking if the lever has collided with the player. If the lever
	is among the currently displayed levels, and the bool returns true, we then handle the collision between the player and lever by
	passing in the player, the lever that has collided with the player, and lastly the current level.
	NOTE:	Current level is being passed instead of i. This is because in the case where it is the 2nd level that collided,
			we need to ensure that if the lever affects the 1st level, we are able to access the level array of the 1st lever
			hence preventing UDB.
*/
void update_Collision_Player_Water_Lever(Player* player, Level levels[], Lever levers[], int numLevers, int *currentLevel, float* leverCooldown) {
    for (int i = *currentLevel; i < *currentLevel + 2; i++)
	{  // Check current and next level
        for (int leverCycle = 0; leverCycle < numLevers; leverCycle++)
		{
            bool leverCollision = false;
            if (levers[leverCycle].locationOfLever == i) 
			{  // Check if lever belongs to the current level
                leverCollision = check_Collision_Player_Lever(player, &levers[leverCycle]);
                
                if (leverCollision) 
				{
                    /*
                     * Collision between player and water lever
                     */
                    handle_Collision_Player_Water_Lever( &levers[leverCycle], &levels[(*currentLevel)], leverCooldown);
                }
            }
        }
    }
}
/*
	Basically, this is the collision for water, keep in mind the prev values of water_top water_bottom, are now instead retrieved from the water array within the array of
	Rooms within the array of Levels, rather then a generic water initialization
*/
void handle_Collision_Player_Water(Player* player, Room*room, Water* waterMain, float dt, float water_top, float water_bottom, int* activeHotkey) {
	float buffer = 5.0f;
	float overallWaterBottom = water_bottom;
	if (waterMain->linkedID != -1)
	{
		if ((room->water + waterMain->linkedID)->scaleY>0.f)
		{
			float linkedWaterBottom = (room->water + waterMain->linkedID)->posY - (room->water + waterMain->linkedID)->scaleY / 2.f;
			if (linkedWaterBottom < overallWaterBottom)
			{
				overallWaterBottom = linkedWaterBottom;
			}
		}
	}

	if (waterMain->type == WATER_LAVA && *activeHotkey != 1)
	{
		player->currhealth -= 1000.f * dt;
	}
	else if (waterMain->type == WATER_POISON && *activeHotkey != 1)
	{
		player->currhealth -= 150.f * dt;
	}

	//float only when water level is more than equal to the height of player
	if (water_top > water_bottom + player->scaleY/2 || waterMain->LinkedAbove&& water_top > buffer) {
		if ((player->posY < water_top + buffer) && waterMain->LinkedAbove || player->posY < water_top+player->scaleY/2.f) {
			player->inWater = true;
			player->onGround = true;
		}
		else
			player->inWater = false;

	// only apply the water logic if player is fully within the water horizontally
		f32 side_length_water = waterMain->scaleX / 2.0f;
		if ((player->posX >= waterMain->posX - side_length_water) && (player->posX <= waterMain->posX + side_length_water)) {
			if (!player->isJumping) {
				if (!AEInputCheckCurr(AEVK_S)) {
					if (player->posY < water_top) {
						// Smoothly float to the surface
						player->posY += 150.0f * dt;
						if (AEInputCheckCurr(AEVK_W)) {
							player->posY += player->max_speed * dt;
						}
					}
					else {
					}
				}
				else {
					// Diving logic: Move downward when S is pressed
					player->posY -= player->max_speed * dt;
					if (player->posY < overallWaterBottom + player->scaleY / 2) {
						player->posY = overallWaterBottom + player->scaleY / 2; // Clamp to bottom
					}
				}
			}
		}
	}
}

void update_Collision_Player_Water(Player* player, Level *levels, int* activeHotkey, float dt) {
	bool waterCollisionCheck = false;
	bool playerAboveWaterTop = false;
	/*
	*	Here, we basically check the water position in the room, within the level the player is currently in if collision has occured.
	*/
	float water_TOP = groundLevel + player->scaleY / 2;
	for (int i = 0; i < levels[player->currLevel].rooms[player->currRoom].waterCount; i++)//loop thru all water
	{
		if (check_Collision_Player_Water(player, &levels[player->currLevel].rooms[player->currRoom].water[i]) && levels[player->currLevel].rooms[player->currRoom].water[i].scaleY > 0.0f) {
			handle_Collision_Player_Water(player, &levels[player->currLevel].rooms[player->currRoom], &levels[player->currLevel].rooms[player->currRoom].water[i], dt,
				levels[player->currLevel].rooms[player->currRoom].water[i].posY + levels[player->currLevel].rooms[player->currRoom].water[i].scaleY / 2,// Water Top 
				levels[player->currLevel].rooms[player->currRoom].water[i].posY - levels[player->currLevel].rooms[player->currRoom].water[i].scaleY / 2, activeHotkey); // Water Bottom
			float currentWaterTop = levels[player->currLevel].rooms[player->currRoom].water[i].posY + levels[player->currLevel].rooms[player->currRoom].water[i].scaleY / 2.0f;

			// Update water_TOP.
			// If multiple water blocks collide, you might choose the highest water top.
			if (!waterCollisionCheck)
			{
				water_TOP = currentWaterTop;
			}
			else
			{
				if (currentWaterTop > water_TOP && levels[player->currLevel].rooms[player->currRoom].water[i].scaleY > 5.f)
				{
					water_TOP = currentWaterTop;
				}
			}
			if (levels[player->currLevel].rooms[player->currRoom].water[i].LinkedAbove && levels[player->currLevel].rooms[player->currRoom].water[i].scaleY > 0.f)
			{
				// Here we assume the player is "above" if their position is greater than water_TOP.
				playerAboveWaterTop = (player->posY > water_TOP);
			}
			else if (!levels[player->currLevel].rooms[player->currRoom].water[i].LinkedAbove)
			{
				// Otherwise, include an offset (half the player's height) to decide.
				if (water_TOP < levels[player->currLevel].rooms[player->currRoom].water[i].posY - levels[player->currLevel].rooms[player->currRoom].water[i].scaleY / 2.f + player->scaleY / 2.f)
				{
					playerAboveWaterTop = false;
				}
				else
				{
					playerAboveWaterTop = ((player->posY) >= water_TOP);
				}
			}
			waterCollisionCheck = true;
			//break;
		}
	}
	if (!waterCollisionCheck) { //reseting all stats
		player->inWater = false;
	}
	else if (waterCollisionCheck && playerAboveWaterTop && !player->isJumping)
	{
		player->posY = water_TOP;
	}
}


void display_Hotkey_Items(square* hotkey_item, Player* player, int* activeHotkey, AEGfxVertexList* color, AEGfxVertexList* color1, AEGfxVertexList* color2, AEGfxTexture* shield, AEGfxTexture* speed, AEGfxTexture* oxy, s8 cooldownFont) {
	double current_time = AEGetTime(time);
	if (AEInputCheckTriggered(AEVK_Q) && !player->isCrouching) {
		hotkey_item->hotkey_ID = (hotkey_item->hotkey_ID % 3) + 1; // Cycle through 1-3
		lastPressTime = current_time;
	}
	if (current_time - lastPressTime <= 2.0) {
		switch (hotkey_item->hotkey_ID) {
			//shield
		case 1: mesh_Draw_With_Texture(hotkey_item->rotation, hotkey_item->scaleX, hotkey_item->scaleY, hotkey_item->posX, hotkey_item->posY, color, shield);
			break;
			//speed
		case 2: mesh_Draw_With_Texture(hotkey_item->rotation, hotkey_item->scaleX, hotkey_item->scaleY, hotkey_item->posX, hotkey_item->posY, color1, speed);
			break;
			//increase oxygen level
		case 3: mesh_Draw_With_Texture(hotkey_item->rotation, hotkey_item->scaleX, hotkey_item->scaleY, hotkey_item->posX, hotkey_item->posY, color2, oxy);
			break;
		default:
			break;
		}
	}
	else {
		hotkey_item->hotkey_ID = 0;
	}
	if (AEInputCheckTriggered(AEVK_RETURN)) {
		switch (hotkey_item->hotkey_ID) {
			//shield
		case 1:
			if (current_time - lastShieldTime >= SHIELD_COOLDOWN) {
				lastShieldTime = current_time;
				*activeHotkey = 1;
				showCooldownText = false;
			}
			else {
				if (!showCooldownText) {
					showCooldownText = true;
					lastTextTime = current_time; 
					cooldownTextHotkeyID = 1;
				}
			}
			break;

			//speed (3 secs)
		case 2:
			if (!player->inWater && current_time - lastSpeedTime >= SPEED_COOLDOWN) {
				lastSpeedTime = current_time;
				*activeHotkey = 2;
				showCooldownText = false;
			}
			else {
				if (!showCooldownText) {
					showCooldownText = true;
					lastTextTime = current_time;
					cooldownTextHotkeyID = 2;
				}
			}
			break;

			//increase oxygen level (3 secs)
		case 3:
			if (player->inWater && current_time - lastOxygenTime >= OXYGEN_COOLDOWN) {
				lastOxygenTime = current_time;
				*activeHotkey = 3;
			}
			else {
				if (!showCooldownText) {
					showCooldownText = true;
					lastTextTime = current_time;
					cooldownTextHotkeyID = 3;
				}
			}
			break;

		default:
			*activeHotkey = 0;
			showCooldownText = false;
			break;
		}
	}

	// Display cooldown message if necessary
	if (showCooldownText && (current_time - lastTextTime <= TEXT_COOLDOWN)) {
		char cooldownMsg[64];

		double timeRemaining = 0.0;
		switch (cooldownTextHotkeyID) {
		case 1:
			timeRemaining = SHIELD_COOLDOWN - (current_time - lastShieldTime);
			break;
		case 2:
			timeRemaining = SPEED_COOLDOWN - (current_time - lastSpeedTime);
			break;
		case 3:
			timeRemaining = OXYGEN_COOLDOWN - (current_time - lastOxygenTime);
			break;
		}

		if (timeRemaining > 0)
		{
			snprintf(cooldownMsg, sizeof(cooldownMsg), "cooldown %.1fs", timeRemaining);
		}
		AEGfxPrint(cooldownFont, cooldownMsg, (player->posX - 60.0f) / 800.0f, (player->posY + 150.0f) / 450.0f, 0.3f, 1.f, 0.f, 0.f, 1.f);
	}

	// Auto-hide the cooldown message after 5 seconds
	if (showCooldownText && (current_time - lastTextTime > TEXT_COOLDOWN)) {
		showCooldownText = false;
	}
}



void handle_Oxygen_Level(square* oxygenLevel, Player* player, Room* room, int* activeHotkey, float* dt) {
	// checks firstly, if the player collides with water and also if the player is fully submerged in water 
	if (player->inWater && player->posY < get_Highest_Water_Top_Above_Player(player, room) - player->scaleY / 2) {
		oxygenLevel->scaleX -= (10.f * *dt);
		oxygenLevel->percentage_increment -= (5.f * *dt);
	}
	else {
		oxygenLevel->scaleX += (8.f * *dt);
		oxygenLevel->percentage_increment += (4.f * *dt);
	}

	if (ability_three(activeHotkey) && player->inWater) {
		oxygenLevel->scaleX += (10.f * *dt);
		oxygenLevel->percentage_increment += (5.f * *dt);
	}

	if (oxygenLevel->scaleX <= 0) {
		oxygenLevel->scaleX = 0;
		oxygenLevel->percentage_increment = -(oxygenLevel->original_scaleX / 2.f);
	}
	if (oxygenLevel->scaleX >= oxygenLevel->original_scaleX) {
		oxygenLevel->scaleX = oxygenLevel->original_scaleX;
		oxygenLevel->percentage_increment = 0.f;
	}
	
	// damages player if the oxygen level is depleted 
	if (oxygenLevel->scaleX <= 0.f)
	{
		player->currhealth -= 50.f * *dt;
	}
}

void align_Player_object(Player* player, square* oxygenLevel, square* oxygen_level_bg, square* hotkey_item) {
	
	//ensures oxygen level and bg is always above player	
	oxygen_level_bg->posX = player->posX;
	oxygen_level_bg->posY = player->posY + player->scaleY / 2 + 20.0f;
	oxygenLevel->posX = oxygen_level_bg->posX + oxygenLevel->percentage_increment;
	oxygenLevel->posY = player->posY + player->scaleY / 2 + 20.0f;

	if (player->isCrouching) {
		oxygenLevel->posY += 8.f;
		oxygen_level_bg->posY += 8.f;
	}

	// ensures hotkey_item is always above oxygen_level
	hotkey_item->posX = player->posX;
	hotkey_item->posY = oxygenLevel->posY + 50.0f;
}
