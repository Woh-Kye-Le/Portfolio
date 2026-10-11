/* Start Header ************************************************************************/
/*!
\file       room.hpp
\project    Ocean Bound
\author     Kye Le, Jethro, Reginald , Dillon  
\date       April 06, 2025
\brief      This header file defines the core structures and functions for handling the game’s
			environmental elements including levels, rooms, doors, traps, trap switches, and
			dynamic water systems.

			Features:
			- `Room`, `Level`, and `Door` struct definitions for layout and navigation.
			- Tidal system handling for rising and falling water.
			- Collision detection and handling between the player and environment objects
			  (doors, traps, trap switches).
			- Physics-based interactions such as gravity and room boundary enforcement.
			- Support for water jump mechanics and trap-switch triggered events.

			Water/Tidal Mechanics/Door/Traps/Switch/Room/Player-Enviroment/Gravity -> Jethro
			AABB Collision/Gravity -> Kye Le
			Initialization Rooms -> Reginald
			Multi Water block -> dillon

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef ROOM_HPP
#define ROOM_HPP

#include <crtdbg.h> // To check for memory leaks
#include "water.h"
#include "traps.h"
#include "trapSwitch.h"
#include "AEEngine.h"

typedef struct Door {
	float scaleX, scaleY; // Size of the door
	float posX, posY; // Position within the rooms
	int destinationLevel; // The level the door leads to
	int destinationDoor; // The door the door leads to within that level
	bool triggerNextLevel; // Check if the door leads to the next level, eg. level 2 to 3. (KEEP IN MIND, this is only when transitioning to the next display of levels, and not level 1 to level 2
} Door;

typedef struct Room {
	f32 scaleX, scaleY;
	f32 posX, posY;
	bool hasWater, hasTidal; // Bool checks for water and tidal
	//f32 maxTidalHeight; //If there is tidal, the value of this must be initialized and more than 0.
	int waterCount;
	Water water[5];// array of water blocks
	//Water water; // struct water is here as every room has a water struct assigned to it, if there is no water, the water struct is left empty
	//bool maxTidalReached; // Checking if the water levels in the room has reached the maximum tidal level
}Room;

typedef struct Level {
	int roomCount=0; // Num of rooms in the level
	int doorCount=0; // Num of Doors in Level
	int trapCount=0;
	int trapSwitchCount=0;
	Room rooms[5] = {}; // Array of up to 5 rooms
	Door doors[20] = {}; // Arr of Up to 20 doors
	Trap traps[10] = {}; // At most 10 traps per level 
	TrapSwitch trapSwitches[10] = {};
}Level;

struct Player; // Forward Declaration of Player, this is to prevent circular dependency, while enabling the use of Player in the functions. The definitions are used in Room.cpp


// Initialization of Levels
void initializeLevels(Level levels[]);

// Handling Of Tidal in a room
void handle_Tidal(Room* room, float dt);

// updating of tidals in all rooms
void update_tidal(Level levels[], int currentLevel, float dt);

// collision check between player and door 
bool check_Collision_Player_Door(Player* player, Door* door);

// handling player - door interaction 
void handle_Collision_Player_Door(Player* player, Door* doorToHandle, Level* levels, int* currLevel);

// updating all player - door interaction in all rooms 
void update_Player_Door(Player* player, Level levels[], int *currentLevel);

void apply_Player_Room_Boundary(Player* player, Room* room);

void apply_gravity_Per_Room(Player* player, Room* room, float dt);

// collision check between player and trap 
bool check_Collision_Player_Trap(Player* player, Trap* trap);

// handling player - trap collision 
void handle_Collision_Player_Trap(Player* player, Trap* trap, int* activeHotkey);

// updating player - trap collision for all rooms
void update_Collision_Player_Trap(Player* player, Level levels[], int* currentLevel, int* activeHotkey, float dt);

// collision check between player and trap switch 
bool check_Collision_Player_Trap_Switch(Player* player, TrapSwitch* trapSwitch);

// handling player - trap switch collision 
void handle_Collision_Player_Trap_Switch( TrapSwitch* trapSwitch, Level* level, float dt);

// update player - trap switch collision for all rooms
void update_Collision_Player_Trap_Switch(Player* player, Level levels[], int *currentLevel, float dt);

bool check_Collision_Player_Water_For_Jump(Player* player, Room* room);

float get_Highest_Water_Top_Above_Player(const Player* player, const Room* room);

#endif // !ROOM_HPP