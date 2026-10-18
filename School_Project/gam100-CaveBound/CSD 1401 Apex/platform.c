#include "cprocessing.h"
//#include "platform.h"
#include "utils.h"
#include <math.h>
#include "game.h"
#include <stdio.h>
//#include "player.h"
//#include "enemies.h"

#define buffer (3.0f)
CP_Color white;
CP_Color black;

/*
* Jointly done and Updated By All.
* function to initialise platforms for level 1 as well as to return the number of platforms created
* method to create platform: platforms[i] = (platform){ x-coordinate, y-coordinate, width, height }, where i is the index of the array
* number of platform created will be returned for this function
*/
int initialise_Platform(Platform platforms[]) {
	platforms[0] = (Platform){ 0.f, 850.f, 480.f, 40.f }; // Ground platform
	platforms[1] = (Platform){ 480.f, 850.f, 1280.f, 40.f }; // Ground platform continuation*
	platforms[2] = (Platform){ 680, 740, 270, 40 }; // Middle platform
	platforms[3] = (Platform){ 500, 590, 250, 40 }; // MIddle 3rd level platform
	platforms[4] = (Platform){ 1130, 710, 160, 40 }; // Middle right short platform
	platforms[5] = (Platform){ 0, 650, 400, 40 }; // Left 2nd level platform
	platforms[6] = (Platform){ 1570, 700, 90, 40 }; // Right extreme small platform
	platforms[7] = (Platform){ 260, 400, 540, 40 }; // Left 3rd level platform
	platforms[8] = (Platform){ 900, 520, 500, 40 }; // Middle 3rd level platform
	platforms[9] = (Platform){ 0, 360, 100, 40 }; // Top floor left short platform
	platforms[10] = (Platform){ 1520, 360, 100, 40 }; // Top floor right short platform
	platforms[11] = (Platform){ 445, 230, 450, 40 }; // Top floor right short platform
	platforms[12] = (Platform){ 1055, 360, 320, 40 }; // Top floor right short platform

	return 13;
}
/*
* Jointly done and Updated By All.
* function to initialise platforms for level 2 as well as to return the number of platforms created
* method to create platform: platforms[i] = (platform){ x-coordinate, y-coordinate, width, height }, where i is the index of the array
* number of platform created will be returned for this function
*/
int initialise_Platform_Level2(Platform platforms[])
{
	platforms[0] = (Platform){ 0.0, 850.0, 800.0, 30.0 };
	platforms[1] = (Platform){ 800.0, 850.0, 800.0, 30.0 };
	platforms[2] = (Platform){ 941.0, 771.0, 280.0, 35.0 };
	platforms[3] = (Platform){ 941.0, 815.0, 250.0, 35.0 };
	platforms[4] = (Platform){ 1228.0, 823.0, 35.0, 35.0 };
	platforms[5] = (Platform){ 1267.0, 818.0, 39.0, 38.0 };
	platforms[6] = (Platform){ 0.0, 611.0, 193.0, 30.0 };
	platforms[7] = (Platform){ 150.0, 667.0, 320.0, 30.0 };
	platforms[8] = (Platform){ 574.0, 648.0, 440.0, 35.0 };
	platforms[9] = (Platform){ 1224.0, 565.0, 390.0, 40.0 };
	platforms[10] = (Platform){ 1406.0, 606.0, 200.0, 40.0 };
	platforms[11] = (Platform){ 0.0, 431.0, 230.0, 35.0 };
	platforms[12] = (Platform){ 394.0, 431.0, 175.0, 35.0 };
	platforms[13] = (Platform){ 657.0, 431.0, 530.0, 35.0 };
	platforms[14] = (Platform){ 1453.0, 431.0, 200.0, 36.0 };
	platforms[15] = (Platform){ 0.0, 262.0, 254.0, 34.0 };
	platforms[16] = (Platform){ 1443.0, 273.0, 250.0, 34.0 };
	platforms[17] = (Platform){ 412.0, 157.0, 166.0, 34.0 };
	platforms[18] = (Platform){ 950.0, 204.0, 327.0, 37.0 };

	return 19;
}


/*
* Done By: Kye Le
* checking collision between 2 rect(player and platform) using AABB collision detection. Composed of four conditions:
*  1) player->x < platforms->x + platforms->width:
*		- checks if the left edge of the player's rectangle is to the left of the right edge of the platform's rectangle
*		- ensure part of the player is not completely to the right of the platform
*  2) player->x + player->width > platforms->x:
*		- ensures that the right edge of the player's rectangle is to the right of the left edge of the platform's rectangle
*		- ensures that part of the player is not completely to the left of the platform
*  3) player->y < platforms->y + platforms->height:
*		- checks if the top edge of the player's rectangle is above the bottom edge of the platform's rectangle
*		- ensures that part of the player is not completely below the platform
*  4) player->y + player->height > platforms->y:
*		- ensures that the bottom edge of the player's rectangle is below the top edge of the platform's rectangle
*		- ensures that part of the player is not completely above the platform
* this function takes in parameter: address of player, address of platforms
*/
bool check_Collision_Player_Platform(Character* player, Platform* platforms) {
	return (player->x < platforms->x + platforms->width && player->x + player->width > platforms->x &&
		player->y < platforms->y + platforms->height && player->y + player->height > platforms->y);
}


/*
* Done By: Kye Le
* This function only handles collisions without applying gravity
* It works by adjusting the players position based on the overlap between 2 entities
* overlapX is the horizontal overlap while overlapy is the vertical overlap. The fminf function is used to determine the smaller
* overlap in both x and y directions.
* overlapX calculates the minimum distance to move horizontally, either by moving left or right
* overlapY calculates the minimum distance to move vertically, either by moving up or down.
* Handling horizontal collisions: if the player's x-coordinate is less than the platform's x-coordinate, the player is moved left by overlapX
* else the player will move right
* handling vertical collisions: if player is located above the platforms, player y-coordinate will be adjusted upwards by overlapY,
* vertical velocity will be set to 0 to prevent the player from falling, else if player is located below the platforms, the player
* is moved down by overlapY and the player is prevented from any upwards movement by setting vertical velocity to be 0
* this function parameter takes in: address of player, address of platforms
*/
void handle_Collision_Player_Platform(Character* player, Platform* platform) {
	float overlapX = fminf((player->x + player->width) - platform->x, (platform->x + platform->width) - player->x);
	float overlapY = fminf((player->y + player->height) - platform->y, (platform->y + platform->height) - player->y);

	if (overlapX < overlapY) {
		// Horizontal collision
		if (player->x < platform->x) {
			player->x -= overlapX;  // Move left
		}
		else {
			player->x += overlapX;  // Move right
		}
	}

	if (overlapY < overlapX) {
		// Vertical collision
		if (player->y < platform->y) {
			// Collision from above the platform
			player->y -= overlapY;  // Adjust position so player lands on the platform
			player->verticalvelocity = 0;  // Stop downward movement
			player->isJumping = false; // End jumping state

		}
		else {
			// Collision from below the platform
			player->y += overlapY;  // Move player down
			player->verticalvelocity = 0;  // Stop upward movement
		}
	}
}

/*
* Done By: Jethro, AABB Collision by Kye Le
* First, we set the index counter to 0, we then use the counter to loop through the platforms, and check if the enemy has collided with any of them.
* If the enemy has collided with any of the platforms, we give the current index of the platform to the index where we store the index of platforms to be handled for collision, given by platformsToHandleArray.
* Note, for platform 0 collision, we instead give -1 to the platformsToHandleArray index as the value. So eg if collision happens for platforms 0, 3, 4, platformsToHandleArray would be {-1, 3, 4}. This is because
* for integer arrays, checking for null would simply be 0, and this creates an issue when handling platform[0] later.
* checking collision between 2 rect(enemy and platform) using AABB collision detection. Composed of four conditions:
*  1) enemy->x < platforms->x + platforms->width:
*		- checks if the left edge of the enemy's rectangle is to the left of the right edge of the platform's rectangle
*		- ensure part of the enemy is not completely to the right of the platform
*  2) enemy->x + enemy->width > platforms->x:
*		- ensures that the right edge of the enemy's rectangle is to the right of the left edge of the platform's rectangle
*		- ensures that part of the enemy is not completely to the left of the platform
*  3) enemy->y < platforms->y + platforms->height:
*		- checks if the top edge of the enemy's rectangle is above the bottom edge of the platform's rectangle
*		- ensures that part of the enemy is not completely below the platform
*  4) enemy->y + enemy->height > platforms->y:
*		- ensures that the bottom edge of the enemy's rectangle is below the top edge of the platform's rectangle
*		- ensures that part of the enemy is not completely above the platform
* this function takes in parameter: address of enemy, the number of platforms to process, array of platforms, integer array to store index of platforms.
*/
void check_Collision_Enemies_Platform(Enemies* enemy, int numOfPlatforms, Platform platforms[], int platformsToHandleArray[]) {
	//int* platformsToHandle = *platformsToHandleArray;
	int indexCounter = 0;
	//printf("Platform index is %d", numOfPlatforms);
	for (int i = 0; i < numOfPlatforms; i++)
	{
		if (enemy->x < platforms[i].x + platforms[i].width && enemy->x + enemy->width > platforms[i].x &&
			enemy->y < platforms[i].y + platforms[i].height && enemy->y + enemy->height > platforms[i].y)
		{
			if (i == 0)
			{
				platformsToHandleArray[indexCounter] = -1;
			}
			else
			{
				platformsToHandleArray[indexCounter] = i;
				indexCounter += 1;
				//printf("value of 1st index is %d", platformsToHandleArray[indexCounter]);
			}
		}
	}
}

/*
* Done By: Jethro, AABB Collision by Kye Le
* We first loop through the array of platforms to handle for collision, and assign the value of each incremental index to platformIndex. We then create a pointer struct platform
* to the address of the struct platforms of the index of the array of platforms platforms, with the index given by the integer platform index.
* By doing so, we can check via AABB collision detection for the enemies collision.
* It works by adjusting the enemy position based on the overlap between 2 entities
* overlapX is the horizontal overlap while overlapy is the vertical overlap. The fminf function is used to determine the smaller
* overlap in both x and y directions.
* overlapX calculates the minimum distance to move horizontally, either by moving left or right
* overlapY calculates the minimum distance to move vertically, either by moving up or down.
* Handling horizontal collisions: if the enemy's x-coordinate is less than the platform's x-coordinate, the enemy switches states, reversing into the left direction
* else the enemy will reverse state to move right
* handling vertical collisions: if enemy is located above the platforms, enemy y-coordinate will be adjusted upwards by overlapY,
* vertical velocity will be set to 0 to prevent the enemy from falling, else if enemy is located below the platforms, the enemy
* is moved down by overlapY and the enemy is prevented from any upwards movement by setting vertical velocity to be 0
* this function parameter takes in: address of enemy, array of platforms, integer array of index of platforms to handle for collision
*/
void handle_Collision_Enemies_Platform(Enemies* enemy, Platform platforms[], int * noOfPlatforms) {
	for (int i=0; noOfPlatforms[i]; i++)
	{
		//printf("i is %d", i);
		int platformIndex = 0;
		if (noOfPlatforms[i] > 0)
		{
			platformIndex = noOfPlatforms[i];
		}
		Platform* platform = &platforms[platformIndex];
		//printf("Platform index is %d", platformIndex);
		float overlapX = fminf((enemy->x + enemy->width) - platform->x, (platform->x + platform->width) - enemy->x);
		float overlapY = fminf((enemy->y + enemy->height) - platform->y, (platform->y + platform->height) - enemy->y);
		if (overlapX < overlapY) {
			// Horizontal collision
			if (enemy->x < platform->x) {
				enemy->x -= (overlapX + buffer);  // Move left
				enemy->state = 0;
				printf("Enemy Moving Left");
			}
			else {
				enemy->x += (overlapX + buffer);  // Move right
				enemy->state = 1;
				printf("Enemy Moving Right");
			}
		}

		if (overlapY < overlapX) {
			// Vertical collision
			if (enemy->y < platform->y) {
				// Collision from above the platform
				enemy->y -= overlapY;  // Adjust position so enemy lands on the platform

				enemy->verticalvelocity = 0;  // Stop downward movement (for future use if enemy needs to jump)
				//enemy->isJumping = false; // End jumping state (for future use if enemy needs to jump)
			}
			else {
				// Collision from below the platform
				enemy->y += overlapY;  // Move enemy down
				//enemy->verticalVelocity = 0;  // Stop upward movement (for future use if enemy needs to jump)
			}
		}
	}
}