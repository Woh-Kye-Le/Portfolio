/* Start Header ************************************************************************/
/*!
\file       player.hpp
\project    Ocean Bound
\author     Kye Le, Jethro, Rou Shyan 
\date       April 06, 2025
\brief      This header defines the Player structure and declares all related functions
			for managing player behavior in Ocean Bound. It includes movement mechanics,
			animation handling, rope attachment logic, crouch actions, jump logic, input
			collection for player name, and respawn systems.

			Features:
			- `Player` struct: encapsulates position, physics, animation, and gameplay state.
			- Rope and crouch interaction mechanics.
			- Dynamic animation system with frame tracking.
			- Player input popup and name entry logic.
			- Jumping, movement, and collision behavior.

			Kye Le: Player Movement, Parts of Jump, Rope,
			Jethro : Player Input and Popup, Animation, Respawn and Parts of Jump, Rope, Crouch
			Rou Shyan : Crouch, Jump

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include <string>

extern std::string playerName;

enum AnimationState {
	ANIM_IDLE,
	ANIM_WALK,
	ANIM_JUMP,
	ANIM_CROUCH,
	ANIM_UNCROUCH
};

// Structure for a bounding box (pixel coordinates)
typedef struct {
	int left;
	int top;
	int right;
	int bottom;
} BoundingBox;

typedef struct Player{
	f32 scaleX, scaleY;
	f32 posX, posY;
	f32 rotation;
	f32 velX, velY;
	f32 max_speed;
	f32 acceleration;
	f32 vertical_velocity;
	f32 gravity;
	bool inWater;
	bool isJumping;
	bool isCrouching;
	f32 oxygen_level;
	f32 original_scaleY;
	int currLevel, currRoom; //Tracker for player to keep track of current player level and room on the map
	f32 currhealth, originalhealth;
	int ropeAttachedIndex; //Index of rope player is attached to, initial is -1
	f32 percentage, percentage_increment;
	f32 original_scaleX, original_posx;
	bool onRope; //Check if player is on rope
	f32 ropeCooldown; //Cooldown for rope
	AnimationState animationState; //Current animation state of player
	float animationTimer; //Timer for animation
	int currentFrame; //Current frame of animation
	float standPosY;
	float standScaleY;
	float crouchPosY;
	float crouchScaleY;
	float playerTotalAnimationTime;
	float animationOffset;
	bool onGround;
	AEMtx33 transform;
}Player;

struct Platform; //Forward Declaration of Platforms
struct Level;	 //Forward Declaration of Levels
struct square;	 //Forward Declaration of squares
struct Rope;	 //Forward Declaration of Ropes


void start_Jump_High(Player* player);

void update_Jump_High(Player* player, Level* levels, float dt);

void update_Overall_Jump(Player* player, Level levels[], float dt);

void player_remain_crouched(Player* player);

void player_uncrouch(Player* player);

bool player_Rope_Collision(Player* player, Rope* rope);

void tryAttachToRope(Player* player, const Rope ropes[], int numRopes);

void update_Player_Rope(Player* player, /*const Rope ropes[],*/ float dt);

void updatePlayerRopeCooldown(Player* player, float dt);

void handleCrouchStandAction(Player* player, Platform platforms[], int no_of_platforms);

void update_Movement(Player* player, float dt);

void player_respawn(Player* player, square* oxygenLevel, square* oxygen_level_bg, int* currentLevel);

void displayPlayerInputPopup(s8 FONT, bool* showPopup, AEGfxVertexList* pMesh, AEGfxVertexList* txtBoxMesh, AEGfxVertexList* closeMesh, AEGfxVertexList* confirmMesh, int* currentLevel);

void updatePlayerInput();

void startCrouchAnimation(Player* player);

void startUncrouchAnimation(Player* player);

void update_Player_Animation(Player* player, float dt);

void resetAnimation(Player* player);

void draw_Player(Player* player, AEGfxTexture* textureWalk, AEGfxTexture* textureCrouch);


#endif