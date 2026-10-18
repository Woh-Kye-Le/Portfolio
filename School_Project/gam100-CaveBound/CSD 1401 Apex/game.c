/*
@file       this is file game.c
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    cavebound
@date       10/2/24
@brief      this file contains a collection of definitions of a series of functions in game.h, by including game.h in the file, and game.h in mainmenu.c,
			mainmenu.c is able to call on the functions in game.h, which processes the input according to the definitions in game.c
*/
#include "cprocessing.h"
#include "mainmenu.h"
#include "game.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include "collectables.h"


//#define groundlevel (850.0)
#define cooldown_5_seconds (5.0)
#define buffer (3.0f) //ensure collision is smoother 
#define baseGravity (1500.0)
#define playerHurtTimer (1.0f) // Timer To Display Player Hurt Image
#define max_HP (100.0)
#define PI 3.14159265358979323846f // value for pi, needed for the character switching interval cooldown
Collectable collectables[NUM_COLLECTABLES];


CP_Color blue;
CP_Color grey;
CP_Color red;
CP_Color white;
CP_Color black;
CP_Color green;
CP_Color darkRed;
CP_Color yellow;
CP_Color purple;
CP_Color healthBG;
CP_Color healthBarColor;
CP_Image player1WalkRight;
CP_Image player1WalkLeft;
CP_Image player1JumpRight;
CP_Image player1JumpLeft;
CP_Image player1HurtRight;
CP_Image player1HurtLeft;
CP_Image player2WalkRight;
CP_Image player2WalkLeft;
CP_Image player2JumpRight;
CP_Image player2JumpLeft;
CP_Image player2HurtRight;
CP_Image player2HurtLeft;
CP_Image level1Map;
CP_Image level2Map;
CP_Image monsterRangedLeft;
CP_Image genericBullet;
CP_Image monsterMeleeLeftWalk;
CP_Image monsterMeleeRightWalk;
CP_Image monsterMeleeLeftAttack;
CP_Image monsterMeleeRightAttack;
//audio
CP_Sound gemCollect;
CP_Sound jump;
CP_Sound walk;
CP_Sound game;
CP_Sound powerUp;
CP_Sound killEnemy;
CP_Sound enemyCollision;
CP_BOOL isWalking = FALSE;
CP_BOOL wasWalking = FALSE;

// Scaling factors
double scaleX, scaleY;


float dt;
float windowHeight;
float windowWidth;
float pauseTimer = 2.0f;

//base attribute of a chacter (both character will use the same base)
Character player1 = { 200.0, 800.0, 40.0, 80.0, 300.0, 0.0, 0.0, 1500.0, false, 0, 1000.0f, 1000.0f, 0, false, 0.0f, 1.0f };
Platform healthBar = { 0.0, 0.0, 100.0, 10.0 };
Boss arrayOfBoss[2]; //limit number of Boss, value can be adjusted
Bullet arrayOfBullets[100]; //limit number of Bullets,value can be adjusted
Enemies arrayOfRangedEnemies[100]; //limit number of meleeEnemy,value can be adjusted
Enemies arrayOfMeleeEnemies[100];  //limit number of rangedEnemy,value can be adjusted
int currentCharacterIndex = 0;
int numberOfBulletsPresent = 0; //initialise bullet counter
int numOfRangedEnemies = 0;  //initialise ranged Enemy counter
int numOfMeleeEnemies = 0; //initialise melee Enemy counter
int numOfEnemiesLeft = 0; //Remaining Enemies, Initilaized as 1
int level = 1; //initial Level is 1
int numOfPlatforms = 0;  //initialise platform counter
int numOfBoss = 0; //initialise Boss counter
float reactionTimer = 0.f; // to count the melee enemy time before switching directions
float meleeEnemyOriginalX[50];
float meleeEnemyOriginalY[50];
static int pointsSaved = 0;
int gameOver = 0;
int berserkMode = 0;
static int transitioningToLevel2 = 0;
int regenCounter = 1;
int regenstate = 0;
double lastCharacterSwitchTime = 0.0;
bool characterSwitched = false;
float defaultSpeed = 300.f;

//  @Game_Init:		Game_Init initializes the game, it first sets the frame rate and then clears the background, and loads the colour of the various colors initialized earlier. Here we also assign the various values 
//					for the circles and health bars.
//  @param:         Game_Init takes in a param of void.
//  @return:        Game_Init returns void
void Game_Init(void)
{
	windowWidth = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	blue = CP_Color_Create(0, 0, 255, 255);
	red = CP_Color_Create(255, 0, 0, 255);
	grey = CP_Color_Create(110, 110, 110, 20);
	white = CP_Color_Create(255, 255, 255, 255);
	black = CP_Color_Create(0, 0, 0, 255);
	green = CP_Color_Create(0, 255, 0, 255);
	darkRed = CP_Color_Create(100, 0, 0, 255);
	yellow = CP_Color_Create(255, 255, 0, 255);
	purple = CP_Color_Create(160, 32, 240, 255);
	healthBG = CP_Color_Create(132, 31, 45, 255);
	healthBarColor = CP_Color_Create(255, 0, 0, 255);
	player1WalkRight = CP_Image_Load("Assets/player1walk1.png");
	player1WalkLeft = CP_Image_Load("Assets/player1walk1LEFT.png");
	player1JumpRight = CP_Image_Load("Assets/player1jump.png");
	player1JumpLeft = CP_Image_Load("Assets/player1jumpLEFT.png");
	player1HurtRight = CP_Image_Load("Assets/player1hurt.png");
	player1HurtLeft = CP_Image_Load("Assets/player1hurtLEFT.png");
	player2WalkRight = CP_Image_Load("Assets/player2walk1.png");
	player2WalkLeft = CP_Image_Load("Assets/player2walk1LEFT.png");
	player2JumpRight = CP_Image_Load("Assets/player2jump.png");
	player2JumpLeft = CP_Image_Load("Assets/player2jumpLEFT.png");
	player2HurtRight = CP_Image_Load("Assets/player2hurt.png");
	player2HurtLeft = CP_Image_Load("Assets/player2hurtLEFT.png");
	monsterRangedLeft = CP_Image_Load("Assets/monsterwalk1.png");
	genericBullet = CP_Image_Load("Assets/greyBullet.png");
	level1Map = CP_Image_Load("./Assets/final1.png");
	level2Map = CP_Image_Load("Assets/final2.png");
	monsterMeleeLeftWalk = CP_Image_Load("Assets/skeletonLeftwalk.png");
	monsterMeleeRightWalk = CP_Image_Load("Assets/skeletonRightwalk.png");
	monsterMeleeLeftAttack = CP_Image_Load("Assets/skeletonAttackLeft.png");
	monsterMeleeRightAttack = CP_Image_Load("Assets/skeletonAttackRight1.png");
	intialising_Gems(level);
	// Reset player attributes
	player1.x = 200.0f;
	player1.y = 800.0f;
	player1.health = player1.maxHealth;

	// Reset game variables
	level = 1;
	numOfEnemiesLeft = 0;
	numOfRangedEnemies = 0;
	numOfMeleeEnemies = 0;
	numOfBoss = 0;

	loadImage();  // load images for collectibles
	initialiseCollectables(collectables);
	gemCollect = CP_Sound_Load("Assets/gemCollect.wav");
	jump = CP_Sound_Load("Assets/jump.wav");
	walk = CP_Sound_Load("Assets/walk2.wav");
	game = CP_Sound_Load("Assets/gameBG.wav");
	powerUp = CP_Sound_Load("Assets/powerUps.wav");
	killEnemy = CP_Sound_Load("Assets/killEnemy.wav");
	enemyCollision = CP_Sound_Load("Assets/collision.wav");

	CP_Sound_PlayAdvanced(game, 0.3f, 1.0f, TRUE, CP_SOUND_GROUP_MUSIC);
}

//  @Game_Update:		Main_Menu_Update updates the game, it first obtains the height and width of the window and from there, clears the background. Next it sets the DT to be multiplied by to the speed to obtain the velocity
//						of the player circle. Next we initialized the var representing the number of smaller health bars, as well as the square comparison vars for between the player circle and the red circle, followed by that
//						of the green circle. Next we put in a list of if statements to handle the moving of the player circle via keydown. This is followed by the various checks to see if the player circle has entered
//						the red or green circle, and from there we can reduce the big health bar. this is followed by a for loop to set the number of small health bars according to the remaining width of the big health bar.
//						Lastly we simply draw out the various shapes, and for the small health bars, we loop it through a for loop to display as many as stated in the numOfSmallHealthBars var.
//						
//  @param:				Game_Update takes in a param of void.
//  @return:			Game_Update returns void

void Game_Update(void)
{
	CP_Graphics_ClearBackground(grey);
	dt = CP_System_GetDt();
	Platform platforms[100]; //limit number of platforms,value can be adjusted
	//numOfPlatforms = initialise_Platform(platforms); //set numPlatforms to be total number of platforms created, 
	//apply gravity 
	if (player1.isHurt == true) {
		player1.hurtTimer += dt;
		// Exit the hurt state after the duration expires
		if (player1.hurtTimer >= player1.hurtDuration) {
			player1.isHurt = false;
			player1.hurtTimer = 0.0f;
		}
	}
	apply_Gravity(&numOfMeleeEnemies, &numOfRangedEnemies, &player1, arrayOfMeleeEnemies, arrayOfRangedEnemies, dt);
	//declare a static variable lastswitchtime and initialised it to 0,it retains the last time a character 
	//switch occured across the frames 
	static double lastSwitchtime = 0.0;
	//current_time keeps track of the current time in seconds
	double current_time = CP_System_GetSeconds();
	double initialReactTime = 0.0;
	srand((unsigned int)time(NULL));
	//skip 1st level for testing purposes ONLY
	if (CP_Input_KeyDown(KEY_I))
	{
		level = 2;
		numOfEnemiesLeft = 0;
		transitioningToLevel2 = 1;  // Set transitioning flag
		intialising_GemsLvl2();
		initialiseCollectablesLvl2(collectables);
		player1.x = 200.0;
		player1.y = 800.0;
		arrayOfBoss->x = 700.0f;
		arrayOfBoss->y = 100.0f;
		arrayOfBoss->health = 1000.0f;
		arrayOfBoss->isAlive = true;
		arrayOfBoss->state = 0;
		//printf("Transitioning to level 2.\n");
	}
	checkLevel(level, &numOfBoss, &numOfEnemiesLeft, &numOfRangedEnemies, &numOfMeleeEnemies, &numOfPlatforms, arrayOfBoss, platforms, arrayOfRangedEnemies, arrayOfMeleeEnemies, &player1);
	check_Boundary(&numOfMeleeEnemies, &numOfRangedEnemies, &player1, arrayOfMeleeEnemies, arrayOfRangedEnemies);
	if (player1.health <= 1)
	{
		if (!pointsSaved) {
			//printf("Game Over. Saving player points.\n");
			savePlayerPoints(playerName, playerPoints);
			//printf("Final Points: %d\n", playerPoints);
			pointsSaved = 1;  // Prevent multiple saves
		}
		ShowGameOverScreen();
		return;
	}
	if (0 == numOfEnemiesLeft && 1 == level)
	{
		if (!transitioningToLevel2) {
			level = 2;
			transitioningToLevel2 = 1;  // Set transitioning flag
			intialising_GemsLvl2();
			initialiseCollectablesLvl2(collectables);
			player1.x = 200.0;
			player1.y = 800.0;
			arrayOfBoss->x = 700.0f;
			arrayOfBoss->y = 100.0f;
			arrayOfBoss->health = 1000.0f;
			arrayOfBoss->isAlive = true;
			arrayOfBoss->state = 0;
			//printf("Transitioning to level 2.\n");
		}
	}

	if ((1 == currentCharacterIndex) && player1.health < 250 && player1.health <= 500 && 1 == regenCounter)
	{
		regenstate = 1;
		regenCounter = 0;
	}
	else if ((0 == currentCharacterIndex) || player1.health > 500)
	{
		regenstate = 0;
	}
	if (1 == regenstate)
	{
		player1.health = player1.health + 200.0f * dt;
		//printf("Player Health is: %lf\n", player1.health);
	}

	if (CP_Input_KeyDown(KEY_ESCAPE))
	{

		CP_Sound_StopAll();

		CP_Sound_Free(&gemCollect);
		CP_Sound_Free(&jump);
		CP_Sound_Free(&walk);
		CP_Sound_Free(&game);
		CP_Sound_Free(&powerUp);
		CP_Sound_Free(&killEnemy);

		if (!pointsSaved) {
			//printf("Leaving Game, saving player points.\n");
			savePlayerPoints(playerName, playerPoints);
			//printf("Final Points: %d\n", playerPoints);
			pointsSaved = 1;  // Prevent multiple saves
		}
		promptname_Visibility = 0;
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
	}
	if (level == 2 && numOfEnemiesLeft > 0 && transitioningToLevel2) {
		transitioningToLevel2 = 0;
		//printf("Level 2 initialized, transitioning complete.\n");
	}

	// Handle level 2 completion
	if (0 == numOfEnemiesLeft && 2 == level && !transitioningToLevel2) {
		if (!pointsSaved) {
			//printf("Level 2 complete, saving player points.\n");
			savePlayerPoints(playerName, playerPoints);
			//printf("Final Points: %d\n", playerPoints);
			pointsSaved = 1;  // Prevent multiple saves
		}
		promptname_Visibility = 0;
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
	}
	/*---------------------------------Game_Update---------------------------------*/

	detect_Player_Boss(numOfBoss, &player1, arrayOfBoss, &dt); //call function detect_Player_Boss
	//Switch between long jump and short jump only when theres no jumping action 
	//interval of 5 secs: time elapse from the last character switch is calculated from (current_time - lastSwitchtime)
	//                    it checks if it is greater of equal to the cooldown to prevent 
	if (!player1.isJumping && CP_Input_KeyReleased(KEY_Q) && (current_time - lastSwitchtime >= cooldown_5_seconds)) {
		switch_Character(&currentCharacterIndex);
		//if character is switched, the lastswitchtime will be set to the currenttime, to ensure the interval of 5 secs has been reset
		lastSwitchtime = current_time;
		lastCharacterSwitchTime = CP_System_GetSeconds();
		characterSwitched = true;
	}
	//printf("current_time: %f\n", current_time);
	//printf("lastCharacterSwitchTime: %f\n", lastCharacterSwitchTime);

	//Loop Through Number Of Ranged Enemies
	check_Counter_And_Fire_Bullets(numOfRangedEnemies, &numberOfBulletsPresent, &player1, arrayOfRangedEnemies, arrayOfBullets);
	detect_Player_Melee(numOfMeleeEnemies, currentCharacterIndex, &pauseTimer, &dt, &reactionTimer, meleeEnemyOriginalX, meleeEnemyOriginalY, &player1, arrayOfMeleeEnemies, monsterMeleeLeftAttack, monsterMeleeRightAttack, player1HurtRight, player1HurtLeft, player2HurtRight, player2HurtLeft, monsterMeleeLeftWalk, monsterMeleeRightWalk);
	bullet_Collision_And_Damage(numOfPlatforms, currentCharacterIndex, &numberOfBulletsPresent, &player1, platforms, arrayOfBullets, genericBullet);
	//Loop Through Number Of Bullets
	// If Player health is not zero, set the health bar based on the percentage of the remaining hp
	if (player1.health > 0)
	{
		healthBar.width = health_Bar_Calculate(player1.health, player1.maxHealth, player1.width);// health bar = player width * percentage of remaining player hp
		healthBar.x = player1.x;
	}
	// else if player has reached 0 health, stop reduction and end game
	else
	{
		healthBar.width = 0.1f;
		healthBar.x = player1.x;
	}
	//call upon creat health bar fn to create health bar for player, passing in player struct and health bar struct;
	healthBar.y = player1.y * 0.95f;
	create_Rect(healthBar.x, healthBar.y, player1.width, healthBar.height, CP_Color_Create(100, 0, 0, 255)); // For Player to visualize max hp bar
	create_Rect(healthBar.x, healthBar.y, healthBar.width, healthBar.height, CP_Color_Create(0, 255, 0, 255)); // For Current Player Max Hp

	//create a visual timer for player character switch cool down 
	//coordinate for center point, 5 vertices for 5 equal triangle that make up a pentagon and length of triangle 
	float triangleLength = 11.0f;
	float CP_x = healthBar.x + player1.width + 21.0f;
	float CP_y = healthBar.y + (healthBar.height / 2);
	float vertices[5][2];
	int activeTriangles = 5;

	//initialise vertices
	for (int i = 0; i < 5; i++) {
		float angle = 2.f * PI * (i / 5.0f);
		vertices[i][0] = CP_x + triangleLength * (float)cos(angle);
		vertices[i][1] = CP_y + triangleLength * (float)sin(angle);
	}
	//printf("before function, %d\n", characterSwitched);
	updateCooldownTimer(&activeTriangles, CP_x, CP_y, vertices, green, &lastCharacterSwitchTime, &characterSwitched);
	//printf("after function, %d\n", characterSwitched);


	if (player1.isSpeed) {
		player1.speedTimer -= dt;
		if (player1.speedTimer <= 0.0f) {
			player1.speed_x = defaultSpeed; //reset to default speed
			player1.isSpeed = false;         //end boost
		}
	}

	//left and right movement for character 
	if (CP_Input_KeyDown(KEY_D)) {
		player1.x += player1.speed_x * dt;
		player1.direction = 0;
		//audio walk
	}
	if (CP_Input_KeyDown(KEY_A)) {
		player1.x -= player1.speed_x * dt;
		player1.direction = 1;
		//audio walk
	}

	//character with long jump
	if (currentCharacterIndex == 0) {
		if (player1.isJumping && 0 == player1.direction)
		{
			CP_Image_Draw(player1JumpRight, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (player1.isJumping && 1 == player1.direction)
		{
			CP_Image_Draw(player1JumpLeft, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (0 == player1.direction && (!player1.isHurt))
		{
			CP_Image_Draw(player1WalkRight, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (1 == player1.direction && (!player1.isHurt))
		{
			CP_Image_Draw(player1WalkLeft, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (0 == player1.direction && (player1.isHurt))
		{
			CP_Image_Draw(player1HurtRight, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (1 == player1.direction && (player1.isHurt))
		{
			CP_Image_Draw(player1HurtLeft, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		if (CP_Input_KeyTriggered(KEY_SPACE) && !player1.isJumping) {
			CP_Sound_PlayAdvanced(jump, 0.1f, 1.0f, FALSE, CP_SOUND_GROUP_2);
			start_Jump_High(&player1);
		}
		update_Jump_Long(&player1, dt);
	}

	//character with double jump
	if (currentCharacterIndex == 1) {
		if (player1.isJumping && 0 == player1.direction)
		{
			CP_Image_Draw(player2JumpRight, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (player1.isJumping && 1 == player1.direction)
		{
			CP_Image_Draw(player2JumpLeft, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (0 == player1.direction && (!player1.isHurt))
		{
			CP_Image_Draw(player2WalkRight, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (1 == player1.direction && (!player1.isHurt))
		{
			CP_Image_Draw(player2WalkLeft, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (0 == player1.direction && (player1.isHurt))
		{
			CP_Image_Draw(player2HurtRight, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		else if (1 == player1.direction && (player1.isHurt))
		{
			CP_Image_Draw(player2HurtLeft, player1.x + player1.width / 2, player1.y + player1.height / 2, player1.width, player1.height, 255);
		}
		if (CP_Input_KeyTriggered(KEY_SPACE)) {
			CP_Sound_PlayAdvanced(jump, 0.1f, 1.0f, FALSE, CP_SOUND_GROUP_SFX);
			start_Jump_Double(&player1);
		}
		update_Jump_Double(&player1, dt);
	}

	//points
	drawPoints(playerPoints); //draw points of player
	draw_Gems(level);

	for (int i = 0; i < MAX_GEMS; i++) {
		if (checkCollisionWithGem(&player1, &gems[i], &playerPoints)) {
			CP_Sound_Play(gemCollect);
			//printf("gem %d collected! Player Points: %d\n", i, playerPoints);
		}
	}

	//collectables
	drawCollectables(collectables, NUM_COLLECTABLES);

	bool collected = checkCollisionWithCollectables(&player1, collectables, NUM_COLLECTABLES);
	if (collected) {
		//printf("Collectible collected!\n");
		CP_Sound_Play(powerUp);
	}
}
//  @Game_Exit:		Game_Exit handles the exiting of the game state, as nothing is needed to be processed here as of yet, it is empty.
//  @param:         Game_Exit takes in a param of void.
//  @return:        Game_Exit returns void
void Game_Exit(void)
{
	CP_Sound_Free(&gemCollect);
	CP_Sound_Free(&jump);
	CP_Sound_Free(&walk);
	CP_Sound_Free(&game);
	CP_Sound_Free(&powerUp);
	CP_Sound_Free(&killEnemy);
}
/*
* Done By: Kye Le and Jethro
* This basically checks if the player or enemies have reached the limit of the screen, and prevents them from going out of bounds. For the player, we simply set them back to the boundary limit.
* For melee enemies, we simply flip the enemy state if the enemy is out of the x limit. For y, we reset the enemy y back to the limit. For ranged enemies, it is similar to the player.
*/
void check_Boundary(int* numberOfMeleeEnemies, int* numberOfRangedEnemies, Character* player, Enemies arrayOfMeleeEnemies[], Enemies arrayOfRangedEnemies[]) {
	if (player->x <= 0.0) { //hitting the left limit of windows
		player->x = 0.0;
	}
	if (player->x >= windowWidth - player->width) {
		player->x = windowWidth - player->width;
	}
	if (player->y <= 0.0) {
		player->y = 0.0;
	}
	for (int i = 0; i < *numberOfMeleeEnemies; i++)
	{
		if (arrayOfMeleeEnemies[i].x <= 0.0) { //hitting the left limit of windows
			arrayOfMeleeEnemies[i].x = 0.0;
			arrayOfMeleeEnemies[i].state = 1 - arrayOfMeleeEnemies[i].state;
		}
		if (arrayOfMeleeEnemies[i].x >= windowWidth - arrayOfMeleeEnemies[i].width) {
			arrayOfMeleeEnemies[i].x = windowWidth - arrayOfMeleeEnemies[i].width;
			arrayOfMeleeEnemies[i].state = 1 - arrayOfMeleeEnemies[i].state;
		}
		if (arrayOfMeleeEnemies[i].y <= 0.0) {
			arrayOfMeleeEnemies[i].y = 0.0;
		}
	}
	for (int i = 0; i < *numberOfRangedEnemies; i++)
	{
		if (arrayOfRangedEnemies[i].x <= 0.0) { //hitting the left limit of windows
			arrayOfRangedEnemies[i].x = 0.0;
		}
		if (arrayOfRangedEnemies[i].x >= windowWidth - arrayOfRangedEnemies[i].width) {
			arrayOfRangedEnemies[i].x = windowWidth - arrayOfRangedEnemies[i].width;
		}
		if (arrayOfRangedEnemies[i].y <= 0.0) {
			arrayOfRangedEnemies[i].y = 0.0;
		}
	}
}

/*
* Done By: Jethro and Kye Le(For Boss Related)
* this function is designed to check the current level of the game that the player is in, and generate the set amount of enemies for the level
* First, we create an array of ints. This is for the melee enemies collision with platforms. This is because there are multiple melee enemies, and there are potential cases where the melee enemies may collide with 2-4 platforms at once.
* Hence, this array  is initialized so that we can handle all 2-4 platform collisions for each enemy, unlike ranged which usually only handles a single platform collision, and player which only has a single spawn.
* Next, we initialize a state float variable for player damage. This tracks the damage inflicted by the player to the boss, hence determining when the boss would switch states.
* Level 1 (First Initialization)
* With this, we first check if the current level is 1, if so, we draw the map for level 1. Next, we check if the enemy has been initialized by checking if the number of remaining melee, ranged or boss is at 0. 
* If it hasn't we initialized the initial coordinates and values of the respective enemies and boss.Next we initialize the platforms.(For Platforms, it happens regardless of first initialization.
* Keep in mind, for melee enemies, we also set their original x and y values into two arrays respectively to store the original x and y of the melee enemies. The use for this can be seen in detect player melee.
* Lastly, we set the numberOfRemaining Enemies to 0, because this pointer to the number of remaining enemies left will be incremented when we loop through the melee enemies, ranged enemies, and boss. This is so that the initial initilization
* would not happen again. 
* Level 1 (Default Loop)
* Next, this is the default loop, regardless if the First init happens or not. Firstly, we loop through the platforms, and check for collision handling with player. Next, we loop through the array of ranged enemies.
* Keep note that if the structure of any of the index within the array has it's var isAlive as 0, we will use continue to interate to the next index to prevent it from spawning. With this, we check for collisions with the player and platforms,
* and call the respective functions to handle each case. Do note that platforms to handle array is memset after every enemy to be able to handle the platforms for each enemy with a reset array.
* A similar loop occurs for handling the melee enemies and boss. However regarding the boss, it also has two different states to switch to, depending on the playerDamage inflicted.
* Furthermore, we also need to draw the health bar of the boss, unlike the normal enemies which die in a single hit. The final result of the collision as well is different, with the boss teleporting the player randomly, unlike the enemy which will
* die in one hit. For all 3 enemy types, once their health reaches 0, their isalive is set to 0.
* Level 2 (First Initialization)
* This is essentially the same logic as level 1. Except that we are calling the level 2 functions for the initial initialization as well as the drawing of the map.
* Level 2 (Default Loop)
* Similar to level 1, however boss states are different in terms of handling.
* this function takes in parameter: currentLevel, address to the number of boss, address of number of ranged enemies, address of number of melee enemies, address of numberOfPlatforms, array of boss structs, array of platform structs,
*									array of ranged Enemies structs, array of melee enemy structs and the struct for player1.
*/
void checkLevel(int currentLevel, int* numberOfBoss, int* numberOfRemainingEnemies, int* numberOfRangedEnemies, int* numberOfMeleeEnemies, int* numberOfPlatforms, Boss arrayOfBoss[], Platform arrayOfPlatforms[], Enemies arrayOfRangedEnemies[], Enemies arrayOfMeleeEnemies[], Character* player)
{
	int platformsToHandleArray[20] = {0};
	static float playerDamage = 0.0f;
	if (1 == currentLevel)
	{
		CP_Image_Draw(level1Map, windowWidth / 2, windowHeight / 2, windowWidth, windowHeight, 255);

		if (*numberOfRemainingEnemies == 0) //check if ranged enemies have been set
		{
			*numberOfRangedEnemies = initialise_Level1_Enemies_Ranged(arrayOfRangedEnemies);  //set numberOfRangedEnemies to be starting number of ranged enemies created, 
		}
		if (*numberOfRemainingEnemies == 0) //check if melee enemies have been set
		{
			*numberOfMeleeEnemies = initialise_Level1_Enemies_Melee(arrayOfMeleeEnemies);  //set numberOfMeleeEnemies to be starting number of melee enemies created, 
			for (int i = 0; i < *numberOfMeleeEnemies; i++)
			{
				meleeEnemyOriginalX[i] = arrayOfMeleeEnemies[i].x;
				meleeEnemyOriginalY[i] = arrayOfMeleeEnemies[i].y;
			}
		}
		if (*numberOfBoss == 0) { //check if Boss has been set
			*numberOfBoss = initialise_Level1_Boss(arrayOfBoss);  //set numberOfBoss to be starting number of Boss created
		}
		*numberOfRemainingEnemies = 0;
		*numberOfPlatforms = initialise_Platform(arrayOfPlatforms); //set numPlatforms to be total number of platforms created, 

		//create the platforms using a for loop that iterates over the function initialise_Platform 
		for (int i = 0; i < *numberOfPlatforms; i++)
		{
			if (check_Collision_Player_Platform(&player1, &arrayOfPlatforms[i])) {
				handle_Collision_Player_Platform(&player1, &arrayOfPlatforms[i]);
			}
		}
		//create the rangedEnemies using a for loop that iterates over the function create_Ranged_Enemy 
		for (int i = 0; i < *numberOfRangedEnemies; i++)
		{
			if (0 == arrayOfRangedEnemies[i].isAlive)
			{
				continue;
			}
			create_Ranged_Enemy(arrayOfRangedEnemies[i].x + arrayOfRangedEnemies[i].width / 2, arrayOfRangedEnemies[i].y + arrayOfRangedEnemies[i].height / 2, arrayOfRangedEnemies[i].width, arrayOfRangedEnemies[i].height, red, black, monsterRangedLeft);
			//platformIndexCollision = check_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);
			check_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], *numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);
			if ((-1 == platformsToHandleArray[0]) || (0 < platformsToHandleArray[0])) { //checking and handling collision between ranged enemy and platform 
				handle_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], arrayOfPlatforms,platformsToHandleArray);
			}
			if (check_Collision_Player_Enemies(&player1, &arrayOfRangedEnemies[i])) {
				handle_Collision_Player_Enemies(&player1, &arrayOfRangedEnemies[i], &dt);
			}
			memset(platformsToHandleArray, 0, sizeof(platformsToHandleArray));
			*numberOfRemainingEnemies += 1;
		}
		//create the meleeEnemies using a for loop that iterates over the function create_Melee_Enemy 
		for (int i = 0; i < *numberOfMeleeEnemies; i++)
		{
			if (arrayOfMeleeEnemies[i].y > 850.f)
			{
				arrayOfMeleeEnemies[i].isAlive = 0;
			}
			if (0 == arrayOfMeleeEnemies[i].isAlive)
			{
				continue;
			}
			//platformIndexCollision = check_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], numberOfPlatforms, arrayOfPlatforms);
			//if (platformIndexCollision) { //checking and handling collision between melee enemy and platform 
			//	handle_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], &arrayOfPlatforms[platformIndexCollision - 1]);
			//}
			check_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], *numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);
			if ((-1 == platformsToHandleArray[0]) || (0 < platformsToHandleArray[0])) { //checking and handling collision between ranged enemy and platform 
				handle_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], arrayOfPlatforms,platformsToHandleArray);
			}
			if (check_Collision_Player_Enemies(&player1, &arrayOfMeleeEnemies[i])) {
				handle_Collision_Player_Enemies(&player1, &arrayOfMeleeEnemies[i], &dt);

			}
			memset(platformsToHandleArray, 0, sizeof(platformsToHandleArray));
			*numberOfRemainingEnemies += 1;
		}
		for (int i = 0; i < *numberOfBoss; i++) {
			if (!arrayOfBoss[i].isAlive)
			{
				continue;
			}
			if (arrayOfBoss[i].health < 200.0f)
			{
				berserkMode = 1;
			}
			drawBossHealthBar(&arrayOfBoss[i]);
			if (arrayOfBoss[i].health <= 0.0f) { // temporary for testing purposes
				arrayOfBoss[i].health = 0.0f;
			}

			if (check_Collision_Player_Boss(&player1, &arrayOfBoss[i])) {
				handle_Collision_Player_Boss(&player1, &arrayOfBoss[i], &dt);
			}
			else
			{
				if (!player1.isSpeed) {
					player1.speed_x = defaultSpeed;
				}
			}

			if (arrayOfBoss[i].state == 0) { //patrolling state 
				//printf("purple:patrolling\n");
				//printf("damage: %f\n", playerDamage);
				create_Boss(arrayOfBoss[i].x, arrayOfBoss[i].y, arrayOfBoss[i].width, arrayOfBoss[i].height, purple, black);
				detect_Player_Boss(numOfBoss, &player1, &arrayOfBoss[i], &dt); //call function detect_Player_Boss and attacks the player using melee
				if (arrayOfBoss[i].x <= 485.0f) { // makes sure the boss stays 40 unit off from the left of the platform 
					arrayOfBoss[i].x = 485.0f;
				}
				if (arrayOfBoss[i].x >= 855.0f - arrayOfBoss[i].width) { //makes sure the boss stays 40 unit off from the right of the platform
					arrayOfBoss[i].x = 855.0f - arrayOfBoss[i].width;
				}

				if (currentCharacterIndex == 0 && check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i])) { //check if its player 1 that colliding with the boss from the top 
					arrayOfBoss[i].health -= 150.0f; //if so, only player 1 can damage boss 
					playerDamage += 150.0f;
				}
				if (check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i])) {
					int playerState = (rand() % 6) + 1;
					//printf("player state: %d\n", playerState);
					player_Location_Set(&playerState, &player1, &level);
				}
				if (playerDamage >= 600.0f) { //condition to switch state if player has done enough damage 
					switch_Boss_state(&arrayOfBoss[i]);
					playerDamage = 0.0f;
				}
			}

			else if (arrayOfBoss[i].state == 1) { //attacking state 
				//printf("red:attacking\n");
				create_Boss(arrayOfBoss[i].x, arrayOfBoss[i].y, arrayOfBoss[i].width, arrayOfBoss[i].height, red, black);
				check_Counter_And_Fire_Bullets_Boss(&numberOfBulletsPresent, &player1, &arrayOfBoss[i], arrayOfBullets, &berserkMode); //range attack
				bullet_Collision_And_Damage_Boss(&numberOfBulletsPresent, &dt, &player1, arrayOfBullets, genericBullet); //range attack 
				arrayOfBoss[i].health += 10.0f * dt;
				if (arrayOfBoss[i].health >= 1000.0f) {
					arrayOfBoss[i].health = 1000.0f;
				}
				if (currentCharacterIndex == 1 && check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i])) { //check if its player 2 that colliding with the boss from the top 
					arrayOfBoss[i].health -= 150.0f; //if so, only player 2 can damage boss 
				}
				if (check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i])) {
					int playerState = (rand() % 6) + 1;
					//printf("player state: %d\n", playerState);
					player_Location_Set(&playerState, &player1, &level);
				}
				if (arrayOfBoss[i].health >= 750.0f) {
					switch_Boss_state(&arrayOfBoss[i]);
				}
			}
			if (arrayOfBoss[i].health <= 0.0f) {
				arrayOfBoss[i].isAlive = false;
				arrayOfBoss[i].health = 0.0f;
			}
			*numberOfRemainingEnemies += 1;
		}
	}
	else if (2 == currentLevel)
	{
		CP_Image_Draw(level2Map, windowWidth / 2, windowHeight / 2, windowWidth, windowHeight, 255);

		if (*numberOfRemainingEnemies == 0) //check if ranged enemies have been set
		{
			*numberOfRangedEnemies = initialise_Level2_Enemies_Ranged(arrayOfRangedEnemies);  //set numberOfRangedEnemies to be starting number of ranged enemies created, 
		}
		if (*numberOfRemainingEnemies == 0) //check if melee enemies have been set
		{
			*numberOfMeleeEnemies = initialise_Level2_Enemies_Melee(arrayOfMeleeEnemies);  //set numberOfMeleeEnemies to be starting number of melee enemies created, 
		}
		if (*numberOfBoss == 0) { //check if Boss has been set
			*numberOfBoss = initialise_Level1_Boss(arrayOfBoss);  //set numberOfBoss to be starting number of Boss created
		}
		*numberOfRemainingEnemies = 0;
		*numberOfPlatforms = initialise_Platform_Level2(arrayOfPlatforms); //set numPlatforms to be total number of platforms created, 

		//create the platforms using a for loop that iterates over the function initialise_Platform 
		for (int i = 0; i < *numberOfPlatforms; i++)
		{
			if (check_Collision_Player_Platform(&player1, &arrayOfPlatforms[i])) {
				handle_Collision_Player_Platform(&player1, &arrayOfPlatforms[i]);
			}
		}
		//create the rangedEnemies using a for loop that iterates over the function create_Ranged_Enemy 
		for (int i = 0; i < *numberOfRangedEnemies; i++)
		{
			if (0 == arrayOfRangedEnemies[i].isAlive)
			{
				continue;
			}
			create_Ranged_Enemy(arrayOfRangedEnemies[i].x + arrayOfRangedEnemies[i].width / 2, arrayOfRangedEnemies[i].y + arrayOfRangedEnemies[i].height / 2, arrayOfRangedEnemies[i].width, arrayOfRangedEnemies[i].height, red, black, monsterRangedLeft);
			if (check_Collision_Player_Enemies(&player1, &arrayOfRangedEnemies[i])) {
				handle_Collision_Player_Enemies(&player1, &arrayOfRangedEnemies[i], &dt);

			}
			check_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], *numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);
			if ((-1 == platformsToHandleArray[0]) || (0 < platformsToHandleArray[0])) { //checking and handling collision between ranged enemy and platform 
				handle_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], arrayOfPlatforms, platformsToHandleArray);
			}
			//check_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);

			//if (platformsToHandleArray[0]) { //checking and handling collision between ranged enemy and platform 
			//	handle_Collision_Enemies_Platform(&arrayOfRangedEnemies[i], platformsToHandleArray);
			//}
			memset(platformsToHandleArray, 0, sizeof(platformsToHandleArray));
			*numberOfRemainingEnemies += 1;
		}
		//create the meleeEnemies using a for loop that iterates over the function create_Melee_Enemy 
		for (int i = 0; i < *numberOfMeleeEnemies; i++)
		{
			if (arrayOfMeleeEnemies[i].y > 850.f)
			{
				arrayOfMeleeEnemies[i].isAlive = 0;
			}

			if (0 == arrayOfMeleeEnemies[i].isAlive)
			{
				continue;
			}

			if (check_Collision_Player_Enemies(&player1, &arrayOfMeleeEnemies[i])) {
				handle_Collision_Player_Enemies(&player1, &arrayOfMeleeEnemies[i], &dt);

			}
			//platformIndexCollision = check_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], numberOfPlatforms, arrayOfPlatforms);
			//if (platformIndexCollision) { //checking and handling collision between melee enemy and platform 
			//	handle_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], &arrayOfPlatforms[platformIndexCollision - 1]);
			//}
			check_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], *numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);
			if ((-1 == platformsToHandleArray[0]) || (0 < platformsToHandleArray[0])) { //checking and handling collision between ranged enemy and platform 
				handle_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], arrayOfPlatforms, platformsToHandleArray);
			}
			//check_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], numberOfPlatforms, arrayOfPlatforms, platformsToHandleArray);

			//if (platformsToHandleArray[0]) { //checking and handling collision between ranged enemy and platform 
			//	handle_Collision_Enemies_Platform(&arrayOfMeleeEnemies[i], platformsToHandleArray);
			//}
			memset(platformsToHandleArray, 0, sizeof(platformsToHandleArray));
			*numberOfRemainingEnemies += 1;
		}
		for (int i = 0; i < *numberOfBoss; i++) {
			if (!arrayOfBoss[i].isAlive)
			{
				continue;
			}
			if (arrayOfBoss[i].health < 200.0f)
			{
				berserkMode = 1;
			}
			//create_Boss(arrayOfBoss[i].x, arrayOfBoss[i].y, arrayOfBoss[i].width, arrayOfBoss[i].height, purple, black);
			drawBossHealthBar(&arrayOfBoss[i]);

			if (check_Collision_Player_Boss(&player1, &arrayOfBoss[i])) {
				handle_Collision_Player_Boss(&player1, &arrayOfBoss[i], &dt);
			}

			if (arrayOfBoss[i].state == 0) { //patrolling state
				create_Boss(arrayOfBoss[i].x, arrayOfBoss[i].y, arrayOfBoss[i].width, arrayOfBoss[i].height, purple, black);
				check_Counter_And_Fire_Bullets_Boss(&numberOfBulletsPresent, &player1, &arrayOfBoss[i], arrayOfBullets, &berserkMode); //range attack
				bullet_Collision_And_Damage_Boss(&numberOfBulletsPresent, &dt, &player1, arrayOfBullets, genericBullet); //range attack 
				if (check_Collision_Player_Boss_Bottom(&player1, &arrayOfBoss[i]) && currentCharacterIndex == 0) {
					arrayOfBoss[i].health -= 30.0f; //reduced damaged, assuming normal damage is 50 units
				}
				if (check_Collision_Player_Boss_Bottom(&player1, &arrayOfBoss[i])) {
					int playerState = (rand() % 6) + 1;
					//printf("player state: %d\n", playerState);
					player_Location_Set(&playerState, &player1, &level);
				}
				if (check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i])) {
					arrayOfBoss[i].health -= 150.0f;
					int playerState = (rand() % 6) + 1;
					//printf("player state: %d\n", playerState);
					player_Location_Set(&playerState, &player1, &level);
				}
				if (arrayOfBoss[i].health <= 750.0f) {
					switch_Boss_state(&arrayOfBoss[i]);
				}
			}

			else if (arrayOfBoss[i].state == 1) { //attacking state 
				create_Boss(arrayOfBoss[i].x, arrayOfBoss[i].y, arrayOfBoss[i].width, arrayOfBoss[i].height, red, black);
				arrayOfBoss[i].y = 262.0f - arrayOfBoss[i].height;
				arrayOfBoss[i].x = 100.0f;
				detect_Player_Boss(numOfBoss, &player1, &arrayOfBoss[i], &dt); //call function detect_Player_Boss and attacks the player using melee
				if (arrayOfBoss[i].x >= 215.0f - arrayOfBoss[i].width) { //makes sure the boss stays 40 unit off from the right of the platform
					arrayOfBoss[i].x = 215.0f - arrayOfBoss[i].width;
				}
				if (check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i]) && currentCharacterIndex == 1) {
					arrayOfBoss[i].health -= 150.0f;
				}
				if (check_Collision_Player_Boss_Top(&player1, &arrayOfBoss[i])) {
					int playerState = (rand() % 6) + 1;
					//printf("player state: %d\n", playerState);
					player_Location_Set(&playerState, &player1, &level);
				}
			}
			if (arrayOfBoss[i].health <= 0.0f) {
				arrayOfBoss[i].isAlive = false;
				arrayOfBoss[i].health = 0.0f;
			}
			*numberOfRemainingEnemies += 1;
		}
	}
}

/*
* Done By: Kye Le, Jethro(For Enemies)
* this function is designed to apply gravitational effect to the player at any point of time,
* !player->isjumping: even if the player is not technically in a jump, gravity may need to be applied
* player->y < (groundlevel - player->height): This ensures gravity is only applied if the player is above the ground level.
* Applying gravity to vetical velocity: player->verticalvelocity -= player->gravity * dt, this effectively increases the downward
* velocity each frame, simulating the acceleration due to gravity.
* Afterwards, it will update the player's vertical position through player->y -= player->verticalvelocity * dt.
* it will then check if the player has made contact with the ground, if so the player's position will be on the ground and
* the vertical velocity will be set to 0
* this function takes in parameter: address of player, dt
*/
void apply_Gravity(int const* numberOfMeleeEnemies, int const* numberOfRangedEnemies, Character* player, Enemies arrayOfMeleeEnemies[], Enemies arrayOfRangedEnemies[], float dt) {
	if (!player->isJumping || player->y < (groundlevel - player->height)) {
		player->verticalvelocity -= player->gravity * dt;
		player->y -= player->verticalvelocity * dt;

		// Check if the player has reached the ground
		if (player->y >= (groundlevel - player->height)) {
			player->y = (groundlevel - player->height);
			player->verticalvelocity = 0.0;
			player->isJumping = false;
		}
	}
	for (int i = 0; i < *numberOfMeleeEnemies; i++)
	{
		arrayOfMeleeEnemies[i].verticalvelocity -= player->gravity / 2 * dt;
		arrayOfMeleeEnemies[i].y -= arrayOfMeleeEnemies[i].verticalvelocity * dt;
	}
	for (int i = 0; i < *numberOfRangedEnemies; i++)
	{
		arrayOfRangedEnemies[i].verticalvelocity -= player->gravity / 2 * dt;
		arrayOfRangedEnemies[i].y -= arrayOfRangedEnemies[i].verticalvelocity * dt;
	}
}
/*
* Done By: Lumi
*/
void SetGameState(GameState state)
{

}