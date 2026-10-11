#include "cprocessing.h"
#include <stdbool.h>
//#include "enemies.h"
#include "game.h"
#include "utils.h"
#include <math.h>

CP_Sound enemyCollision;
/*
* Done By: Jethro
* function to initialise melee enemies for level 1 as well as to return the number of melee enemies created
* method to create meleeEnemies: meleeEnemies[i] = (Enemies){ x-coordinate, y-coordinate, width, height, health, fireRate, counterToNextShot, isAlive, typeOfEnemy }, where i is the index of the array
* number of meleeEnemies created will be returned for this function
*/
int initialise_Level1_Enemies_Melee(Enemies meleeEnemies[]) {
	meleeEnemies[0] = (Enemies){ 470.0, 130.0, 50.0, 100.0, 50.0, 50, 0, true, 1,1, 0,0 };
	meleeEnemies[1] = (Enemies){ 1000.0, 130.0, 50.0, 100.0, 50.0, 50, 0, true, 1,1, 0,0 };
	return 2;
}

/*
* Done By: Jethro
* function to initialise ranged enemies for level 1 as well as to return the number of ranged enemies created
* method to create rangedEnemies: rangedEnemies[i] = (Enemies){ x-coordinate, y-coordinate, width, height, health, fireRate, counterToNextShot, isAlive, typeOfEnemy }, where i is the index of the array
* number of rangedEnemies created will be returned for this function
*/
int initialise_Level1_Enemies_Ranged(Enemies rangedEnemies[]) {
	rangedEnemies[0] = (Enemies){ 20.0, 550.0 , 100.0, 100.0, 50.0, 50, 0, true, 0, 3 ,0 , 0 };
	rangedEnemies[1] = (Enemies){ 470.0, 130.0 , 100.0, 100.0, 50.0, 50, 0, true, 0, 4,0 , 0 };
	rangedEnemies[2] = (Enemies){ 1070.0, 260.0, 100.0, 100.0, 50.0, 50, 0, true, 0, 5, 0, 0 };
	rangedEnemies[3] = (Enemies){ 280.0, 300.0 , 100.0, 100.0, 50.0, 50, 0, true, 0,6, 0, 0 };
	return 4;
}
/*
* Done By: Jethro
* function to initialise melee enemies for level 2 as well as to return the number of ranged enemies created
* method to create meleeEnemies: meleeEnemies[i] = (Enemies){ x-coordinate, y-coordinate, width, height, health, fireRate, counterToNextShot, isAlive, typeOfEnemy }, where i is the index of the array
* number of meleeEnemies created will be returned for this function
*/
int initialise_Level2_Enemies_Melee(Enemies meleeEnemies[]) {
	meleeEnemies[0] = (Enemies){ 1457.0, 175.0, 50.0, 100.0, 50.0, 50, 0, true, 1,1, 0,0 };
	meleeEnemies[1] = (Enemies){ 48.0, 163.0, 50.0, 100.0, 50.0, 50, 0, true, 1,1, 0,0 };
	return 2;
}
/*
* Done By: Jethro
* function to initialise ranged enemies for level 2 as well as to return the number of ranged enemies created
* method to create rangedEnemies: rangedEnemies[i] = (Enemies){ x-coordinate, y-coordinate, width, height, health, fireRate, counterToNextShot, isAlive, typeOfEnemy }, where i is the index of the array
* number of rangedEnemies created will be returned for this function
*/
int initialise_Level2_Enemies_Ranged(Enemies rangedEnemies[]) {
	rangedEnemies[0] = (Enemies){ 443.0, 332.0, 100.0, 100.0, 50.0, 50, 0, true, 0, 3 ,0 , 0 };
	rangedEnemies[1] = (Enemies){ 1475.0, 173.0, 100.0, 100.0, 50.0, 50, 0, true, 0, 4 ,0 , 0 };
	rangedEnemies[2] = (Enemies){ 695.0, 332.0, 100.0, 100.0, 50.0, 50, 0, true, 0, 5 ,0 , 0 };
	rangedEnemies[3] = (Enemies){ 970.0, 107.0, 100.0, 100.0, 50.0, 50, 0, true, 0, 6 ,0 , 0 };


	return 4;
}
/*
* Done By: Jethro
* function to draw ranged Enemies
* method to create rangedEnemies: image of enemies, x and y of enemy center, width and height of enemies, as well as alpha value for transparency
* enemy colour and weapon colour are defunct and for testing purposes.
* this func returns void.
*/
void create_Ranged_Enemy(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour, CP_Image monsterRangedLeft) {
	CP_Image_Draw(monsterRangedLeft, x1, y1, w, h, 255);
	CP_Settings_Fill(enemyColour);
	//CP_Graphics_DrawRect(x1, y1, w, h);
	CP_Settings_Fill(weaponColour);
}
/*
* Done By: Jethro
* function to handle how the melee enemy handles patrolling as well as how it handles detection, reaction and attack
* first, we initialize a boolean disable move reaction = false. This is because the enemy essentially has three states, patrolling, attacking and reaction.
* When the enemy is patrolling, the move reaction would remain false, meaning that the enemy can react to various scenarios, eg. the limit of the distance where the enemy can move based on its original x and y. 
* If it is atttacking, this moving based on the original x and y would be disabled, allowing the enemy to chase down the player regardless of its distance limitations as long as the player is within the range of the enemy.
* The last state, Reaction is handled based on a timer, in which case the enemy must be in the patrolling state, where disableMoveReaction is false. This reaction state essentially means that when the enemy hits the distance
* limitation from its original point, it will pause, before reversing its direction.
* With this, after the initialization of the bool, we then loop through the array of melee enemies. As long as the value of the variable isAlive in the struct for any index of the array of structs is 0, we skip past this enemy,
* as it is dead, and hence we should not be handling any states, collisions, or even spawns at all. This is done by continuing to the next iteration.
* Next, we check if the enemy's y has changed. This is to handle for if the enemy drops to a different platform. If the y has changed, we update the orginal y and x of the enemy. This is to reset the limitations of where the enemy can move for
* its patrolling state. To reduce the predictability of the enemy movements for the player, we reverse the state of the enemy. This state does not mean the patrolling state, but rather a state that determines the directional movement
* of the enemy. 
* Attacking State (damage)
* Next, we check if the player is within the attack range of the enemy, if it is, disableMoveReaction is set to true to disable the patrolling state, and then we draw the enemy to face the player, and proceed to check the counter
* to the next shot. This var in the enemy struct determines the attack rate of the enemy. With every iteration, this counter increases until it equalizes with the fireRate, of which we then proceed to deal damage to the player. Upon dealing damage
* we set the isHurt var of the player struct to true, hence beginning the timer showing the image of the player being damaged. Finally, we reset the counter. The damage inflicted on the player is determined by the base enemy damage given in
* the respective enemy struct, as well as the damage calculation function in utils.c, where the damage is multiplied or reduced based on the current Character index.
* Attacking State (Pursue)
* On the other hand, if player is within detection range of the enemy, the patrolling state would be disabled, however, the enemy would then pursue in the direction of the player.
* Patrolling State
* If none of the damage and pursue conditions are met, the enemy would move based on the directional state it is in, 0 for left, 1 for right.
* Reaction State
* Lastly, we check for if the player is currently in the patrolling State, if the player has reached the limit distance from their original x, we pause the player movement, and set a timer. Once the timer is up, we reverse the directional state
* of the player.
* This function takes in the number of melee enemies, the current character index, a constant float for the pause timer, a const float dt, a reaction timer(once this hits the pause timer, the d state is reversed), the array fo the original
* X and Y of each melee enemy, the player struct, an array of structs of melee enemies, and the various images for the various states of the melee enemies and players.
*/
void detect_Player_Melee(int numberOfMeleeEnemies, int currentChar, float const* pauseTimer, float const* dt, float* reactionTimer, float meleeEnemyOriginalX[], float meleeEnemyOriginalY[], Character* player, Enemies arrayOfMeleeEnemies[], CP_Image monsterMeleeLeftAttack, CP_Image monsterMeleeRightAttack, CP_Image player1HurtRight, CP_Image player1HurtLeft, CP_Image player2HurtRight, CP_Image player2HurtLeft, CP_Image monsterMeleeLeftWalk, CP_Image monsterMeleeRightWalk)
{
	bool disableMoveReaction = false;
	for (int i = 0; i < numberOfMeleeEnemies; i++)
	{
		if (0 == arrayOfMeleeEnemies[i].isAlive)
		{
			continue;
		}
		if (arrayOfMeleeEnemies[i].y > meleeEnemyOriginalY[i] + arrayOfMeleeEnemies[i].height / 2 || arrayOfMeleeEnemies[i].y < meleeEnemyOriginalY[i] - arrayOfMeleeEnemies[i].height / 2)
		{
			meleeEnemyOriginalY[i] = arrayOfMeleeEnemies[i].y;
			meleeEnemyOriginalX[i] = arrayOfMeleeEnemies[i].x;
			arrayOfMeleeEnemies[i].state = 1 - arrayOfMeleeEnemies[i].state;
		}
		// Check if Player is within attack range of each MeleeEnemy, and if not, check if Player within Detection Range.
		if (1 == arrayOfMeleeEnemies[i].isAlive && (arrayOfMeleeEnemies[i].x - player->x - player->width < 25.0 && arrayOfMeleeEnemies[i].x - player->x - player->width > 0.0 || player->x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width < 25.0 && player->x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width > 0.0) && (arrayOfMeleeEnemies[i].y <= player->y + player->height / 2 && arrayOfMeleeEnemies[i].y >= player->y - player->height / 2))
		{
			disableMoveReaction = true;
			//If enemy is in state 0, face left
			if (arrayOfMeleeEnemies[i].x - player->x - player->width > 0.0)
			{
				CP_Image_Draw(monsterMeleeLeftAttack, arrayOfMeleeEnemies[i].x + arrayOfMeleeEnemies[i].width / 2, arrayOfMeleeEnemies[i].y + arrayOfMeleeEnemies[i].height / 2, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, 255);
			}
			//If Enemy is in state 1, face right
			else if (player->x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width > 0.0)
			{
				CP_Image_Draw(monsterMeleeRightAttack, arrayOfMeleeEnemies[i].x + arrayOfMeleeEnemies[i].width / 2, arrayOfMeleeEnemies[i].y + arrayOfMeleeEnemies[i].height / 2, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, 255);
			}
			//increment counter to next shot
			if (arrayOfMeleeEnemies[i].counterToNextShot < arrayOfMeleeEnemies[i].fireRate) {
				arrayOfMeleeEnemies[i].counterToNextShot++;
			}
			else
			{
				if (0 == currentChar && 0 == player->direction)
				{
					CP_Image_Draw(player1HurtRight, player->x + player->width / 2, player->y + player->height / 2, player->width, player->height, 255);
				}
				else if (0 == currentChar && 1 == player->direction)
				{
					CP_Image_Draw(player1HurtLeft, player->x + player->width / 2, player->y + player->height / 2, player->width, player->height, 255);
				}
				else if (1 == currentChar && 0 == player->direction)
				{
					CP_Image_Draw(player2HurtRight, player->x + player->width / 2, player->y + player->height / 2, player->width, player->height, 255);
				}
				else if (1 == currentChar && 1 == player->direction)
				{
					CP_Image_Draw(player2HurtLeft, player->x + player->width / 2, player->y + player->height / 2, player->width, player->height, 255);
				}
				player->health = player->health - damage_Calculation(currentChar, 50.0f, arrayOfMeleeEnemies[i].attackType);//50.0;
				player->isHurt = true;
				arrayOfMeleeEnemies[i].counterToNextShot = 0;
			}
			meleeEnemyOriginalY[i] = arrayOfMeleeEnemies[i].y;
			meleeEnemyOriginalX[i] = arrayOfMeleeEnemies[i].x;
		}
		// Check if player on left
		else if (arrayOfMeleeEnemies[i].x - player->x < 250.0f && arrayOfMeleeEnemies[i].x - player->x - player->width >= 0 && arrayOfMeleeEnemies[i].y <= player->y + player->height / 2 && arrayOfMeleeEnemies[i].y >= player->y - player->height / 2)
		{
			disableMoveReaction = true;
			arrayOfMeleeEnemies[i].x = arrayOfMeleeEnemies[i].x - 250.0f * *dt;
			arrayOfMeleeEnemies[i].state = 0;
			CP_Image_Draw(monsterMeleeLeftWalk, arrayOfMeleeEnemies[i].x + arrayOfMeleeEnemies[i].width / 2, arrayOfMeleeEnemies[i].y + arrayOfMeleeEnemies[i].height / 2, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, 255);
			meleeEnemyOriginalY[i] = arrayOfMeleeEnemies[i].y;
			meleeEnemyOriginalX[i] = arrayOfMeleeEnemies[i].x;
		}
		// Check if Player On Right
		else if (player->x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width < 250.0f && arrayOfMeleeEnemies[i].y <= player->y + player->height / 2 && arrayOfMeleeEnemies[i].y >= player->y - player->height / 2 && player->x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width >= 0)
		{
			disableMoveReaction = true;
			arrayOfMeleeEnemies[i].x = arrayOfMeleeEnemies[i].x + 250.0f * *dt;
			arrayOfMeleeEnemies[i].state = 1;
			CP_Image_Draw(monsterMeleeRightWalk, arrayOfMeleeEnemies[i].x + arrayOfMeleeEnemies[i].width / 2, arrayOfMeleeEnemies[i].y + arrayOfMeleeEnemies[i].height / 2, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, 255);
			meleeEnemyOriginalY[i] = arrayOfMeleeEnemies[i].y;
			meleeEnemyOriginalX[i] = arrayOfMeleeEnemies[i].x;
		}
		else if (0 == arrayOfMeleeEnemies[i].state)
		{
			arrayOfMeleeEnemies[i].x += -150.0f * *dt;
			CP_Image_Draw(monsterMeleeLeftWalk, arrayOfMeleeEnemies[i].x + arrayOfMeleeEnemies[i].width / 2, arrayOfMeleeEnemies[i].y + arrayOfMeleeEnemies[i].height / 2, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, 255);
		}
		else if (1 == arrayOfMeleeEnemies[i].state)
		{
			arrayOfMeleeEnemies[i].x += 150.0f * *dt;
			CP_Image_Draw(monsterMeleeRightWalk, arrayOfMeleeEnemies[i].x + arrayOfMeleeEnemies[i].width / 2, arrayOfMeleeEnemies[i].y + arrayOfMeleeEnemies[i].height / 2, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, 255);
		}
		if (arrayOfMeleeEnemies[i].x <= meleeEnemyOriginalX[i] - 150.0 && disableMoveReaction != true)
		{
			if (*reactionTimer < *pauseTimer) {
				arrayOfMeleeEnemies[i].x += 150.0f * *dt;
				*reactionTimer += *dt;
			}
			else if (*reactionTimer >= *pauseTimer) {
				arrayOfMeleeEnemies[i].state = 1;  // Set state to 1 (moving right)
				*reactionTimer = 0.0f;  // Reset the timer for next pause
			}
			//arrayOfMeleeEnemies[i].state = 1-arrayOfMeleeEnemies[i].state;
			//arrayOfMeleeEnemies[i].state = 1;
		}
		else if (arrayOfMeleeEnemies[i].x >= meleeEnemyOriginalX[i] + 150.0 && disableMoveReaction != true)
		{
			//arrayOfMeleeEnemies[i].state = 1 - arrayOfMeleeEnemies[i].state;
			if (*reactionTimer < *pauseTimer) {
				arrayOfMeleeEnemies[i].x -= 150.0f * *dt;
				*reactionTimer += *dt;
			}
			else if (*reactionTimer >= *pauseTimer) {
				arrayOfMeleeEnemies[i].state = 0;  // Set state to 1 (moving right)
				*reactionTimer = 0.0f;  // Reset the timer for next pause
			}
			//arrayOfMeleeEnemies[i].state = 0;
		}
	}
}
/*
* Done By: Kye Le
* checking collision between 2 rect(enemy and platform) using AABB collision detection. Composed of four conditions:
*  1) player->x < enemy->x + enemy->width:
*		- checks if the left edge of the player's rectangle is to the left of the right edge of the platform's rectangle
*		- ensure part of the player is not completely to the right of the enemy
*  2) player->x + player->width > enemy->x:
*		- ensures that the right edge of the player's rectangle is to the right of the left edge of the enemy's rectangle
*		- ensures that part of the player is not completely to the left of the enemy
*  3) enemy->y < platforms->y + platforms->height:
*		- checks if the top edge of the enemy's rectangle is above the bottom edge of the platform's rectangle
*		- ensures that part of the enemy is not completely below the platform
*  4) enemy->y + enemy->height > platforms->y:
*		- ensures that the bottom edge of the enemy's rectangle is below the top edge of the platform's rectangle
*		- ensures that part of the enemy is not completely above the platform
* this function takes in parameter: address of player, address of enemy
*/
bool check_Collision_Player_Enemies(Character* player, Enemies* enemy) {
	// Check horizontal overlap
	bool overlapX = (player->x < enemy->x + enemy->width) && (enemy->x < player->x + player->width);
	// Check vertical overlap
	bool overlapY = (player->y < enemy->y + enemy->height) && (enemy->y < player->y + player->height);

	return overlapX && overlapY;
}

/*
* Done by; Kye Le and Jia Hui (points handling & sound)
* this function aims to handle collision between player and enmeies. First overlapX is calculated as the horizontal overlap between player and enemy, next overlapY is calculated 
* as the vetical overlap between the player and the enemy. the fmin function takes the smaller of the 2 overlap to determine the depth of penetration in that axis.  if overlapX is lesser
* than overlapY, it is a horizontal collision if the player is on the left side of the enemy, they are shifted left according to the overlap and the buffer and vice versa for the other direction.
* however if overlapY is less than overlapX, it indicates a vertical collision, if the collision if from above the player is bounced upwards with a vertical velocity of 500.0f. else if the player hits 
* the enemy from below the player is moved downwards to resolve the overlap and the player verticalverlocity is set to 0.
* If the player lands off-center, they are nudged slightly left or right based on their position relative to the enemy's center. upon enemy defeat, The enemy is marked as no longer alive (isAlive = 0).
* Different types of enemies give different point values: typeOfEnemy == 1: +30 points.typeOfEnemy == 0: +50 points.
*/
void handle_Collision_Player_Enemies(Character* player, Enemies* enemy, float* dt) {
	float overlapX = fminf((player->x + player->width) - enemy->x, (enemy->x + enemy->width) - player->x);
	float overlapY = fminf((player->y + player->height) - enemy->y, (enemy->y + enemy->height) - player->y);

	enemyCollision = CP_Sound_Load("Assets/collision.wav");

	if (overlapX < overlapY) {
		// Horizontal collision
		if (player->x < enemy->x) {
			player->x -= (overlapX + buffer);  // Move left

		}
		else {
			player->x += (overlapX + buffer);  // Move right
		}
	}

	if (overlapY < overlapX) { // Vertical collision

		if (player->y < enemy->y) { //collision from above the enemy
			player->verticalvelocity = 500.0f; //bounce effect

			if ((player->x + player->width / 2) < (enemy->x + enemy->width / 2)) { //collision from the left side
				player->x -= 500.0f * *dt; //
			}
			else { //collision from the right side 
				player->x += 500.0f * *dt;
			}
			enemy->isAlive = 0;
			CP_Sound_Play(enemyCollision);

			if (enemy->typeOfEnemy == 1) {
				playerPoints += 30;
			}
			else if (enemy->typeOfEnemy == 0) {
				playerPoints += 50;
			}
		}
		else {
			// Collision from below the enemy
			player->y += overlapY;  // Move player down
			player->verticalvelocity = 0;  // Stop upward movement
		}
	}
}