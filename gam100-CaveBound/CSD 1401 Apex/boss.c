#include <cprocessing.h>
#include <stdbool.h>
#include "game.h"
#include <math.h>
#include <stdio.h>

//Done by: Celest
//create Level1 Boss array
int initialise_Level1_Boss(Boss boss[]) { 
	boss[0] = (Boss){ 700.0, 110.0, 120.0, 120.0, 60.0, 0.0, 0.0, 50, 0, 1000.0, true, 0 , 3 }; //initialise Boss in boss[i], numbers changeable
	return 1; //return number of Boss
}

//Done by: Celest
//create level2 boss array
int initialise_Level2_Boss(Boss boss[]) { 
	boss[1] = (Boss){ 800.0, 150.0, 150.0, 150.0, 60.0, 0.0, 0.0, 50, 0, 100.0, 100.0,true }; //initialise Boss in boss[i], numbers changeable
	return 1; //return number of Boss
}

//Done by: Kye Le 
//this function switched betweeen 1 and 0 for boss states  
void switch_Boss_state(Boss* arrayOfBoss) {
	arrayOfBoss->state = 1 - arrayOfBoss->state;
}


//Done by: Kye Le
/*
	* This basically checks if a player is within melee attack range of the boss, if so, we increment the boss counter per iteration until it equalizes with the fire rate, and proceed to inflict damage on the player.
	* We then proceed to check if the player is on the right or left of the boss, if so, the boss will move towards the direction of the player as long as the player is within the detection range of the boss.
*/
void detect_Player_Boss(int numberOfBoss, Character* player, Boss* boss, float* dt) {

	CP_Image BossWalkRight = CP_Image_Load("Assets/bossRight.png");
	CP_Image BossWalkLeft = CP_Image_Load("Assets/bossLeft.png");

	//check if Player is within attack range of Boss, and if not, check if Player within Detection Range.
	if ((boss->x - player->x - player->width < 50.0 && boss->x - player->x - player->width > 0.0 || player->x - boss->x - boss->width < 50.0 && player->x - boss->x - boss->width > 0.0) && (boss->y <= player->y + player->height / 2 && boss->y >= player->y - player->height / 2)) {
		if (boss->counterToNextShot < boss->fireRate) {
			boss->counterToNextShot++;
		}
		else {
			player->health = player->health - 50.0f;
			boss->counterToNextShot = 0;
		}
	} //check if Player is on the left
	else if (boss->x - player->x < 250.0 && boss->x - player->x - player->width >= 0 && boss->y <= player->y + player->height / 2 && boss->y >= player->y - player->height / 2) {
		boss->x = boss->x - 250.0f * *dt;
		CP_Image_Draw(BossWalkLeft, boss->x + boss->width / 2, boss->y + boss->height / 2, boss->width, boss->height, 255);
		if (boss->x <= 485.0f) { // makes sure the boss stays 40 unit off from the left of the platform 
			boss->x = 485.0f;
		}
	} //check if Player on the right
	else if (player->x - boss->x - boss->width < 250.0 && boss->y <= player->y + player->height / 2 && boss->y >= player->y - player->height / 2 && player->x - boss->x - boss->width >= 0) {
		boss->x = boss->x + 250.0f * *dt;
		CP_Image_Draw(BossWalkRight, boss->x + boss->width / 2, boss->y + boss->height / 2, boss->width, boss->height, 255);
		if (boss->x >= 855.0f - boss->width) { //makes sure the boss stays 40 unit off from the right of the platform
			boss->x = 855.0f - boss->width;
		}
	}
}

/* 
* Done by: Celest and Kye Le
* this function checks if player has collide with boss from any sides(top, bottom, right and left). this function first check if the left side of the player is 
* to the left of the right side of the boss. and then it checks if the left side of the boss is to the left of the right side of the player . if 2 of these condition are true,
* player and boss are overlapping horizontally .
* Secondly, the function checks if the top of the player is above the bottom of the boss and then it checks if the top of the boss is above the bottom of the player 
* similarly, if both condition are true, player and boss are overlapping vertically.
* thus if both horizontal and vertical area have been overlapped at the same time, it shows that boss and player collides.
* this function returns true if both overlapX and overlapY are true, meaning both player and boss overlap both horizontally and vertically
*/

bool check_Collision_Player_Boss(Character* player, Boss* boss) {
	//check horizontal overlap
	bool overlapX = (player->x < boss->x + boss->width) && (boss->x < player->x + player->width);
	//check vertical overlap
	bool overlapY = (player->y < boss->y + boss->height) && (boss->y < player->y + player->height);

	return overlapX && overlapY;
}

/*
* Done by: Kye Le
* this function checks if player has collide with the boss particular from above of the boss in addition of a general collision. overlapX checks if the player and the boss 
* overlap horizontally, while overlapY checks if the player and boss overlaps vertically. fromTop, checks if the collision occurs specifically from the top, by first checking 
* if the bottom of the player is close to the top of the boss and within 10 units. and then it checks if 
*
*/
bool check_Collision_Player_Boss_Top(Character* player, Boss* boss) {

	// Check horizontal overlap
	bool overlapX = (player->x < boss->x + boss->width) && (boss->x < player->x + player->width);
	// Check vertical overlap
	bool overlapY = (player->y < boss->y + boss->height) && (boss->y < player->y + player->height);

	// Additional condition to check if the collision is from the top
	// This checks if the bottom of the player is colliding with the top of the boss and the player is above the boss
	bool fromTop = (player->y + player->height <= boss->y + 10) && (player->y + player->height > boss->y);

	return overlapX && overlapY && fromTop;

}

/*
* Done by: Kye Le
* this function checks if a player has collide with the boss particular from below. this function first checks for horizontal overlap in the form of overlapX.
* this condition checks if the player left if to the left of the right side of the boss and if the left side of the boss is to the left of the right side of the player.
* fromBottom ensures that the collision is from below, player->y <= boss->y + boss->height: checks if the top of the player is intersecting with the bottom of the boss. and
* ensures that player is vertically aligned for potential collision. player->y + player->height > boss->y + boss->height: checks if player is always from the bottom of the boss
* as the first conditon can still be triggered if the player is from the top
*/

bool check_Collision_Player_Boss_Bottom(Character* player, Boss* boss) {
	// Check horizontal overlap
	bool overlapX = (player->x < boss->x + boss->width) && (boss->x < player->x + player->width);
	// The top of the player must be just below the bottom of the boss
	// The bottom of the player must be above the bottom of the boss to prevent detecting when the player is entirely below the boss
	bool fromBottom = (player->y <= boss->y + boss->height) && (player->y + player->height > boss->y + boss->height);

	return overlapX && fromBottom;
}

/*
* Done by: Kye Le
* this function handles the collision when a player collides with the boss. this function first calculates the horizontal overlap between the player and the boss.
* it takes the smaller overlap on either side(left or right). then it calculates the vertical overlap between the player and the boss and takes the smaller overlap
* on either side(top or bottom).if overlapX is lesser than overlapY, it indicates a collision from the side. if player is to the left of the boss, the player is shifted left
* by the amount that it overlaps. same thing happens if the player is to the right of the boss. however for vertical collision, if first check if the player is colliding from above,
* if so, the player' vertical velocity is set to 600.0f, introducing a bouncing effect. directionMultiplier: determines if the player moves left or right based on their relative position
* to the boss's center. horizontalChange: Calculates the amount of horizontal movement using horizontalBounceIntensity, damping, and dt. then the player's x value will be adjusted accordingly.
* finally if the collision is from the bottom, the player is shifted down by the overlaped amount. The player's vertical velocity is also set to 0, stopping any upward movement.
*/


void handle_Collision_Player_Boss(Character* player, Boss* boss, float const* dt) {
	float overlapX = fminf((player->x + player->width) - boss->x, (boss->x + boss->width) - player->x);
	float overlapY = fminf((player->y + player->height) - boss->y, (boss->y + boss->height) - player->y);

	static float horizontalBounceIntensity = 4000.0f;
	static float damping = 0.5;
	printf("Initial state - Player X: %f, Speed X: %f, OverlapX: %f, OverlapY: %f\n", player->x, player->speed_x, overlapX, overlapY);

	// Horizontal collision
	if (overlapX < overlapY) {
		if (player->x < boss->x) { //collision from the left
			player->x -= (overlapX + buffer);  //shift to the left
		}
		else { //collision from the right
			player->x += (overlapX + buffer);  //shift to the right
		}
	}
	// Vertical collision
	if (overlapY < overlapX) {
		if (player->y < boss->y) {  // Collision from above
			player->verticalvelocity = 600.0f;  // Set upward bounce velocity
			float directionMultiplier = ((player->x + player->width / 2) < (boss->x + boss->width / 2)) ? -1.f : 1.f;
			float horizontalChange = directionMultiplier * horizontalBounceIntensity * damping * (*dt);
			player->x += horizontalChange;
		}
		else {  // Collision from below
			player->y += overlapY;  // Move player down to resolve overlap
			player->verticalvelocity = 0;  // Stop upward movement
		}
	}
	//printf("Updated state - Player X: %f, Speed X: %f, dt: %f\n", player->x, player->speed_x, *dt);
}
/*
	* Done by: Jia Hui
	* This basically draws out the boss health and its max hp behind it to visualize the remaining boss health.
*/
void drawBossHealthBar(Boss* arrayOfBoss) {
	CP_Settings_RectMode(CP_POSITION_CORNER);
	float bossMaxHP = 1000.0f;
	float hpPercent = (arrayOfBoss->health / bossMaxHP);
	float healthBarWidth = 300.0f;
	float healthBarHeight = 20.0f;

	float healthBarX = 650.0f;
	float healthBarY = 20.0f;

	if (arrayOfBoss->health <= 0.0f) {
		hpPercent = 0.0f;
	}
	if (arrayOfBoss->health >= bossMaxHP) {
		hpPercent = 1.0f;
	}
	// background health bar
	CP_Settings_Fill(CP_Color_Create(132, 31, 45, 255));
	CP_Graphics_DrawRect(healthBarX, healthBarY, healthBarWidth, healthBarHeight);

	// current health bar based on remaining health
	CP_Settings_Fill(CP_Color_Create(255, 0, 0, 255));
	CP_Graphics_DrawRect(healthBarX, healthBarY, healthBarWidth * hpPercent, healthBarHeight);
}
/*
	* Done By: Celest and Jethro
	* Here we basically draw the image of the boss based on the values passed in.
*/
void create_Boss(float x1, float y1, float w, float h, CP_Color bossColour, CP_Color weaponColour) {
	CP_Image BossWalkRight = CP_Image_Load("Assets/bossRight.png");
	CP_Image_Draw(BossWalkRight, x1 + w / 2, y1 + h / 2, w, h, 255);
	CP_Settings_Fill(bossColour);
	//CP_Graphics_DrawRect(x1, y1, w, h);
	CP_Settings_Fill(weaponColour);
}