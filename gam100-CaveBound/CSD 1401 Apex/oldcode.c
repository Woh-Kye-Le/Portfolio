///*
//@file       this is file game.c
//@author     Jethro Sung (sung.h)
//@course     CSD1401
//@section    a
//@project    player_state_game
//@date       10/2/24
//@brief      this file contains a collection of definitions of a series of functions in game.h, by including game.h in the file, and game.h in mainmenu.c,
//			mainmenu.c is able to call on the functions in game.h, which processes the input according to the definitions in game.c
//*/
//#include "cprocessing.h"
//#include "mainmenu.h"
//#include "game.h"
//#include "utils.h"
//#include <math.h>
//#include <stdbool.h>
//#include <stdio.h>
//
//#define groundlevel (700.0)
//#define cooldown_5_seconds (5.0)
//
//CP_Color blue;
//CP_Color grey;
//CP_Color red;
//CP_Color white;
//CP_Color black;
//CP_Color green;
//CP_Color darkRed;
//CP_Color yellow;
//
//float dt;
//int window_height;
//int window_width;
//
///*
//	* base struct for Character
//	* x: top left x-coordinate of the Character, y:top left y coordinate of the Character
//	* width: horizontal length of character, height: vertical length of character, speed_x: horizontal velocity(movement speed)
//	* gravity: const variable that pulls the player down, isjumping: jump state, jumpsleft: cals no of jumps left for double jump function
//	* health: represents the remaining health of the player, at 0 the game is over, maxhealth: maximum health cap and starting health of the player.
//*/
//
//typedef struct Character {   // Structure declaration
//	float x;           // X Coordinates of Character
//	float y;       // Y Coordinates of Character
//	float width; // Width of Char
//	float height; // height of char
//	float speed_x; // speed x of char
//	float speed_y; // speed y of char
//	float verticalvelocity; // vertical jumping speed of char
//	float gravity; // gravity of character
//	bool isJumping; // Check if character is currently jumping or not
//	int jumpsLeft; // Represents how many jumps the character has left
//	float health; // Remaining health Of Char
//	float maxHealth; // Max Health of Char
//}Character;// End the structure with a semicolon
//
///*
//	* base struct for Enemies
//	* x: top left x-coordinate of the Enemy, y:top left y coordinate of the Enemy
//	* width: horizontal length of Enemy, height: vertical length of Enemy
//	* health: represents the remaining health of the enemy, fireRate: Represents the time taken for each shot or attack
//	* counterToNextShot: Represents an incremnetal counter, once this matches the fire rate, the attack is made and the counter is reset to zero.
//	* isAlive: Check if Enemy is Alive, else remove Enemy (unused for now), typeOfEnemy: determines if enemy is ranged, melee and potenitally more
//*/
//typedef struct Enemies {   // Structure declaration
//	float x;           // X Coordinates of Enemies
//	float y;       // Y Coordinates of Enemies
//	float width; // width of Enemy
//	float height; // height of Enemy
//	float health; // Enemy Health
//	int fireRate; // Fire Rate/AttackRate
//	int counterToNextShot; //Counter for Fire/AttackRate
//	bool isAlive; // Checks if Enemy is Alive
//	int typeOfEnemy; // If enemy is melee or ranged, 0 if ranged, 1 if not
//}Enemies;// End the structure with a semicolon
//
///*
//* base struct for platform, x: top left x-coordinate of the rect, y:top left y coordinate of the rect
//* width: horizontal length of rect, height: vertical length of rect
//*/
//typedef struct Platform {   // Structure declaration
//	float x;           // X Coordinates of Platform
//	float y;       // Y Coordinates of Platform
//	float width; // width of platform
//	float height; // height of platform
//}Platform;// End the structure with a semicolon
//
///*
//	* base struct for Bullet
//	* x: top left x-coordinate of the Bullet, y:top left y coordinate of the Bullet
//	* width: horizontal length of Bullet, height: vertical length of Bullet, speedy: x movement of bullet, speedy: y movement of bullet
//	* originalFiringPointX: Original starting Point Of Bullet for X coords, originalFiringPointY: same as originalFiringPointX, but for y Coords
//	* damageDealt: represents the damage dealt to the player. typeOfDamage: type of damage dealt by enemy, currently unused
//*/
//typedef struct Bullet {
//	float x;
//	float y;
//	float width;
//	float height;
//	float speedx;
//	float speedy;
//	float originalFiringPointX;
//	float originalFiringPointY;
//	float damageDealt;
//	int typeOfDamage;
//}Bullet;
//
//
////base attribute of a chacter (both character will use the same base)
//Character player1 = { 200.0, 600.0, 50.0, 100.0, 300.0, 0.0, 0.0, 1500.0, false, 0, 1000.0, 1000.0 };
//Platform healthBar = { 0.0, 0.0, 100.0, 10.0 };
//Character character[2];//array for 2 diff characters
//Platform floorOfLevel = { 0.0, 700.0, 1600.0, 200.0 }; // floor of levels
//Bullet arrayOfBullets[100]; //limit number of Bullets,value can be adjusted
//Enemies arrayOfRangedEnemies[100]; //limit number of meleeEnemy,value can be adjusted
//Enemies arrayOfMeleeEnemies[100];  //limit number of rangedEnemy,value can be adjusted
//int currentCharacterIndex = 0;
//int numberOfBulletsPresent = 0; //initialise bullet counter
//int numOfRangedEnemies = 0;  //initialise ranged Enemy counter
//int numOfMeleeEnemies = 0; //initialise melee Enemy counter
//int level = 1; //initial Level is 1
//static void switch_Character();
//static void create_Ranged_Enemy(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour);
//static void start_Jump_Long(Character* player);
//static void update_Jump_Long(Character* player, float dt);
//static void start_Jump_Double(Character* player);
//static void update_Jump_Double(Character* player, float dt);
//static void create_Rect(float x, float y, float width, float height, CP_Color playerColour);
//static void bullet_Fire(Bullet* bulletGeneric);
//static void create_Melee_Enemy(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour);
//static void create_Health_Bar(Character* player, Platform* healthBar);
//void checkLevel(int currentLevel, int* numberOfRangedEnemies, int* numberOfMeleeEnemies, int* numberOfPlatforms, Platform platforms[10], Enemies arrayOfRangedEnemies[100], Enemies arrayOfMeleeEnemies[100]);
//
//void apply_Gravity(Character* player, float dt);
//bool check_Collision(Character* player, Platform* platforms);
//void handle_Collision(Character* player, Platform* platforms);
////bool isEnemyOnPlatform(Enemies* enemy, Platform* platforms);
//
///*
//* function to initialise platforms for level as well as to return the number of platforms created
//* method to create platform: platforms[i] = (platform){ x-coordinate, y-coordinate, width, height }, where i is the index of the array
//* number of platform created will be returned for this function
//*/
//static int initialise_Platform(Platform platforms[]) {
//	platforms[0] = (Platform){ 0.0, 500.0, 400.0, 50.0 };
//	platforms[1] = (Platform){ 0.0, 300.0, 200.0, 50.0 };
//	platforms[2] = (Platform){ 600.0, 500.0, 200.0, 50.0 };
//
//	return 3;
//}
//
///*
//* function to initialise melee enemies for level 1 as well as to return the number of melee enemies created
//* method to create meleeEnemies: meleeEnemies[i] = (Enemies){ x-coordinate, y-coordinate, width, height, health, fireRate, counterToNextShot, isAlive, typeOfEnemy }, where i is the index of the array
//* number of meleeEnemies created will be returned for this function
//*/
//static int initialise_Level1_Enemies_Melee(Enemies meleeEnemies[]) {
//	//rangedEnemies[0] = (Enemies){ 0.0, 400.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	//rangedEnemies[1] = (Enemies){ 0.0, 300.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	//rangedEnemies[0] = (Enemies){ 600.0, 400.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	meleeEnemies[0] = (Enemies){ 1200.0, 600.0, 100.0, 100.0, 50.0, 50, 0, true, 1 };
//	return 1;
//}
//
///*
//* function to initialise ranged enemies for level 1 as well as to return the number of ranged enemies created
//* method to create rangedEnemies: rangedEnemies[i] = (Enemies){ x-coordinate, y-coordinate, width, height, health, fireRate, counterToNextShot, isAlive, typeOfEnemy }, where i is the index of the array
//* number of rangedEnemies created will be returned for this function
//*/
//static int initialise_Level1_Enemies_Ranged(Enemies rangedEnemies[]) {
//	rangedEnemies[0] = (Enemies){ 0.0, 400.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	rangedEnemies[1] = (Enemies){ 0.0, 200.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	rangedEnemies[2] = (Enemies){ 600.0, 400.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	rangedEnemies[3] = (Enemies){ 800.0, 600.0, 100.0, 100.0, 50.0, 50, 0, true, 0 };
//	return 4;
//}
//
//
////  @Game_Init:		Game_Init initializes the game, it first sets the frame rate and then clears the background, and loads the colour of the various colors initialized earlier. Here we also assign the various values 
////					for the circles and health bars.
////  @param:         Game_Init takes in a param of void.
////  @return:        Game_Init returns void
//void Game_Init(void)
//{
//	window_width = CP_System_GetWindowWidth();
//	window_height = CP_System_GetWindowHeight();
//
//	blue = CP_Color_Create(0, 0, 255, 255);
//	red = CP_Color_Create(255, 0, 0, 255);
//	grey = CP_Color_Create(110, 110, 110, 20);
//	white = CP_Color_Create(255, 255, 255, 255);
//	black = CP_Color_Create(0, 0, 0, 255);
//	green = CP_Color_Create(0, 255, 0, 255);
//	darkRed = CP_Color_Create(100, 0, 0, 255);
//	yellow = CP_Color_Create(255, 255, 0, 255);
//}
//
////  @Game_Update:		Main_Menu_Update updates the game, it first obtains the height and width of the window and from there, clears the background. Next it sets the DT to be multiplied by to the speed to obtain the velocity
////						of the player circle. Next we initialized the var representing the number of smaller health bars, as well as the square comparison vars for between the player circle and the red circle, followed by that
////						of the green circle. Next we put in a list of if statements to handle the moving of the player circle via keydown. This is followed by the various checks to see if the player circle has entered
////						the red or green circle, and from there we can reduce the big health bar. this is followed by a for loop to set the number of small health bars according to the remaining width of the big health bar.
////						Lastly we simply draw out the various shapes, and for the small health bars, we loop it through a for loop to display as many as stated in the numOfSmallHealthBars var.
////						
////  @param:				Game_Update takes in a param of void.
////  @return:			Game_Update returns void
//
//void Game_Update(void)
//{
//	CP_Graphics_ClearBackground(grey);
//	dt = CP_System_GetDt();
//	Platform platforms[10]; //limit number of platforms,value can be adjusted
//	int numPlatforms = initialise_Platform(platforms); //set numPlatforms to be total number of platforms created, 
//	//apply gravity 
//	apply_Gravity(&player1, dt);
//	//declare a static variable lastswitchtime and initialised it to 0,it retains the last time a character 
//	//switch occured across the frames 
//	static double lastSwitchtime = 0.0;
//	//current_time keeps track of the current time in seconds
//	double current_time = CP_System_GetSeconds();
//	//check if level is 1
//	checkLevel(level, &numOfRangedEnemies, &numOfMeleeEnemies, &numPlatforms, platforms, arrayOfRangedEnemies, arrayOfMeleeEnemies);
//	/*
//	if (1 == level) {
//		if (numOfRangedEnemies == 0) //check if ranged enemies have been set
//		{
//			numOfRangedEnemies = initialise_Level1_Enemies_Ranged(arrayOfRangedEnemies);  //set numOfRangedEnemies to be starting number of ranged enemies created,
//		}
//		if (numOfMeleeEnemies == 0) //check if melee enemies have been set
//		{
//			numOfMeleeEnemies = initialise_Level1_Enemies_Melee(arrayOfMeleeEnemies);  //set numOfMeleeEnemies to be starting number of melee enemies created,
//		}
//		//create the platforms using a for loop that iterates over the function initialise_Platform
//		for (int i = 0; i < numPlatforms; i++) {
//			create_Rect(platforms[i].x, platforms[i].y, platforms[i].width, platforms[i].height, white);
//		}
//		//create the rangedEnemies using a for loop that iterates over the function create_Ranged_Enemy
//		for (int i = 0; i < numOfRangedEnemies; i++)
//		{
//			create_Ranged_Enemy(arrayOfRangedEnemies[i].x, arrayOfRangedEnemies[i].y, arrayOfRangedEnemies[i].width, arrayOfRangedEnemies[i].height, red, black);
//		}
//		//create the meleeEnemies using a for loop that iterates over the function create_Melee_Enemy
//		for (int i = 0; i < numOfMeleeEnemies; i++)
//		{
//			create_Melee_Enemy(arrayOfMeleeEnemies[i].x, arrayOfMeleeEnemies[i].y, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, green, black);
//		}
//	}
//	*/
//	//Switch between long jump and short jump only when theres no jumping action 
//	//interval of 5 secs: time elapse from the last character switch is calculated from (current_time - lastSwitchtime)
//	//                    it checks if it is greater of equal to the cooldown to prevent 
//	if (!player1.isJumping && CP_Input_KeyReleased(KEY_Q) && (current_time - lastSwitchtime >= cooldown_5_seconds)) {
//		switch_Character();
//		//if character is switched, the lastswitchtime will be set to the currenttime, to ensure the interval of 5 secs has been reset
//		lastSwitchtime = current_time;
//	}
//	//Loop Through Number Of Ranged Enemies
//	for (int i = 0; i < numOfRangedEnemies; i++)
//	{
//		// Check if Player is within detection of each RangedEnemy
//		if (arrayOfRangedEnemies[i].x - player1.x - player1.width < 300.0 && arrayOfRangedEnemies[i].x - player1.x - player1.width > 0.0 || player1.x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width < 300.0 && player1.x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width > 0.0) // if dist between front of ranged in less than 500
//		{
//			// If player is in range, and counter has not equalized to fireRate, increment counter.
//			if (arrayOfRangedEnemies[i].counterToNextShot < arrayOfRangedEnemies[i].fireRate) {
//				arrayOfRangedEnemies[i].counterToNextShot++;
//			}
//			else
//			{
//				// If Player is to the left of enemy, create bullet firing to the left according to the last known direction of player
//				if (player1.x - arrayOfRangedEnemies[i].x <= 0)
//				{
//					Bullet bulletToBeAppended = { arrayOfRangedEnemies[i].x, arrayOfRangedEnemies[i].y + arrayOfRangedEnemies[i].height / 2, 25.f, 25.f, (player1.x - arrayOfRangedEnemies[i].x) / 2, (player1.y - arrayOfRangedEnemies[i].y) / 2, arrayOfRangedEnemies[i].x, arrayOfRangedEnemies[i].y, 25.f, 0 };
//					arrayOfBullets[numberOfBulletsPresent] = bulletToBeAppended;
//					numberOfBulletsPresent += 1;
//					arrayOfRangedEnemies[i].counterToNextShot = 0;
//				}
//				// If Player is to the right of enemy, create bullet firing to the right according to the last known direction of player
//				else if (player1.x - arrayOfRangedEnemies[i].x > 0)
//				{
//					Bullet bulletToBeAppended = { arrayOfRangedEnemies[i].x + arrayOfRangedEnemies[i].width, arrayOfRangedEnemies[i].y + arrayOfRangedEnemies[i].height / 2, 25.f, 25.f, (player1.x - arrayOfRangedEnemies[i].x - arrayOfRangedEnemies[i].width) / 2, (player1.y - arrayOfRangedEnemies[i].y) / 2, arrayOfRangedEnemies[i].x, arrayOfRangedEnemies[i].y, 25.f, 0 };
//					arrayOfBullets[numberOfBulletsPresent] = bulletToBeAppended;
//					numberOfBulletsPresent += 1;
//					arrayOfRangedEnemies[i].counterToNextShot = 0;
//				}
//			}
//		}
//	}
//	//Loop Through Number Of Melee Enemies
//	for (int i = 0; i < numOfMeleeEnemies; i++)
//	{
//		// Check if Player is within attack range of each RangedEnemy, and if not, check if Player within Detection Range.
//		if (arrayOfMeleeEnemies[i].x - player1.x - player1.width < 50.0 && arrayOfMeleeEnemies[i].x - player1.x - player1.width > 0.0 || player1.x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width < 50.0 && player1.x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width > 0.0)
//		{
//			if (arrayOfMeleeEnemies[i].counterToNextShot < arrayOfMeleeEnemies[i].fireRate) {
//				arrayOfMeleeEnemies[i].counterToNextShot++;
//			}
//			else
//			{
//				player1.health = player1.health - 50.0;
//				arrayOfMeleeEnemies[i].counterToNextShot = 0;
//			}
//		}
//		// Check if player on left
//		else if (arrayOfMeleeEnemies[i].x - player1.x < 250.0 && arrayOfMeleeEnemies[i].x - player1.x - player1.width >= 0 && arrayOfMeleeEnemies[i].y <= player1.y + player1.height / 2 && arrayOfMeleeEnemies[i].y >= player1.y - player1.height / 2)
//		{
//			arrayOfMeleeEnemies[i].x = arrayOfMeleeEnemies[i].x - 250.0 * dt;
//
//		}
//		// Check if Player On Right
//		else if (player1.x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width < 250.0 && arrayOfMeleeEnemies[i].y <= player1.y + player1.height / 2 && arrayOfMeleeEnemies[i].y >= player1.y - player1.height / 2 && player1.x - arrayOfMeleeEnemies[i].x - arrayOfMeleeEnemies[i].width >= 0)
//		{
//			arrayOfMeleeEnemies[i].x = arrayOfMeleeEnemies[i].x + 250.0 * dt;
//
//		}
//	}
//
//	//Loop Through Number Of Bullets
//	for (int a = 0; a < numberOfBulletsPresent; a++) {
//		// set y and y of each bullet to new x and y for the current game state update to account for the speed*dt.
//		arrayOfBullets[a].x = arrayOfBullets[a].x + arrayOfBullets[a].speedx * dt;
//		arrayOfBullets[a].y = arrayOfBullets[a].y + arrayOfBullets[a].speedy * dt;
//		//loop throught number of bullets present again;
//		for (int i = 0; i < numberOfBulletsPresent; i++) {
//			//FInd dist between x and y of player and bullet
//			float bulletDistX = fabs(arrayOfBullets[i].x - player1.x);
//			float bulletDistY = fabs(arrayOfBullets[i].y - player1.y);
//			//check if bullet is out of screen, if so, remove bullet from array, and hence the game
//			if (arrayOfBullets[i].x < 0 || arrayOfBullets[i].y < 0 || arrayOfBullets[i].x > 1600 || arrayOfBullets[i].y > 700)
//			{
//
//				for (int pos = i; pos < numberOfBulletsPresent - 1; pos++)
//				{
//					arrayOfBullets[pos] = arrayOfBullets[pos + 1];
//				}
//
//				numberOfBulletsPresent = numberOfBulletsPresent - 1;
//			}
//			//check if bullet is has hit player, if so, reduce player health by the damage dealth of bullet, and then remove bullet from array, and hence the game
//			else if (bulletDistX <= player1.width / 2 && bulletDistY <= player1.height / 2)
//			{
//				player1.health = player1.health - arrayOfBullets[i].damageDealt;
//
//				for (int pos = i; pos < numberOfBulletsPresent - 1; pos++)
//				{
//					arrayOfBullets[pos] = arrayOfBullets[pos + 1];
//				}
//
//				numberOfBulletsPresent = numberOfBulletsPresent - 1;
//			}
//			//check if bullet has reached the maximum distance allowed, if so, remove bullet from array, and hence the game
//			else if (fabs(arrayOfBullets[i].originalFiringPointX - arrayOfBullets[i].x) > 300.0 || fabs(arrayOfBullets[i].originalFiringPointY - arrayOfBullets[i].y) > 300.0)
//			{
//
//				for (int pos = i; pos < numberOfBulletsPresent - 1; pos++)
//				{
//					arrayOfBullets[pos] = arrayOfBullets[pos + 1];
//				}
//
//				numberOfBulletsPresent = numberOfBulletsPresent - 1;
//			}
//			else
//				//check if bullet has collided with a platform, if so, remove bullet from array, and hence the game
//			{
//				for (int p = 0; p < numPlatforms; p++)
//				{
//					if (platforms[p].x <= arrayOfBullets[i].x && platforms[p].x + platforms[p].width >= arrayOfBullets[i].x && platforms[p].y <= arrayOfBullets[i].y && platforms[p].y + platforms[p].height >= arrayOfBullets[i].y)
//					{
//
//						for (int pos = i; pos < numberOfBulletsPresent - 1; pos++)
//						{
//							arrayOfBullets[pos] = arrayOfBullets[pos + 1];
//						}
//
//						numberOfBulletsPresent = numberOfBulletsPresent - 1;
//					}
//				}
//			}
//		}
//		bullet_Fire(&arrayOfBullets[a]); // call bullet_Fire fn and pass in current bullet in array to fire bullet if it has passed the checks above.
//	}
//	// If Player health is not zero, set the health bar based on the percentage of the remaining hp
//	if (player1.health > 0)
//	{
//		healthBar.width = health_Bar_Calculate(player1.health, player1.maxHealth, player1.width);// health bar = player width * percentage of remaining player hp
//		healthBar.x = player1.x;
//	}
//	// else if player has reached 0 health, stop reduction and end game
//	else
//	{
//		healthBar.width = 0.1;
//		healthBar.x = player1.x;
//	}
//	//call upon creat health bar fn to create health bar for player, passing in player struct and health bar struct;
//	create_Health_Bar(&player1, &healthBar);
//	// create floor of game
//	create_Rect(floorOfLevel.x, floorOfLevel.y, floorOfLevel.width, floorOfLevel.height, black);
//
//	//left and right movement for character 
//	if (CP_Input_KeyDown(KEY_D)) {
//		player1.x += player1.speed_x * dt;
//	}
//	if (CP_Input_KeyDown(KEY_A)) {
//		player1.x -= player1.speed_x * dt;
//	}
//
//	//character with long jump
//	if (currentCharacterIndex == 0) {
//		create_Rect(player1.x, player1.y, player1.width, player1.height, blue);
//
//		if (CP_Input_KeyTriggered(KEY_SPACE)) {
//			start_Jump_Long(&player1);
//		}
//		update_Jump_Long(&player1, dt);
//	}
//
//	//character with double jump
//	if (currentCharacterIndex == 1) {
//		create_Rect(player1.x, player1.y, player1.width, player1.height, red);
//
//		if (CP_Input_KeyTriggered(KEY_SPACE)) {
//			start_Jump_Double(&player1);
//		}
//		update_Jump_Double(&player1, dt);
//	}
//	//iterates over each platforms to check for collison between platform and players 
//	for (int i = 0; i < numPlatforms; i++) {
//		if (check_Collision(&player1, &platforms[i])) {
//			create_Rect(platforms[i].x, platforms[i].y, platforms[i].width, platforms[i].height, yellow);//testing
//			handle_Collision(&player1, &platforms[i]);
//
//		}
//	}
//}
////  @Game_Exit:		Game_Exit handles the exiting of the game state, as nothing is needed to be processed here as of yet, it is empty.
////  @param:         Game_Exit takes in a param of void.
////  @return:        Game_Exit returns void
//void Game_Exit(void)
//{
//
//}
//
///*
//	function to create a rectangle based on the xy coords, the width, height and color provided.
//*/
//static void create_Rect(float x, float y, float width, float height, CP_Color rectColour) {
//	CP_Settings_Fill(rectColour);
//	CP_Graphics_DrawRect(x, y, width, height);
//}
//
//void checkLevel(int currentLevel, int* numberOfRangedEnemies, int* numberOfMeleeEnemies, int* numberOfPlatforms, Platform platforms[10], Enemies arrayOfRangedEnemies[100], Enemies arrayOfMeleeEnemies[100])
//{
//	if (1 == currentLevel)
//	{
//		if (*numberOfRangedEnemies == 0) //check if ranged enemies have been set
//		{
//			*numberOfRangedEnemies = initialise_Level1_Enemies_Ranged(arrayOfRangedEnemies);  //set numberOfRangedEnemies to be starting number of ranged enemies created, 
//		}
//		if (*numberOfMeleeEnemies == 0) //check if melee enemies have been set
//		{
//			*numberOfMeleeEnemies = initialise_Level1_Enemies_Melee(arrayOfMeleeEnemies);  //set numberOfMeleeEnemies to be starting number of melee enemies created, 
//		}
//		//create the platforms using a for loop that iterates over the function initialise_Platform 
//		for (int i = 0; i < *numberOfPlatforms; i++) {
//			create_Rect(platforms[i].x, platforms[i].y, platforms[i].width, platforms[i].height, white);
//		}
//		//create the rangedEnemies using a for loop that iterates over the function create_Ranged_Enemy 
//		for (int i = 0; i < *numberOfRangedEnemies; i++)
//		{
//			create_Ranged_Enemy(arrayOfRangedEnemies[i].x, arrayOfRangedEnemies[i].y, arrayOfRangedEnemies[i].width, arrayOfRangedEnemies[i].height, red, black);
//		}
//		//create the meleeEnemies using a for loop that iterates over the function create_Melee_Enemy 
//		for (int i = 0; i < *numberOfMeleeEnemies; i++)
//		{
//			create_Melee_Enemy(arrayOfMeleeEnemies[i].x, arrayOfMeleeEnemies[i].y, arrayOfMeleeEnemies[i].width, arrayOfMeleeEnemies[i].height, green, black);
//		}
//	}
//}
//
///*
//* checking collision between 2 rect(player and platform) using AABB collision detection. Composed of four conditions:
//*  1) player->x < platforms->x + platforms->width:
//*		- checks if the left edge of the player's rectangle is to the left of the right edge of the platform's rectangle
//*		- ensure part of the player is not completely to the right of the platform
//*  2) player->x + player->width > platforms->x:
//*		- ensures that the right edge of the player's rectangle is to the right of the left edge of the platform's rectangle
//*		- ensures that part of the player is not completely to the left of the platform
//*  3) player->y < platforms->y + platforms->height:
//*		- checks if the top edge of the player's rectangle is above the bottom edge of the platform's rectangle
//*		- ensures that part of the player is not completely below the platform
//*  4) player->y + player->height > platforms->y:
//*		- ensures that the bottom edge of the player's rectangle is below the top edge of the platform's rectangle
//*		- ensures that part of the player is not completely above the platform
//* this function takes in parameter: address of player, address of platforms
//*/
//bool check_Collision(Character* player, Platform* platforms) {
//	return (player->x < platforms->x + platforms->width && player->x + player->width > platforms->x &&
//		player->y < platforms->y + platforms->height && player->y + player->height > platforms->y);
//}
//
//
///*
//* This function only handles collisions without applying gravity
//* It works by adjusting the players position based on the overlap between 2 entities
//* overlapX is the horizontal overlap while overlapy is the vertical overlap. The fminf function is used to determine the smaller
//* overlap in both x and y directions.
//* overlapX calculates the minimum distance to move horizontally, either by moving left or right
//* overlapY calculates the minimum distance to move vertically, either by moving up or down.
//* Handling horizontal collisions: if the player's x-coordinate is less than the platform's x-coordinate, the player is moved left by overlapX
//* else the player will move right
//* handling vertical collisions: if player is located above the platforms, player y-coordinate will be adjusted upwards by overlapY,
//* vertical velocity will be set to 0 to prevent the player from falling, else if player is located below the platforms, the player
//* is moved down by overlapY and the player is prevented from any upwards movement by setting vertical velocity to be 0
//* this function parameter takes in: address of player, address of platforms
//*/
//void handle_Collision(Character* player, Platform* platform) {
//	float overlapX = fminf((player->x + player->width) - platform->x, (platform->x + platform->width) - player->x);
//	float overlapY = fminf((player->y + player->height) - platform->y, (platform->y + platform->height) - player->y);
//
//	if (overlapX < overlapY) {
//		// Horizontal collision
//		if (player->x < platform->x) {
//			player->x -= overlapX;  // Move left
//		}
//		else {
//			player->x += overlapX;  // Move right
//		}
//	}
//
//	if (overlapY < overlapX) {
//		// Vertical collision
//		if (player->y < platform->y) {
//			// Collision from above the platform
//			player->y -= overlapY;  // Adjust position so player lands on the platform
//			player->verticalvelocity = 0;  // Stop downward movement
//			player->isJumping = false; // End jumping state
//
//		}
//		else {
//			// Collision from below the platform
//			player->y += overlapY;  // Move player down
//			player->verticalvelocity = 0;  // Stop upward movement
//		}
//	}
//}
///*
//bool isEnemyOnPlatform(Enemies* enemies, Platform* platform) {
//	// Check if enemy's bottom is touching the platform's top
//	bool yCollision = (enemies->y + enemies->height) == platform->y;
//
//	// Check if the enemy's x coordinate intersects with the platform's x coordinate
//	bool xCollision = (enemies->x + enemies->width > platform->x) &&
//		(enemies->x < platform->x + platform->width);
//
//	return yCollision && xCollision;
//}*/
//
///*
//* this function is designed to apply gravitational effect to the player at any point of time,
//* !player->isjumping: even if the player is not technically in a jump, gravity may need to be applied
//* player->y < (groundlevel - player->height): This ensures gravity is only applied if the player is above the ground level.
//* Applying gravity to vetical velocity: player->verticalvelocity -= player->gravity * dt, this effectively increases the downward
//* velocity each frame, simulating the acceleration due to gravity.
//* Afterwards, it will update the player's vertical position through player->y -= player->verticalvelocity * dt.
//* it will then check if the player has made contact with the ground, if so the player's position will be on the ground and
//* the vertical velocity will be set to 0
//* this function takes in parameter: address of player, dt
//*/
//void apply_Gravity(Character* player, float dt) {
//	if (!player->isJumping || player->y < (groundlevel - player->height)) {
//		player->verticalvelocity -= player->gravity * dt;
//		player->y -= player->verticalvelocity * dt;
//
//		// Check if the player has reached the ground
//		if (player->y >= (groundlevel - player->height)) {
//			player->y = (groundlevel - player->height);
//			player->verticalvelocity = 0.0;
//			player->isJumping = false;
//		}
//	}
//}
///*
//* this function is designed to create the health bare
//* healthBar->y = player->y * 0.95 is meant to set the current coordinates of the health bar, which is to hover over the player
//* Afterwards, it creates two rects, one for the health bar and the other for the maximum hp of the player. This allows for the player to visualize how many hp he has lost.
//* this function takes in parameter: address of player, address of platform
//*/
//static void create_Health_Bar(Character* player, Platform* healthBar)
//{
//	healthBar->y = player->y * 0.95;
//	create_Rect(healthBar->x, healthBar->y, player->width, healthBar->height, darkRed); // For Player to visualize max hp bar
//	create_Rect(healthBar->x, healthBar->y, healthBar->width, healthBar->height, green); // For Current Player Max Hp
//}
///*
//* this function is designed to create the meleeEnemies
//* Currently it is the same as create_Rect, however, weapon updates will be added later.
//* this function takes in parameter: x, y, width, height, and color of enemy as well as weapon
//*/
//static void create_Melee_Enemy(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour) {
//	CP_Settings_Fill(enemyColour);
//	CP_Graphics_DrawRect(x1, y1, w, h);
//	CP_Settings_Fill(weaponColour);
//}
//
///*
//* this function is designed to create the rangedEnemies
//* Currently it is the same as create_Rect, however, weapon updates will be added later.
//* this function takes in parameter: x, y, width, height, and color of enemy as well as weapon
//*/
//static void create_Ranged_Enemy(float x1, float y1, float w, float h, CP_Color enemyColour, CP_Color weaponColour) {
//	CP_Settings_Fill(enemyColour);
//	CP_Graphics_DrawRect(x1, y1, w, h);
//	CP_Settings_Fill(weaponColour);
//}
//
///*
//* this function is designed to create the Bullets
//* It sets the colour of the bullet, followed by drawing an ellipse with the provided values.
//* this function takes in parameter: address of Bullet
//*/
//static void bullet_Fire(Bullet* bulletGeneric) {
//	CP_Settings_Fill(black);
//	CP_Graphics_DrawEllipse(bulletGeneric->x, bulletGeneric->y, bulletGeneric->width, bulletGeneric->height);
//}
//
////function for character switch 
//static void switch_Character() {
//	currentCharacterIndex = 1 - currentCharacterIndex; //switch between 1 and 0
//}
//
////funtion to check state of jumping for long jump 
//static void start_Jump_Long(Character* player) {
//	if (!player->isJumping) { // only allows jumping if the player is not already jumping 
//		player->isJumping = true;
//		player->verticalvelocity = 800.0;
//	}
//}
//
////function to execute long jump
//static void update_Jump_Long(Character* player, float dt) {
//
//	if (player->isJumping) {
//		player->verticalvelocity -= player->gravity * dt; //applying gravity to velocity 
//		player->y -= player->verticalvelocity * dt; //updating current position based on vertical velocity
//		create_Health_Bar(&player1, &healthBar);
//		//check if player has reach the ground
//		if (player->y >= (groundlevel - player->height)) {
//			player->y = (groundlevel - player->height); // lock it to ground level 
//			player->verticalvelocity = 0.0; //reset vertical velocity 
//			player->isJumping = false; //player is no longer jumping 
//		}
//	}
//}
//
////function to check state of double jump 
//static void start_Jump_Double(Character* player) {
//	//check if player is not jumping or has a jump left for the double jumping ability 
//	if (!player->isJumping || player->jumpsLeft > 0) {
//		if (!player->isJumping) { //start from the first jump 
//			player->isJumping = true;
//			player->jumpsLeft = 1;
//		}
//		else {
//			player->jumpsLeft--;
//		}
//		player->verticalvelocity = 580.0;
//	}
//}
//
////function to execute double jump
//static void update_Jump_Double(Character* player, float dt) {
//	if (player->isJumping) {
//		player->verticalvelocity -= player->gravity * dt;
//		player->y -= player->verticalvelocity * dt;
//		//check if player laned on ground
//		if (player->y >= (groundlevel - player->height)) {
//			player->y = (groundlevel - player->height); // lock it to the ground level 
//			player->verticalvelocity = 0.0;
//			player->isJumping = false; //player is no longer jumping 
//			player->jumpsLeft = 0;//reset the jumps when landed
//		}
//	}
//}
