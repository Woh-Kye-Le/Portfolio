#include "cprocessing.h"
//#include "player.h"
#include "game.h"




/*
* this function is designed to create the health bare
* healthBar->y = player->y * 0.95 is meant to set the current coordinates of the health bar, which is to hover over the player
* Afterwards, it creates two rects, one for the health bar and the other for the maximum hp of the player. This allows for the player to visualize how many hp he has lost.
* this function takes in parameter: address of player, address of platform
*/
//static void create_Health_Bar(Character* player, Platform* healthBar)
//{
//	healthBar->y = player->y * 0.95;
//	create_Rect(healthBar->x, healthBar->y, player->width, healthBar->height, CP_Color_Create(100, 0, 0, 255)); // For Player to visualize max hp bar
//	create_Rect(healthBar->x, healthBar->y, healthBar->width, healthBar->height, CP_Color_Create(0, 255, 0, 255)); // For Current Player Max Hp
//}


/*
* Done by: Kye Le 
* this is a function for character switching between currentCharacterindex 1 and 0
*/

void switch_Character(int* currentCharacterIndex) {
	*currentCharacterIndex = 1 - *currentCharacterIndex; //switch between 1 and 0
}

/*
* Done by: Kye Le
* this function is to check the state of jumping for long jump. this function only allows jumping if the player is not already jumping 
*/
void start_Jump_High(Character* player) {
	if (!player->isJumping) { 
		player->isJumping = true;
		player->verticalvelocity = 800.0;
	}
}

/*
* Done by: Kye Le
* this function is to execute the long jump, this function first check if the isJumping state is true, if its is, gravity will be applied to velocity and the y position of the 
* player will updated based on the vertical velocity. Eventually the player will start to fall and then it will check if the player has landed on the ground and if so the player will be
* locked on to the ground level, vertical velocity will be reseted to 0 and the isJumping flag will be false.
*/
void update_Jump_Long(Character* player, float dt) {

	if (player->isJumping) {
		player->verticalvelocity -= player->gravity * dt; 
		player->y -= player->verticalvelocity * dt; 
		//check if player has reach the ground
		if (player->y >= (groundlevel - player->height)) {
			player->y = (groundlevel - player->height); 
			player->verticalvelocity = 0.0; 
			player->isJumping = false; 
		}
	}
}

/*
* Done by: Kye Le
* this function checks the state of double jump. It first check if the player is not jumping or has remaining jumps left for the double jumping ability. if these condition is true and 
* if the player is not jumping , it will then handle for the player's first jump, isJumping flag is set to true and jumpsLeft will be set to 1. else if the player is jumping, the 
* remaining jumps are deceremented by 1.regardless of whether its the first jump or second jump, vertical velocity is set to 580.0f.
*/
void start_Jump_Double(Character* player) {
	if (!player->isJumping || player->jumpsLeft > 0) {
		if (!player->isJumping) { //start from the first jump 
			player->isJumping = true;
			player->jumpsLeft = 1;
		}
		else {
			player->jumpsLeft--;
		}
		player->verticalvelocity = 580.0;
	}
}

/*
* Done by: Kye Le
* this function is to execute the double jump. the function first checks if the flag is true for isJumping, if so, gravity will be applied to velocity and the y position of the 
* player will updated based on the vertical velocity. it then checks if the player has landed onto ground level and if so, the player will be locked on to the ground level.
* the isJumping flag will be false, meaning player can start jumping again and the jumpsleft is reset to 0.
*/

void update_Jump_Double(Character* player, float dt) {
	if (player->isJumping) {
		player->verticalvelocity -= player->gravity * dt;
		player->y -= player->verticalvelocity * dt;
		//check if player laned on ground
		if (player->y >= (groundlevel - player->height)) {
			player->y = (groundlevel - player->height); 
			player->verticalvelocity = 0.0;
			player->isJumping = false; 
			player->jumpsLeft = 0;
		}
	}
}

/*
* Done by: Kye Le 
* this functions determine the location of player after it has collide with the boss for level 1 and 2. it first check if level is 1 or 2 
* level 1 and level 2 will have different coordinates for player to teleport to.
*/

int player_Location_Set(int *playerstate, Character* player, int* level) {
	if (*level == 1) {
		switch (*playerstate) {

		case 1: //platform 7
			player->x = 400.0f;
			player->y = 400.f - player->height;
			break;
		case 2: //platform 7
			player->x = 440.0f;
			player->y = 400.f - player->height;
			break;
		case 3: //platform 9
			player->x = 25.0f;
			player->y = 360.0f - player->height;
			break;
		case 4: //platform 12
			player->x = 1100.0f;
			player->y = 360.f - player->height;
			break;
		case 5: //platform 8
			player->x = 1000.0f;
			player->y = 520.0f - player->height;
			break;
		case 6: //platform 7
			player->x = 480.0f;
			player->y = 400.f - player->height;
			break;
		}
	}
	else if (*level == 2) {
		switch (*playerstate) {

		case 1: //platform 11
			player->x = 50.0f;
			player->y = 431.0f - player->height;
			break;
		case 2: //platform 12
			player->x = 450.0f;
			player->y = 431.0f - player->height;
			break;
		case 3: //platform 13
			player->x = 700.0f;
			player->y = 431.0f - player->height;
			break;
		case 4: //platform 13
			player->x = 920.0f;
			player->y = 431.0f - player->height;
			break;
		case 5: //platform 18
			player->x = 970.0f;
			player->y = 204.0f - player->height;
			break;
		case 6: //platform 17
			player->x = 425.0f;
			player->y = 157.0f - player->height;
			break;
		}
	}

	return 6;

}