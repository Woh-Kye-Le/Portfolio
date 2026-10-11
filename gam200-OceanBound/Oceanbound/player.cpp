/* Start Header ************************************************************************/
/*!
\file       player.cpp
\project    Ocean Bound
\author     Kye Le, Jethro, Rou Shyan 
\date       April, 06, 2025
\brief      This file implements core player functionality in the game, handling user input, movement, jumping, crouching, rope mechanics, and animation states. 
			It also manages the player name entry UI, movement physics, and sprite rendering, including transitions between idle, walking, crouching, and rope interactions. 
			The file ensures smooth gameplay feedback and precise control over the player's interactions within different environments and level elements.
			Kye Le: Player Movement, Parts of Jump, Rope, 
			Jethro : Player Input and Popup, Animation, Respawn and Parts of Jump, Rope, Crouch
			Rou Shyan : Crouch, Jump 

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "player.h"
#include "pch.h"
#include "Room.hpp"
#include <math.h>
#include "GameStateManager.h"
#include "objects.hpp"
#include "ropes.h"
#include <string>
#include "utils.hpp"


std::string playerName = "";

void updatePlayerInput() {
	if (AEInputCheckTriggered(AEVK_BACK) && !playerName.empty())
	{
		playerName.pop_back();
	}
	if (playerName.length() > 12)
	{
		return;
	}
	if (AEInputCheckTriggered(AEVK_A)) { playerName.push_back('A'); }
	if (AEInputCheckTriggered(AEVK_B)) { playerName.push_back('B'); }
	if (AEInputCheckTriggered(AEVK_C)) { playerName.push_back('C'); }
	if (AEInputCheckTriggered(AEVK_D)) { playerName.push_back('D'); }
	if (AEInputCheckTriggered(AEVK_E)) { playerName.push_back('E'); }

	if (AEInputCheckTriggered(AEVK_F)) { playerName.push_back('F'); }
	if (AEInputCheckTriggered(AEVK_G)) { playerName.push_back('G'); }
	if (AEInputCheckTriggered(AEVK_H)) { playerName.push_back('H'); }
	if (AEInputCheckTriggered(AEVK_I)) { playerName.push_back('I'); }
	if (AEInputCheckTriggered(AEVK_J)) { playerName.push_back('J'); }

	if (AEInputCheckTriggered(AEVK_K)) { playerName.push_back('K'); }
	if (AEInputCheckTriggered(AEVK_L)) { playerName.push_back('L'); }
	if (AEInputCheckTriggered(AEVK_M)) { playerName.push_back('M'); }
	if (AEInputCheckTriggered(AEVK_N)) { playerName.push_back('N'); }
	if (AEInputCheckTriggered(AEVK_O)) { playerName.push_back('O'); }

	if (AEInputCheckTriggered(AEVK_P)) { playerName.push_back('P'); }
	if (AEInputCheckTriggered(AEVK_Q)) { playerName.push_back('Q'); }
	if (AEInputCheckTriggered(AEVK_R)) { playerName.push_back('R'); }
	if (AEInputCheckTriggered(AEVK_S)) { playerName.push_back('S'); }
	if (AEInputCheckTriggered(AEVK_T)) { playerName.push_back('T'); }

	if (AEInputCheckTriggered(AEVK_U)) { playerName.push_back('U'); }
	if (AEInputCheckTriggered(AEVK_V)) { playerName.push_back('V'); }
	if (AEInputCheckTriggered(AEVK_W)) { playerName.push_back('W'); }
	if (AEInputCheckTriggered(AEVK_X)) { playerName.push_back('X'); }
	if (AEInputCheckTriggered(AEVK_Y)) { playerName.push_back('Y'); }
	if (AEInputCheckTriggered(AEVK_Z)) { playerName.push_back('Z'); }

}

void displayPlayerInputPopup(s8 FONT , bool* showPopup, AEGfxVertexList* pMesh, AEGfxVertexList* txtBoxMesh, AEGfxVertexList* closeMesh, AEGfxVertexList* confirmMesh, int* currentLevel)
{
	updatePlayerInput();

	mesh_Draw(0.0f, 400.0f, 200.0f, 0.0f, 100.0f, pMesh);

	mesh_Draw(0.0f, 300.f, 75.0f, 0.0f, 100.0f, txtBoxMesh);

	AEGfxPrint(FONT, playerName.c_str(), (0.0f-playerName.length()*0.01f), 0.2f, 0.4f, 0.f, 0.f, 0.f, 1.f);

	mesh_Draw(0.f, 30.f, 30.f, 170.f, 170.f, closeMesh);
	AEGfxPrint(FONT, "X", 0.2f, 0.35f, 0.5f, 1.f, 1.f, 1.f, 1.f);

	if (checkCursorCollisionWithRectangle(970.f, 280.f, 30.f, 30.0f)) {
		if (AEInputCheckTriggered(AEVK_LBUTTON)) {
			*showPopup = false;
		}
	}

	mesh_Draw(0.f, 30.f, 30.f, 0.f, 30.f, confirmMesh);
	AEGfxPrint(FONT, "Y", -0.0125f, 0.035f, 0.5f, 0.f, 0.f, 0.f, 1.f);

	if (checkCursorCollisionWithRectangle(800.f, 420.f, 30.f, 30.0f)) {
		if (AEInputCheckTriggered(AEVK_LBUTTON)) {
			*showPopup = false;
			*currentLevel = GS_LEVEL1;
		}
	}
	if (AEInputCheckTriggered(AEVK_RETURN))
	{
		*showPopup = false;
		*currentLevel = GS_LEVEL1;
	}
}

void start_Jump_High(Player* player) {
	if (!player->isJumping && player->onGround) {
		player->isJumping = true;
		player->vertical_velocity = 1000.0;
		player->inWater = false;
		player->onGround = false;
	}
}

void update_Jump_High(Player* player, Level* levels, float dt) {
	Room* currentRoom = &levels[player->currLevel].rooms[player->currRoom];
	if (!player->isJumping)
	{
		return;
	}
	else
	{
		player->onGround = false;
	}
	const int steps = 5;
	float stepDT = dt / steps; // we basically split the frame into 5 steps
	for (int i = 0; i < steps; i++)
	{
		player->vertical_velocity -= f32(player->gravity * 1.5 * stepDT);
		player->posY += f32(player->vertical_velocity * 1.5 * stepDT);
		apply_Player_Room_Boundary(player, currentRoom);
		f32 roomBottom = currentRoom->posY - currentRoom->scaleY / 2.f;
		f32 roomTop = currentRoom->posY + currentRoom->scaleY / 2.f;
		if (check_Collision_Player_Water_For_Jump(player, currentRoom))
		{
			if (player->vertical_velocity >= 0.f)
			{
				continue;
			}
			player->vertical_velocity = 0.f;
			player->isJumping = false;
			//player->onGround = true;
			break;
		}
		if (player->posY < roomBottom + player->scaleY / 2.f)
		{
			player->posY = roomBottom + player->scaleY / 2.f;
			player->vertical_velocity = 0.0f;
			player->isJumping = false;
			player->onGround = true;
			break;
		}
		else if (player->posY >= roomTop - player->scaleY / 2.f)
		{
			/*player->vertical_velocity = 0.0f;
			player->isJumping = true;*/
			if (player->vertical_velocity > 0.f)
			{
				player->vertical_velocity *= -0.2f; // small bounce-back (reflection)
				if (fabs(player->vertical_velocity) < 5.0f) { // very small bounce, just clamp to 0
					player->vertical_velocity = 0.f;
				}
			}
			break;
		}
		if (player->isJumping && player->vertical_velocity <= 0.f) {
			player->isJumping = false;
		}
	}
}

void update_Overall_Jump(Player* player, Level levels[], float dt) {
	if (!player->onRope) {
		// Check if the player can jump (not currently jumping or crouching)
		if (AEInputCheckTriggered(AEVK_SPACE) && !player->isJumping && !player->isCrouching && player->onGround) {
			// Get the current room the player is in
			Room* currRoom = &levels[player->currLevel].rooms[player->currRoom];

			// Get the highest water surface above the player
			float highestWaterTop = get_Highest_Water_Top_Above_Player(player, currRoom);

			// Decide if the player can jump
			if (highestWaterTop <= groundLevel || highestWaterTop < player->posY) {
				start_Jump_High(player);  // Normal jump
				player->onRope ? player->onRope = false : player->onRope = player->onRope;
			}
			else if (highestWaterTop > groundLevel) {
				float dist = static_cast<float>(fabs((highestWaterTop)-player->posY));
				if (dist <= 5.f && player->onGround) {
					start_Jump_High(player);  // Allow jumping if close to the water surface
					player->onRope ? player->onRope = false : player->onRope = player->onRope;
				}
			}
		}
		// Update jump logic
		update_Jump_High(player, levels, dt);
	}
}

void player_remain_crouched(Player* player) {
	player->scaleY = 50.0f;
	player->max_speed = 200.0f;
	player->posY -= 25.0f;

	player->velX *= 0.98f;
}

void player_uncrouch(Player* player) {
	player->scaleY = 100.0f;
	player->posY += 25.0f;
	player->max_speed = 400.0f;
	player->velX *= 0.98f;
}

bool player_Rope_Collision(Player* player, const Rope* rope) {
	f32 player_left = player->posX - player->scaleX / 2;
	f32 player_top = player->posY + player->scaleY / 2;
	f32 player_right = player->posX + player->scaleX / 2;
	f32 player_bottom = player->posY - player->scaleY / 2;

	f32 objects_left = rope->posX - rope->scaleX / 2;
	f32 objects_top = rope->posY + rope->scaleY / 2;
	f32 objects_right = rope->posX + rope->scaleX / 2;
	f32 objects_bottom = rope->posY - rope->scaleY / 2;

	return (player_left < objects_right && player_right > objects_left && player_top > objects_bottom && player_bottom < objects_top);
}


void tryAttachToRope(Player* player, const Rope ropes[], int numRopes) {
	if (player->ropeCooldown > 0.f || player->isCrouching)
	{
		return;
	}
	if (!player->onRope && AEInputCheckTriggered(AEVK_E))
	{
		for (int i = 0; i < numRopes; i++)
		{
			if (ropes[i].locationOfRope != (GS_STATES)player->currLevel)
			{
				continue;
			}
			if (player_Rope_Collision(player, &ropes[i]))
			{
				player->onRope = true;
				player->isJumping = false;
				player->ropeAttachedIndex = i;
				player->vertical_velocity = 0.f;
				player->ropeCooldown = 0.2f;
				player->posX = ropes[i].posX;
				player->onGround = true;
				break;
			}
		}
	}
	else if (player->onRope && AEInputCheckTriggered(AEVK_E))
	{
		player->onRope = false;
		player->ropeAttachedIndex = -1;
		player->ropeCooldown = 0.2f;
	}
}

void update_Player_Rope(Player* player, float dt)
{
	if (player->onRope)
	{
		int ropeIdx = player->ropeAttachedIndex;
		if (ropeIdx < 0)
		{
			return;
		}
		if (AEInputCheckTriggered(AEVK_SPACE))
		{
			player->onRope = false;
			player->ropeAttachedIndex = -1;
			player->ropeCooldown = 0.2f;

			start_Jump_High(player);
			player->vertical_velocity -= f32(player->gravity * 1.5 * dt);
			player->posY += f32(player->vertical_velocity * 1.5 * dt);

			return;
		}
		if (AEInputCheckCurr(AEVK_A) || AEInputCheckCurr(AEVK_D))
		{
			player->onRope = false;
			player->ropeAttachedIndex = -1;
			player->ropeCooldown = 0.2f;
			return;
		}
		if (AEInputCheckCurr(AEVK_W))
		{
			player->posY += 200.f * dt;
		}
		else if (AEInputCheckCurr(AEVK_S))
		{
			player->posY -= 200.f * dt;
		}

	}
}

void updatePlayerRopeCooldown(Player* player, float dt) {
	if (player->ropeCooldown > 0.0f) {
		player->ropeCooldown -= dt;
		if (player->ropeCooldown < 0.0f)
			player->ropeCooldown = 0.0f;
	}
}

void handleCrouchStandAction(Player* player, Platform platforms[], int no_of_platforms){
	// check if player is crouched in water, if so, automatically uncrouch 
	if (player->isCrouching && player->inWater) {
		player->isCrouching = false;
		player->animationState = ANIM_IDLE;
		player->animationTimer = 0.0f;
		player->currentFrame = 0;
		player_uncrouch(player);
	}

	if (AEInputCheckTriggered(AEVK_LSHIFT) && !player->inWater) {
		if (player->isCrouching) {
			// Player is currently crouching and wants to stand up
			bool canStand = true;
			for (int i = 0; i < no_of_platforms; i++) {
				if (check_Collision_Player_Platforn_While_Crouching(player, &platforms[i])) {
					canStand = false;
					break;
				}
			}
			if (canStand) {
				// If no collision is detected above, allow the player to stand
				startUncrouchAnimation(player);
			}       
		}
		else {
			// Player is not crouching and wants to crouch
			player->isCrouching = true;
			startCrouchAnimation(player);
		}
	}
}


void update_Movement(Player *player, /*float *ropeCooldown ,*/float dt) {
	if (player->onRope)
	{
		return;
	}
	if (player->inWater) {
		player->max_speed = 250.0f;
		player->gravity = 0.0f;

		if (AEInputCheckCurr(AEVK_A)) {
			player->posX -= player->max_speed * dt;
		}
		if (AEInputCheckCurr(AEVK_D)) {
			player->posX += player->max_speed * dt;
		}
	}
	else if (!player->isCrouching) {
		player->acceleration = 800.0f;
		player->gravity = 3000.0f;
	}
	if (!player->inWater) { // realistic movement based on accleration and velocity 
		if (AEInputCheckCurr(AEVK_A)) {
			player->velX -= player->acceleration * dt;

		}
		if (AEInputCheckCurr(AEVK_D)) {
			player->velX += player->acceleration * dt;
		}
		if (fabs(player->velX) >=3.f && !player->isCrouching && !player->isJumping)
		{
			if (player->animationState != ANIM_WALK) {
				player->animationState = ANIM_WALK;
				player->animationTimer = 0.0f; // reset the walk animation timer
			}
		}
		else if (!player->isCrouching && !player->isJumping)
		{
			resetAnimation(player);
		}
	}

	if (!player->inWater) {
		// Apply velocity cap
		if (player->velX > player->max_speed) player->velX = player->max_speed;
		if (player->velX < -player->max_speed) player->velX = -player->max_speed;
	}

	// Apply velocity damping (friction)
	player->velX *= 0.99f;

	// Update position based on velocity only when not in water 
	if (!player->inWater) {
		player->posX += player->velX * dt;
	}
}

void player_respawn(Player* player, square* oxygenLevel, square* oxygen_level_bg, int *currentLevel){
	if (AEInputCheckCurr(AEVK_R)) {
		player->posX = -700.0;
		player->posY = -400.0;
		player->currLevel = GS_LEVEL1;
		player->currRoom = 0;
		player->currhealth = 1000.f;
		oxygenLevel->scaleX = oxygen_level_bg->scaleX;
		*currentLevel = GS_LEVEL1;
	}
}

void startCrouchAnimation(Player* player) {
	// Record current "standing" state:
	player->standScaleY = player->scaleY;
	player->standPosY = player->posY;     // current y pos
	player->crouchScaleY = 50.0f;         // desired crouched scale
	player->crouchPosY = player->posY - 25.0f;  // adjust y pos
	player->animationState = ANIM_CROUCH;
	player->animationTimer = 0.0f;
	player->playerTotalAnimationTime = 1.f;

}

void startUncrouchAnimation(Player* player) {
	player->animationState = ANIM_UNCROUCH;
	player->standPosY = player->posY+25.f;
}


void update_Player_Animation(Player* player, float dt)
{
	if (player->animationState == ANIM_IDLE)
	{
		return;
	}
	if (player->animationState == ANIM_CROUCH)
	{
		const int totalFrames = 8;
		const float totalAnimationTime = 1.f;
		float frameTime = totalAnimationTime / totalFrames;

		player->animationTimer += dt;
		int frameIndex = (int)(player->animationTimer / frameTime);
		if (frameIndex >= totalFrames)
		{
			frameIndex = totalFrames-1;
			player->animationTimer = player->playerTotalAnimationTime;
		}

		float t = frameIndex / (float)(totalFrames-1);
		player->scaleY = player->standScaleY + t*(player->crouchScaleY - player->standScaleY);
		player->animationOffset = t * (player->crouchPosY - player->standPosY);
		//player->posY = player->standPosY + t * (player->crouchPosY - player->standPosY);

		player->currentFrame = frameIndex;
	}
	else if (player->animationState == ANIM_UNCROUCH)
	{
		const int totalFrames = 8;
		const float totalAnimationTime = 1.f;
		float frameTime = totalAnimationTime / totalFrames;

		player->animationTimer -= dt;
		if (player->animationTimer < 0.f)
		{
			player->animationTimer = 0.f;
		}
		int frameIndex = (int)(player->animationTimer / frameTime);

		float t = frameIndex / (float)(totalFrames - 1);
		player->scaleY = player->standScaleY + t * (player->crouchScaleY - player->standScaleY);
		player->animationOffset = t * (player->crouchPosY - player->standPosY);
		//player->posY = player->standPosY + t * (player->crouchPosY - player->standPosY);

		if (frameIndex <= 0)
		{
			frameIndex = 0;
			resetAnimation(player);
			player->isCrouching = false;
			player->scaleY = 100.0f;
			player->posY = player->standPosY;
			player->max_speed = 400.0f;
			player->velX *= 0.98f;
			player->animationOffset = 0.0f;
		}

		player->currentFrame = frameIndex;
	}
	else if (player->animationState == ANIM_WALK)
	{
		const int totalFrames = 4;
		const float totalAnimationTime = 2.f;
		float frameTime = totalAnimationTime / totalFrames;
		player->animationTimer += dt;
		int frameIndex = (int)(player->animationTimer / frameTime);
		if (frameIndex >= totalFrames)
		{
			frameIndex = 0;
			player->animationTimer = 0.0f;
		}
		player->currentFrame = frameIndex;
	}
}

void resetAnimation(Player* player) {
	player->animationState = ANIM_IDLE;
	player->animationTimer = 0.0f;
	player->currentFrame = 0;
}

void draw_Player(Player* player, AEGfxTexture* textureWalk, AEGfxTexture* textureCrouch)
{
	if (player->animationState == ANIM_CROUCH)
	{
		int totalFrames = 8;
		float margin = 325.f / 6144.f;
		float marginVert = 37.f / 768.f;
		float spriteContentWidth = (1.0f - totalFrames * margin) / totalFrames;
		float blockwidth = spriteContentWidth + margin;
		//float frameWidth = 1.0f / totalFrames;
		int frameIndex = player->currentFrame;
		/*float u0 = frameIndex * frameWidth;
		float v0 = 0.f;
		float u1 = u0 + frameWidth;
		float v1 = 1.f;*/

		float u0 = (margin / 2.0f) + frameIndex * blockwidth;
		float u1 = u0 + spriteContentWidth;

		float v0 = frameIndex*marginVert;
		float v1 = 1.0f;

		AEGfxMeshStart();
		AEGfxTriAdd(
			-0.5f, -0.5f, 0xFFFFFFFF, u0, v1,
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxTriAdd(
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			0.5f, 0.5f, 0xFFFFFFFF, u1, v0,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxVertexList* spriteMesh = AEGfxMeshEnd();

		//float renderPosY = player->posY + player->animationOffset;

		mesh_Draw_With_Texture(player->rotation, player->scaleX, player->scaleY, player->posX, player->posY, spriteMesh, textureCrouch);
		AEGfxMeshFree(spriteMesh);
	}
	else if (player->animationState == ANIM_UNCROUCH)
	{
		int totalFrames = 8;
		//float frameWidth = 1.0f / totalFrames;
		float margin = 325.f / 6144.f;
		float marginVert = 37.f / 768.f;
		float spriteContentWidth = (1.0f - totalFrames * margin) / totalFrames;
		float blockwidth = spriteContentWidth + margin;
		int frameIndex = player->currentFrame;
		/*float u0 = frameIndex * frameWidth;
		float v0 = 0.f;
		float u1 = u0 + frameWidth;
		float v1 = 1.f;*/

		float u0 = (margin / 2.0f) + frameIndex * blockwidth;
		float u1 = u0 + spriteContentWidth;

		float v0 = frameIndex * marginVert;
		float v1 = 1.0f;
		AEGfxMeshStart();
		AEGfxTriAdd(
			-0.5f, -0.5f, 0xFFFFFFFF, u0, v1,
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxTriAdd(
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			0.5f, 0.5f, 0xFFFFFFFF, u1, v0,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxVertexList* spriteMesh = AEGfxMeshEnd();
		//float renderPosY = player->posY + player->animationOffset;

		mesh_Draw_With_Texture(player->rotation, player->scaleX, player->scaleY, player->posX, player->posY, spriteMesh, textureCrouch);
		AEGfxMeshFree(spriteMesh);
	}
	else if (player->animationState == ANIM_WALK)
	{
		int totalFrames = 4;
		float margin = 325.f/3072.f;
		float spriteContentWidth = (1.0f - totalFrames * margin) / totalFrames;
		float blockwidth = spriteContentWidth + margin;
		//float frameWidth = 1.0f / totalFrames;
		int frameIndex = player->currentFrame;
		//float u0 = frameIndex * frameWidth;
		//float v0 = 0.f;
		//float u1 = u0 + frameWidth;
		//float v1 = 1.f;

		float u0 = (margin / 2.0f) + frameIndex * blockwidth;
		float u1 = u0 + spriteContentWidth;

		float v0 = 0.0f;
		float v1 = 1.0f;
		AEGfxMeshStart();
		AEGfxTriAdd(
			-0.5f, -0.5f, 0xFFFFFFFF, u0, v1,
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxTriAdd(
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			0.5f, 0.5f, 0xFFFFFFFF, u1, v0,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxVertexList* spriteMesh = AEGfxMeshEnd();
		mesh_Draw_With_Texture(player->rotation, player->scaleX, player->scaleY, player->posX, player->posY, spriteMesh, textureWalk);
		AEGfxMeshFree(spriteMesh);
	}
	else if (player->animationState == ANIM_IDLE)
	{
		int totalFrames = 4;
		float margin = 325.f / 3072.f;
		float spriteContentWidth = (1.0f - totalFrames * margin) / totalFrames;
		float blockwidth = spriteContentWidth + margin;
		int frameIndex = player->currentFrame;
		float u0 = (margin / 2.0f) + frameIndex * blockwidth;
		float u1 = u0 + spriteContentWidth;

		float v0 = 0.0f;
		float v1 = 1.0f;
		AEGfxMeshStart();
		AEGfxTriAdd(
			-0.5f, -0.5f, 0xFFFFFFFF, u0, v1,
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxTriAdd(
			0.5f, -0.5f, 0xFFFFFFFF, u1, v1,
			0.5f, 0.5f, 0xFFFFFFFF, u1, v0,
			-0.5f, 0.5f, 0xFFFFFFFF, u0, v0);
		AEGfxVertexList* spriteMesh = AEGfxMeshEnd();
		mesh_Draw_With_Texture(player->rotation, player->scaleX, player->scaleY, player->posX, player->posY, spriteMesh, textureWalk);
		AEGfxMeshFree(spriteMesh);
	}
}