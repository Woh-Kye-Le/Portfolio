

//#include "bullet.h"
#include "game.h"
#include "cprocessing.h"
#include <math.h>
//
//CP_Image type4Bullet;
//CP_Image type5Bullet;
//CP_Image type6Bullet;


/*
* Done By Jethro
* this function is designed to create the Bullets
* It sets the colour of the bullet, followed by drawing the bullet image with the provided values.
* this function takes in parameter: address of Bullet
*/
void bullet_Fire(Bullet* bulletGeneric, CP_Image genericBullet) {
	CP_Image_Draw(genericBullet, bulletGeneric->x + bulletGeneric->width / 2, bulletGeneric->y + bulletGeneric->height / 2, bulletGeneric->width, bulletGeneric->height, 255);
	//CP_Graphics_DrawEllipse(bulletGeneric->x, bulletGeneric->y, bulletGeneric->width, bulletGeneric->height);
}

/*
* Done By: Jethro
* this function is designed to handle ranged enemy detection and attack
* First, the function loops through the number Of ranged enemies.
* Next, the function checks if the player is within the ranged attack range of the ranged enemy, and increase the counter. The longer the player is within range, the higher the counter will become.
* Once the counter increment has reached the designated amount equalizing the fireRate, the enemy will fire a bullet. This is done by creating a new bullet struct, based on the current enemy coords
* as its x and y start point, together with the default damage and size values. Its speed x and y is set by the directional vector of the player location
* minues the current enemy location or firing point to be exact. There are two cases of handling this, one is if player is on the right, and the other on the left
* If none of the conditions are met, the ranged enemy will stay in patrolling mode (To Be Implemented)
* this function takes in parameter: number of ranged enemies, address of numberOfBulletsPresent, address of player, array of ranged Enemies structs and array of bullet structs
*/
void check_Counter_And_Fire_Bullets(int numOfRangedEnemies, int* numberOfBulletsPresent, Character* player1, Enemies arrayOfRangedEnemies[], Bullet arrayOfBullets[])
{

	for (int i = 0; i < numOfRangedEnemies; i++)
	{
		float leftOfEnemy = player1->x - arrayOfRangedEnemies[i].x; // diff between player from enemy original x, aka left
		float rightOfEnemy = player1->x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width; // diff between player from enemy right 
		float centerOfEnemy = player1->x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width/2; // diff between player from enemy right 
		float yOfEnemy = player1->y - arrayOfRangedEnemies[i].y; // diff of player from y of enemy

		// Check if Player is within detection of each RangedEnemy
		//if (1 == arrayOfRangedEnemies[i].isAlive && ((player1->y - arrayOfRangedEnemies[i].y - arrayOfRangedEnemies[i].height < 300.0 && arrayOfRangedEnemies[i].y - player1->y - player1->y < 300.0) && (arrayOfRangedEnemies[i].x - player1->x - player1->width < 300.0 && arrayOfRangedEnemies[i].x - player1->x - player1->width > 0.0 || player1->x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width < 300.0 && player1->x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width > 0.0))) // if dist between front of ranged in less than 500
		if (1 == arrayOfRangedEnemies[i].isAlive && ((player1->y - arrayOfRangedEnemies[i].y - arrayOfRangedEnemies[i].height < 300.0 && arrayOfRangedEnemies[i].y - player1->y - player1->y < 300.0) && (arrayOfRangedEnemies[i].x - player1->x - player1->width < 300.0 && player1->x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width < 300.0))) // if dist between front of ranged in less than 500
		{
			// If player is in range, and counter has not equalized to fireRate, increment counter.
			if (arrayOfRangedEnemies[i].counterToNextShot < arrayOfRangedEnemies[i].fireRate) {
				arrayOfRangedEnemies[i].counterToNextShot++;
			}
			else
			{
				CP_Vector vec_a = CP_Vector_Set(centerOfEnemy, yOfEnemy);
				CP_Vector norm_v = CP_Vector_Normalize(vec_a);
				Bullet bulletToBeAppended = { arrayOfRangedEnemies[i].x + arrayOfRangedEnemies[i].width / 2, arrayOfRangedEnemies[i].y + arrayOfRangedEnemies[i].height / 2, 25.f, 25.f, norm_v.x, norm_v.y, arrayOfRangedEnemies[i].x, arrayOfRangedEnemies[i].y, 25.f, arrayOfRangedEnemies[i].attackType, 0 };
				arrayOfBullets[*numberOfBulletsPresent] = bulletToBeAppended;
				*numberOfBulletsPresent += 1;
				arrayOfRangedEnemies[i].counterToNextShot = 0;
			}
		}
	}

}
/*
	* Done By: Kye Le, Jethro
	* AABB Collision, returns if true	
*/
bool check_Collision_Player_Bullet(Character* player, Bullet* bullet) {
	//check horizontal overlap
	bool overlapX = (player->x < bullet->x + bullet->width) && (bullet->x < player->x + player->width);
	//check vertical overlap
	bool overlapY = (player->y < bullet->y + bullet->height) && (bullet->y < player->y + player->height);

	return overlapX && overlapY;
}

//
//
///* Done By : Jethro
//* this function is designed to handle all cases of the bullet hitting an object, and also to handle the limits where a bullet can go before being removed.
//* First, the function loops through the number Of Present bullets. and from there, assign each bullet its new x and y values based on its speed.
//* Next, the function loops through the number of bullers present again, this is because it needs to check through all the bullets first to see if all conditions are met,
//* before drawing the bullet out. As for the conditions, we first find the dist between the bullet and the player. Next, we proceed to the first condition, which is to check
//* if the bullet is out of the current window screen. If it is, we follow the bullet Removal Logic.
//* Bullet Removal Logic: First set the current position of the bullet to that of the new bullet, and then reduces the number of bullets present by one.
//* Next condition is tocheck if the bullet has hit the player. If it has, we reduce the player health by the damage of the bullet post damage calculation func, and then continue with the bullet removal logic.
//* 3rd Condition, we check if bullet has exceeded the limit of the bullet dist travelled, which is found from checking the current x against the original firing points of the bullet.
//* If condition is fulfilled, apply bullet Removal Logic.
//* Last Condition, if bullet collides with platform, apply bullet removal logic.
//* After passing through all the conditions, we draw the Bullet based on the type of damage listed in the array of bullets, eg. 3 => Em damage, denoted by the CP_Image generic bullet being passed in.
//* this function takes in parameter: number of platforms, current char index, address of number of present bullets, address of player, array of plaform structs, array of bullet structs, and the image of the type 3 bullet.

void bullet_Collision_And_Damage(int numberOfPlatforms, int charindex, int* numberOfBulletsPresent, Character* player1, Platform platforms[], Bullet arrayOfBullets[], CP_Image genericBullet)
{
	CP_Image type4Bullet = CP_Image_Load("Assets/greenBullet.png");
	CP_Image type5Bullet = CP_Image_Load("Assets/blueBullet.png");
	CP_Image type6Bullet = CP_Image_Load("Assets/yellowBullet.png");
	for (int a = 0; a < *numberOfBulletsPresent; a++) {
		// set y and y of each bullet to new x and y for the current game state update to account for the speed*dt.
		arrayOfBullets[a].x = arrayOfBullets[a].x + arrayOfBullets[a].speedx;// * dt; removed dt for now
		arrayOfBullets[a].y = arrayOfBullets[a].y + arrayOfBullets[a].speedy;// *dt;
		//loop throught number of bullets present again;
		for (int i = 0; i < *numberOfBulletsPresent; i++) {
			//FInd dist between x and y of player and bullet
			bool hasBulletCollided = check_Collision_Player_Bullet(player1, &arrayOfBullets[i]);
			/*float bulletDistX = fabs(arrayOfBullets[i].x - player1->x);
			float bulletDistY = fabs(arrayOfBullets[i].y - player1->y);*/
			//check if bullet is out of screen, if so, remove bullet from array, and hence the game
			if (arrayOfBullets[i].x < 0 || arrayOfBullets[i].y < 0 || arrayOfBullets[i].x > 1600 || arrayOfBullets[i].y > 900)
			{

				for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
				{
					arrayOfBullets[pos] = arrayOfBullets[pos + 1];
				}

				*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
			}
			//check if bullet is has hit player, if so, reduce player health by the damage dealth of bullet, and then remove bullet from array, and hence the game
			else if (hasBulletCollided)/*(bulletDistX <= player1->width / 2 && bulletDistY <= player1->height / 2)*/
			{
				player1->health = player1->health - damage_Calculation(charindex, arrayOfBullets[i].damageDealt, arrayOfBullets[i].typeOfDamage);// arrayOfBullets[i].damageDealt;
				player1->isHurt = true;
				for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
				{
					arrayOfBullets[pos] = arrayOfBullets[pos + 1];
				}

				*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
			}
			//check if bullet has reached the maximum distance allowed, if so, remove bullet from array, and hence the game
			else if (fabs(arrayOfBullets[i].originalFiringPointX - arrayOfBullets[i].x) > 300.0 || fabs(arrayOfBullets[i].originalFiringPointY - arrayOfBullets[i].y) > 300.0)
			{

				for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
				{
					arrayOfBullets[pos] = arrayOfBullets[pos + 1];
				}

				*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
			}
			else
				//check if bullet has collided with a platform, if so, remove bullet from array, and hence the game
			{
				for (int p = 0; p < numberOfPlatforms; p++)
				{
					if (platforms[p].x <= arrayOfBullets[i].x && platforms[p].x + platforms[p].width >= arrayOfBullets[i].x && platforms[p].y <= arrayOfBullets[i].y && platforms[p].y + platforms[p].height >= arrayOfBullets[i].y)
					{

						for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
						{
							arrayOfBullets[pos] = arrayOfBullets[pos + 1];
						}

						*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
					}
				}
			}
		}
		if (3 == arrayOfBullets[a].typeOfDamage)
		{
			bullet_Fire(&arrayOfBullets[a], genericBullet); // call bullet_Fire fn and pass in current bullet in array to fire bullet if it has passed the checks above.
		}
		else if (4 == arrayOfBullets[a].typeOfDamage)
		{
			bullet_Fire(&arrayOfBullets[a], type4Bullet); // call bullet_Fire fn and pass in current bullet in array to fire bullet if it has passed the checks above.
		}
		else if (5 == arrayOfBullets[a].typeOfDamage)
		{
			bullet_Fire(&arrayOfBullets[a], type5Bullet); // call bullet_Fire fn and pass in current bullet in array to fire bullet if it has passed the checks above.
		}
		else if (6 == arrayOfBullets[a].typeOfDamage)
		{
			bullet_Fire(&arrayOfBullets[a], type6Bullet); // call bullet_Fire fn and pass in current bullet in array to fire bullet if it has passed the checks above.
		}
	}
}



void check_Counter_And_Fire_Bullets_Boss(int* numberOfBulletsPresent, Character* player1, Boss* boss, Bullet arrayOfBulletBoss[], int * berserkMode)
{

	float leftOfBoss = player1->x - boss->x; // diff between player from boss original x, aka left
	float rightOfBoss = player1->x - boss->x - boss->width; // diff between player from boss right 
	float centerOfBoss = player1->x - boss->x - boss->width / 2; // diff between player from enemy right 
	float yOfBoss = player1->y - boss->y; // diff of player from y of boss
	//float centerOfBoss = boss->x + boss->width / 2;

	float berserkModifier = 1.0f;

	if (*berserkMode)
	{
		berserkModifier = 1.25;
	}

	// Check if Player is within detection of each RangedEnemy
	//if (1 == boss->isAlive && ((player1->y - boss->y - boss->height < 300.0 && boss->y - player1->y - player1->y < 300.0) && (boss->x - player1->x - player1->width < 300.0 && boss->x - player1->x - player1->width > 0.0 || player1->x - boss->x - boss->width < 300.0 && player1->x - boss->x - boss->width > 0.0))) // if dist between front of ranged in less than 500
	if (1 == boss->isAlive && ((player1->y - boss->y - boss->height < 300.0 && boss->y - player1->y - player1->y < 300.0) && (boss->x - player1->x - player1->width < 300.0 && player1->x - boss->x - boss->width < 300.0))) // if dist between front of ranged in less than 500
	{
		// If player is in range, and counter has not equalized to fireRate, increment counter.
		if (boss->counterToNextShot < boss->fireRate) {
			boss->counterToNextShot++;
		}
		else
		{
			CP_Vector vec_a = CP_Vector_Set(centerOfBoss, yOfBoss);
			CP_Vector norm_v = CP_Vector_Normalize(vec_a);
			Bullet bulletToBeAppended = {boss->x + boss->width / 2, boss->y + boss->height / 2, 25.f, 25.f, norm_v.x, norm_v.y, boss->x, boss->y, 25.f, boss->attackType, 1 };
			arrayOfBulletBoss[*numberOfBulletsPresent] = bulletToBeAppended;
			// If Player is to the left of enemy, create bullet firing to the left according to the last known direction of player
			//if (player1->x - boss->x <= 0)
			//{
			//	CP_Vector vec_a = CP_Vector_Set(leftOfBoss, yOfBoss);
			//	CP_Vector norm_v = CP_Vector_Normalize(vec_a);
			//	Bullet bulletToBeAppendedBoss = { boss->x, boss->y + boss->height / 2, 25.f, 25.f, norm_v.x, norm_v.y, boss->x, boss->y, 25.f*berserkModifier, boss->attackType, 1 };
			//	arrayOfBulletBoss[*numberOfBulletsPresent] = bulletToBeAppendedBoss;
			//}
			//// If Player is to the right of enemy, create bullet firing to the right according to the last known direction of player
			//else if (player1->x - boss->x > 0)
			//{
			//	CP_Vector vec_a = CP_Vector_Set(rightOfBoss, yOfBoss);
			//	CP_Vector norm_v = CP_Vector_Normalize(vec_a);
			//	Bullet bulletToBeAppendedBoss = { boss->x + boss->width, boss->y + boss->height / 2, 25.f, 25.f, norm_v.x, norm_v.y, boss->x, boss->y, 25.f* berserkModifier, boss->attackType, 1 };
			//	arrayOfBulletBoss[*numberOfBulletsPresent] = bulletToBeAppendedBoss;
			//}

			/*
			else if (player1->x > boss->x && player-)
			{
				CP_Vector vec_a = CP_Vector_Set(rightOfBoss, yOfBoss);
				CP_Vector norm_v = CP_Vector_Normalize(vec_a);
				Bullet bulletToBeAppendedBoss = { boss->x + boss->width, boss->y + boss->height / 2, 25.f, 25.f, norm_v.x, norm_v.y, boss->x, boss->y, 25.f * berserkModifier, boss->attackType, 1 };
				arrayOfBulletBoss[*numberOfBulletsPresent] = bulletToBeAppendedBoss;
			}*/
			*numberOfBulletsPresent += 1;
			boss->counterToNextShot = 0;
		}
	}
}

void bullet_Collision_And_Damage_Boss(int* numberOfBulletsPresent, float * dt, Character* player1, Bullet arrayOfBulletBoss[], CP_Image genericBullet)
{
	for (int a = 0; a < *numberOfBulletsPresent; a++) {
		//set new x and y of each bullet for the current game state update to account for the speed*dt.
		arrayOfBulletBoss[a].x = arrayOfBulletBoss[a].x + 1.5f * arrayOfBulletBoss[a].speedx * *dt;
		arrayOfBulletBoss[a].y = arrayOfBulletBoss[a].y + 1.5f * arrayOfBulletBoss[a].speedy * *dt;
		//loop through number of bullets present
		for (int i = 0; i < *numberOfBulletsPresent; i++) {
			//fInd distance between x and y of player and bullet
			/*float bulletDistX = fabs(arrayOfBulletBoss[i].x - player1->x);
			float bulletDistY = fabs(arrayOfBulletBoss[i].y - player1->y);*/
			bool hasBulletCollided = check_Collision_Player_Bullet(player1, &arrayOfBulletBoss[i]);
			//check if bullet is out of screen, if so, remove bullet from array, and hence the game
			if (arrayOfBulletBoss[i].x < 0 || arrayOfBulletBoss[i].y < 0 || arrayOfBulletBoss[i].x > 1600 || arrayOfBulletBoss[i].y > 700)
			{
				for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
				{
					arrayOfBulletBoss[pos] = arrayOfBulletBoss[pos + 1];
				}
				*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
			}
			//check if bullet hits player, then reduce player health by damage dealt by bullet. Remove bullet from array, and hence the game
			else if (hasBulletCollided)
			{
				player1->health = player1->health - arrayOfBulletBoss[i].damageDealt;
				player1->isHurt = true;
				for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
				{
					arrayOfBulletBoss[pos] = arrayOfBulletBoss[pos + 1];
				}
				*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
			}
			//check if bullet has reached the maximum distance allowed. Remove bullet from array, and hence the game
			else if (fabs(arrayOfBulletBoss[i].originalFiringPointX - arrayOfBulletBoss[i].x) > 1000.0 || fabs(arrayOfBulletBoss[i].originalFiringPointY - arrayOfBulletBoss[i].y) > 1000.0)
			{
				for (int pos = i; pos < *numberOfBulletsPresent - 1; pos++)
				{
					arrayOfBulletBoss[pos] = arrayOfBulletBoss[pos + 1];
				}
				*numberOfBulletsPresent = *numberOfBulletsPresent - 1;
			}
			bullet_Fire(&arrayOfBulletBoss[a], genericBullet); //call bullet_Fire to pass in current bullet in array to fire bullet if it has passed the checks above.
		}
	}
}