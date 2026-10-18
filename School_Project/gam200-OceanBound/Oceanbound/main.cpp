
// ---------------------------------------------------------------------------
// includes

#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "pch.h"
#include "player.h"
#include "objects.hpp"
#include "platforms.h"
#include "Room.hpp"
#include "water.h"
#include "levers.h"
#include "health.h"
#include "specialAbilities.h"
#include <math.h>
#include "utils.hpp"
#include "GameStateManager.h"
#include <cmath>
#include "gameover.h"
#include "mainmenu.h"
#include "ropes.h"
#include "leaderboard.h"
#include "leaderboardLog.h"
#include "gameFin.h"

// ---------------------------------------------------------------------------
// main
extern const float groundLevel = (-450.0f);
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	/* Initialisation (outside game loop) */
	/****************************************************************************************************************************/
	int gGameRunning = 1;
	//int isWater = 0;


	const int numLevels = 10; // Set to max of 10 levels, In future updates to be set to 10
	const int numLevers = 20; //Every Level max 20 levers as of now
	const int numRopes = 25; //Max 20 ropes as of now
	double* time_in_sec = 0;
	f32 gameTime = 0.0f;
	f32 leverCooldown = 0.f;
	f32 ropeCooldown = 0.f;
	int activeHotkey = 0;  // Tracks the active hotkey after RETURN is pressed
	static Platform platforms[25];
	char timerTxt[32];
	int minutes = 0;
	int seconds = 0;
	bool leaderboardpopup = false;
	bool showPlayerPopup = false;
	bool hasLeaderboardBeenLogged = false;
	bool reinit = false;

	//int initialPlayerRoom = 0, initialPlayerLevel = 0; //This is needed to track the current room and level of the player (Defunct, integrated as part of player struct.)
	Player player = { 50.0f, 100.0f , -700.0f, -400.0f , 0.0f, 0.0f, 0.0f, 400.0f, 800.0f, 0.0f, 3000.0f, false, false, false, 1000.0f , 100.0f, GS_LEVEL1, 0, 1000.f,1000.f ,- 1 }; //current jump height for player is 155.83 units;
	player.onGround = true;
	static Lever levers[numLevers]; // Arr of levers, determined by const int of numlevers
	square oxygen_level = { 50.0f, 12.0f, player.posX, player.posY + player.scaleY / 2 + 20.0f, 0.0f, 100.0f, 5.0f, 0.0f, 50.0f, player.posX };
	HealthBar healthbar = { player.posX, player.posY + player.scaleY / 2 + 34.0f, 50.0f, 12.0f, player.posX, 50.f};
	square oxygen_level_bg = { 50.0f, 12.0f, player.posX, player.posY + player.scaleY / 2 + 20.0f, 0.0f };
	square hotkey_item = { player.scaleX , 50.0f, player.posX, oxygen_level.posY + 50.0f, 0.0f ,0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

	static Level levels[numLevels] = { 0 }; // Arr of Levels, determined by const int of numlevels

	static Rope ropes[numRopes] = { 0 }; // Arr of Ropes, determined by const int of numRopes

	for (int i = 0; i < numLevels; i++)
	{
		levels[i].doorCount = 0;
		levels[i].roomCount = 0;
		levels[i].trapCount = 0;
		levels[i].trapSwitchCount = 0;
	}
	initializeLevels(levels); // Initialization Of Hardcoded Levels, Rooms, Doors

	initializeRopes(ropes);

	initializeLevers(levers, numLevers); // Initialization Of Hardcoded Levers

	// Using custom window procedure
	AESysInit(hInstance, nCmdShow, 1600, 900, 1, 60, false, NULL);

	// Changing the window title
	AESysSetWindowTitle("OceanBound");

	// reset the system modules
	AESysReset();


	AEGfxVertexList* redsquareMesh = CreateSquareMesh(0xFFFF0000);			// setting up mesh (red square for player)
	AEGfxVertexList* abilityMesh = CreateSquareMesh(0xFF1F0000, 0xFFAF0000); // setting up mesh (powerups on player)
	AEGfxVertexList* greysquareMesh = CreateSquareMesh(0xFFD3D3D3);			// setting up mesh (light grey square for room)
	AEGfxVertexList* lightgreensquareMesh = CreateSquareMesh(0xFFD3FFD3);	// setting up mesh (light green square for hotkey)
	AEGfxVertexList* blacksquareMesh = CreateSquareMesh(0xFF000000);		// setting up mesh (black squares for platforms/obstacles)
	AEGfxVertexList* lightbluesquareMesh = CreateSquareMesh(0xFF00BFFF);	// setting up mesh (light blue squares for wave)
	AEGfxVertexList* purplesquareMesh = CreateSquareMesh(0xFFA020F0);		// setting up mesh (purple squares for door)
	AEGfxVertexList* darkbluesquareMesh = CreateSquareMesh(0xFF0000FF);		// setting up mesh (dark blue squares for oxygen_level)
	AEGfxVertexList* whitesquareMesh = CreateSquareMesh(0xFFFFFFFF);		// setting up mesh (white squares for oxygen_level)
	AEGfxVertexList* darkRedSqMesh = CreateSquareMesh(0xFF8B0000);			// setting up mesh (dark red sq for health bar bg)
	AEGfxVertexList* topPlatformMesh = PlatformMeshTop(0xFF0000FF);			// setting up mesh (dark red sq for health bar bg)
	AEGfxVertexList* bottomPlatformMesh = PlatformMeshBottom(0xFF0000FF);	// setting up mesh (dark red sq for health bar bg)
	AEGfxVertexList* platformMesh = CreateSquareMesh(0xFF0000FF);			// setting up mesh (dark red sq for health bar bg)
	AEGfxVertexList* poisonMesh = CreateSquareMesh(0xFF61DE2A);				// setting up mesh (green for poison)

	AEGfxTexture* backgroundTex = AEGfxTextureLoad("Assets/BarebearsRoom.png");				//Room Tex
	AEGfxTexture* playerTex = AEGfxTextureLoad("Assets/BarebearsPlayer2.png");				//Player Tex
	AEGfxTexture* playerJumpTex = AEGfxTextureLoad("Assets/JumpSpritesheet.png");			//Player Jumping Tex
	AEGfxTexture* playerCrouchTex = AEGfxTextureLoad("Assets/BarebearsPlayerCrouch.png");	//Player Crouching Tex
	AEGfxTexture* doorTex = AEGfxTextureLoad("Assets/BarebearsDoor.png");					//Door Tex
	AEGfxTexture* buttonTex = AEGfxTextureLoad("Assets/BarebearsButton.png");				//Button Tex
	AEGfxTexture* buttonPressedTex = AEGfxTextureLoad("Assets/BarebearsButtonPressed.png"); //Button Pressed Tex
	AEGfxTexture* leverLeftTex = AEGfxTextureLoad("Assets/BarebearsLeverLeft.png");			//Lever Left Tex
	AEGfxTexture* leverStraightTex = AEGfxTextureLoad("Assets/BarebearsLeverStraight.png"); //Lever Straight Tex
	AEGfxTexture* leverRightTex = AEGfxTextureLoad("Assets/BarebearsLeverRight.png");		//Lever Right Tex
	AEGfxTexture* spikeTrapTex = AEGfxTextureLoad("Assets/BarebearsSpikeTraps.png");		//Spike Tex
	AEGfxTexture* ActiveShieldText = AEGfxTextureLoad("Assets/Barebears Shield_On_Player.png");//shield on player 
	AEGfxTexture* shieldTex = AEGfxTextureLoad("Assets/BarebearsShieldSymbol.png");			//Shield Tex
	AEGfxTexture* ActiveSpeedText = AEGfxTextureLoad("Assets/Barebears speed_On_Player.png");//shield on player 
	AEGfxTexture* speedTex = AEGfxTextureLoad("Assets/BarebearsSpeedSymbol.png");			//Speed Tex
	AEGfxTexture* ActiveBubbleText = AEGfxTextureLoad("Assets/Barebears Bubble_On_Player.png");//bubble on player
	AEGfxTexture* oxygenTex = AEGfxTextureLoad("Assets/BarebearsOxygenSymbol.png");			//Oxygen Tex
	AEGfxTexture* platformTex = AEGfxTextureLoad("Assets/NewPlatform.png");					//Platform Tex
	AEGfxTexture* ropeTex = AEGfxTextureLoad("Assets/Rope.png");							//Rope Tex
	AEGfxTexture* walkTex = AEGfxTextureLoad("Assets/WalkSpritesheet.png");					//Walk Tex
	AEGfxTexture* crouchTex = AEGfxTextureLoad("Assets/CrouchSpritesheet.png");				//Crouch Tex

	/*instruction texture*/
	AEGfxTexture* key_A = AEGfxTextureLoad("Assets/Barebears 20 A key.png"); // key_A
	AEGfxTexture* key_D = AEGfxTextureLoad("Assets/Barebears 20 D key.png"); // key_D
	AEGfxTexture* key_E = AEGfxTextureLoad("Assets/Barebears 20 E key.png"); // key_E
	AEGfxTexture* key_Lshift = AEGfxTextureLoad("Assets/Barebears 20 Lshift key.png"); // key_Lshift
	AEGfxTexture* key_Q = AEGfxTextureLoad("Assets/Barebears 20 Q key.png"); // key_Q
	AEGfxTexture* key_R = AEGfxTextureLoad("Assets/Barebears 20 R key.png"); // key_R
	AEGfxTexture* key_S = AEGfxTextureLoad("Assets/Barebears 20 S key.png"); // key_S
	AEGfxTexture* key_W = AEGfxTextureLoad("Assets/Barebears 20 W key.png"); // key_W
	AEGfxTexture* key_Space = AEGfxTextureLoad("Assets/Barebears 20 Space key.png"); // key_Space
	AEGfxTexture* key_Enter = AEGfxTextureLoad("Assets/Barebears 20 Enter key.png"); // key_Enter

	// Loads a music from given filepath and assign it to 'audio'
	AEAudio audio_background = AEAudioLoadSound("Assets/oceanbound.mp3");
	// Creates an audio group
	AEAudioGroup audio_group_background = AEAudioCreateGroup();

	//Basic Instructions
	// Load the font here 
	s8 pFont = AEGfxCreateFont("Assets/liberation-mono.ttf", 72);

	int currentLevel = GS_MAINMENU; // set first game stete to be main menu
	load_mainMenu();				// load all meshes, font for mainMenu

	// Game Loop
	while (gGameRunning)
	{
		// Informing the system about the loop's start
		AESysFrameStart();

		//AEAudioPlay(audio_background, audio_group_background, 3.f, 1.f, -1); // Play the music

		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);

		if (currentLevel == GS_QUIT || currentLevel == GS_MAINMENU)
		{
			initialise_mainMenu();
			if (leaderboardpopup)
			{
				Leaderboard::displayLeaderboardPopup(pFont, greysquareMesh, lightbluesquareMesh, &leaderboardpopup);
			}
			else if (showPlayerPopup)
			{
				displayPlayerInputPopup(pFont, &showPlayerPopup, greysquareMesh, whitesquareMesh, lightbluesquareMesh, lightgreensquareMesh, &currentLevel);
			}
			else
			{
				update_mainMenu(&gGameRunning, &leaderboardpopup, &showPlayerPopup);
			}
			AESysFrameEnd();
			continue;
		}
		if (currentLevel == GS_GAMEOVER) {
			if (reinit == false)
			{
				reinit = true;
				playerName = "";
				player = { 50.0f, 100.0f , -700.0f, -400.0f , 0.0f, 0.0f, 0.0f, 400.0f, 800.0f, 0.0f, 3000.0f, false, false, false, 1000.0f , 100.0f, GS_LEVEL1, 0, 1000.f,1000.f ,-1 };
				initializeLevels(levels); // Initialization Of Hardcoded Levels, Rooms, Doors

				initializeRopes(ropes);

				initializeLevers(levers, numLevers); // Initialization Of Hardcoded Levers

				oxygen_level = { 50.0f, 12.0f, player.posX, player.posY + player.scaleY / 2 + 20.0f, 0.0f, 100.0f, 5.0f, 0.0f, 50.0f, player.posX };
				healthbar = { player.posX, player.posY + player.scaleY / 2 + 34.0f, 50.0f, 12.0f, player.posX, 50.f };
				oxygen_level_bg = { 50.0f, 12.0f, player.posX, player.posY + player.scaleY / 2 + 20.0f, 0.0f };
				hotkey_item = { player.scaleX , 50.0f, player.posX, oxygen_level.posY + 50.0f, 0.0f ,0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
				time_in_sec = 0;
				gameTime = 0.0f;
				leverCooldown = 0.f;
				ropeCooldown = 0.f;
				activeHotkey = 0;  // Tracks the active hotkey after RETURN is pressed
				minutes = 0;
				seconds = 0;
				leaderboardpopup = false;
				showPlayerPopup = false;
				hasLeaderboardBeenLogged = false;
				player.onGround = true;
			}
			updateGameOver(&currentLevel);
			displayGameOverScreen(pFont, darkbluesquareMesh, lightbluesquareMesh, playerTex);

			//// Informing the system about the loop's end
			AESysFrameEnd();
			continue;
		} 
		if (currentLevel == GS_GAME_END) {
			if (reinit == false)
			{
				reinit = true;
				playerName = "";
				player = { 50.0f, 100.0f , -700.0f, -400.0f , 0.0f, 0.0f, 0.0f, 400.0f, 800.0f, 0.0f, 3000.0f, false, false, false, 1000.0f , 100.0f, GS_LEVEL1, 0, 1000.f,1000.f ,-1 };
				initializeLevels(levels); // Initialization Of Hardcoded Levels, Rooms, Doors

				initializeRopes(ropes);

				initializeLevers(levers, numLevers); // Initialization Of Hardcoded Levers

				oxygen_level = { 50.0f, 12.0f, player.posX, player.posY + player.scaleY / 2 + 20.0f, 0.0f, 100.0f, 5.0f, 0.0f, 50.0f, player.posX };
				healthbar = { player.posX, player.posY + player.scaleY / 2 + 34.0f, 50.0f, 12.0f, player.posX, 50.f };
				oxygen_level_bg = { 50.0f, 12.0f, player.posX, player.posY + player.scaleY / 2 + 20.0f, 0.0f };
				hotkey_item = { player.scaleX , 50.0f, player.posX, oxygen_level.posY + 50.0f, 0.0f ,0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
				time_in_sec = 0;
				gameTime = 0.0f;
				leverCooldown = 0.f;
				ropeCooldown = 0.f;
				activeHotkey = 0;  // Tracks the active hotkey after RETURN is pressed
				minutes = 0;
				seconds = 0;
				leaderboardpopup = false;
				showPlayerPopup = false;
				hasLeaderboardBeenLogged = false;
				player.onGround = true;
			}
			updateGameFin(&currentLevel);
			displayGameFinScreen(pFont, darkbluesquareMesh, lightbluesquareMesh, playerTex);

			//// Informing the system about the loop's end
			AESysFrameEnd();
			continue;
		}

		/* Initialisation (inside game loop) */
		/****************************************************************************************************************************/
		//Platform platforms[25];
		f32 dt = (f32)AEFrameRateControllerGetFrameTime();
		gameTime += dt;
		minutes = (int)(gameTime / 60);
		seconds = (int)(fmod(gameTime, 60.f));
		sprintf_s(timerTxt, "%02d:%02d", minutes, seconds);
		s32 no_of_platform_testLVL = initialise_platforms_level(platforms, currentLevel);
		reinit = false;
		
		/* update logic */
		/****************************************************************************************************************************/

		// Basic way to trigger exiting the application
		// when ESCAPE is hit or when the window is closed
		if (AEInputCheckTriggered(AEVK_ESCAPE) || 0 == AESysDoesWindowExist())
			gGameRunning = 0;

		if (currentLevel == GS_LEVEL5 && !hasLeaderboardBeenLogged)
		{
			hasLeaderboardBeenLogged = true;
			logLeaderboardEntry("leaderboard.txt", playerName, gameTime);
			playerName = "";
		}

		if (player.currhealth <= 0.f)
		{
			currentLevel = GS_GAMEOVER;
			continue;
		}
		if (leverCooldown > 0.0f)
		{
			leverCooldown -= dt;
			if (leverCooldown < 0.0f)
				leverCooldown = 0.0f;
		}

		if (ropeCooldown > 0.0f)
		{
			ropeCooldown -= dt;
			if (ropeCooldown < 0.0f)
				ropeCooldown = 0.0f;
		}

		/*
		*	Crouching mechanics and setting restriction on standing up
		*/

		handleCrouchStandAction(&player, platforms, no_of_platform_testLVL);
		
		/*
		*	resetting the hot key to 0, when power ups not in use
		*/
		revert_activeHotkey(&activeHotkey);

		/*
		*	special ability handling for speed
		*/
		handle_Speed_Ability(&activeHotkey, &player);

		/*
		*	left,right movement for player
		*/
		update_Movement(&player, /*&leverCooldown,*/ dt);

		update_Player_Animation(&player, dt);
		
		/*
		*	respawn mechanics for the player
		*/
		player_respawn(&player,  &oxygen_level, &oxygen_level_bg, &currentLevel);

		/*
		*	updating of tidals
		*/
		update_tidal(levels, currentLevel, dt);

		tryAttachToRope(&player, ropes, numRopes);

		/*
		*	jumping logic for player
		*/
		update_Overall_Jump(&player, levels, dt);

		/*
		*	ensures the player has gravity
		*	essentially, this takes in the player, as well as going to the level and room the player is currently in. By doing so, we can apply gravity to wherever the player is
		*	without causing the player to drop past the room boundary.
		*/
		apply_gravity_Per_Room(&player, &levels[player.currLevel].rooms[player.currRoom], dt);
		
		update_Player_Rope(&player, /*ropes,*/ dt);
		
		updatePlayerRopeCooldown(&player, dt);

		/*
		*	oxygen handling
		*/
		handle_Oxygen_Level(&oxygen_level, &player, &levels[player.currLevel].rooms[player.currRoom], &activeHotkey, &dt);

		/*
		*	update levers for each room 
		*/
		update_Collision_Player_Water_Lever(&player, levels, levers, numLevers, &currentLevel, &leverCooldown);
		
		/*
		*	Check if collided with Trap
		*/
		update_Collision_Player_Trap(&player, levels, &currentLevel, &activeHotkey, dt);
	
		/*
		*	check for collision bewteen player and trap switch
		*/
		update_Collision_Player_Trap_Switch(&player, levels, &currentLevel, dt);

		/*
		*	update collision bewteen player and doors for all levels
		*/
		update_Player_Door(&player, levels, &currentLevel);
		
		/*l
		*	Room Boundary For Player
		*	This basically uses the player tracker to checck if the player has exceeded the boundary of the current room or level.
		*/
		if (player.posX + player.scaleX / 2 >= levels[player.currLevel].rooms[player.currRoom].posX + levels[player.currLevel].rooms[player.currRoom].scaleX / 2 ||
			player.posY + player.scaleY / 2 >= levels[player.currLevel].rooms[player.currRoom].posY + levels[player.currLevel].rooms[player.currRoom].scaleY / 2 ||
			player.posX - player.scaleX / 2 <= levels[player.currLevel].rooms[player.currRoom].posX - levels[player.currLevel].rooms[player.currRoom].scaleX / 2 ||
			player.posY - player.scaleY / 2 <= levels[player.currLevel].rooms[player.currRoom].posY - levels[player.currLevel].rooms[player.currRoom].scaleY / 2)
		{
			apply_Player_Room_Boundary(&player, &levels[player.currLevel].rooms[player.currRoom]);;
		}

		/*
		*	Collision between player and water
		*/
		update_Collision_Player_Water(&player, levels, &activeHotkey, dt);
		
		/*
		*	Collision between player and platforms
		*/
		for (int i = 0; i < no_of_platform_testLVL; i++) {

			if (check_Collision_Player_Platform(&player, &platforms[i])) {
				handle_Collision_Player_Platform(&player, &oxygen_level, &oxygen_level_bg, &hotkey_item, &platforms[i]);
			}
		}

		/*
		*	ensures oxygen level and bg is always above player and hotkey_item is always above oxygen_level
		*/
		align_Player_object(&player, &oxygen_level, &oxygen_level_bg, &hotkey_item);

		drawHealthBar(&healthbar, &player); //ensures that healthbar is updated to player current health

		/* Apply rendering logic */
		/****************************************************************************************************************************/

		/*
		*	Applying transform for Rooms and Levels
		*	In order to draw the levels and rooms, we first loop through the 2 currently displayed levels.
		*	Next we loop through the rooms, and create the exterior and interior of the room (Thus enabling the Boundary Line)
		*	and then, if the room has water, we simply draw the water.
		*
		*	Exiting the room loop, we then loop through the doors.
		*	And after exiting the door loop, we draw the levers as long as their location equates to the levels being displayed.
		*/

		for (int i = currentLevel; i < currentLevel + 2; i++)
		{
			int levelCounterForLever = 0;
			for (int room_Counter = 0; room_Counter < levels[i].roomCount; room_Counter++)
			{
				//For exterior
				mesh_Draw_No_Rotate(levels[i].rooms[room_Counter].scaleX + 5.f, levels[i].rooms[room_Counter].scaleY + 5.f, levels[i].rooms[room_Counter].posX, levels[i].rooms[room_Counter].posY, redsquareMesh);
				
				//Interior
				mesh_Draw_With_Texture(0, levels[i].rooms[room_Counter].scaleX, levels[i].rooms[room_Counter].scaleY, levels[i].rooms[room_Counter].posX, levels[i].rooms[room_Counter].posY, greysquareMesh, backgroundTex);

				//For Water
				if (levels[i].rooms[room_Counter].hasWater)
				{
					for (int waterCounter = 0; waterCounter < levels[i].rooms[room_Counter].waterCount; waterCounter++)
					{
						if (levels[i].rooms[room_Counter].water[waterCounter].type == WATER_DEFAULT)
						{
							mesh_Draw(levels[i].rooms[room_Counter].water[waterCounter].rotation, levels[i].rooms[room_Counter].water[waterCounter].scaleX, levels[i].rooms[room_Counter].water[waterCounter].scaleY, levels[i].rooms[room_Counter].water[waterCounter].posX, levels[i].rooms[room_Counter].water[waterCounter].posY, lightbluesquareMesh);
						}
						else if (levels[i].rooms[room_Counter].water[waterCounter].type == WATER_LAVA)
						{
							mesh_Draw(levels[i].rooms[room_Counter].water[waterCounter].rotation, levels[i].rooms[room_Counter].water[waterCounter].scaleX, levels[i].rooms[room_Counter].water[waterCounter].scaleY, levels[i].rooms[room_Counter].water[waterCounter].posX, levels[i].rooms[room_Counter].water[waterCounter].posY, darkRedSqMesh);
						}
						else if (levels[i].rooms[room_Counter].water[waterCounter].type == WATER_POISON)
						{
							mesh_Draw(levels[i].rooms[room_Counter].water[waterCounter].rotation, levels[i].rooms[room_Counter].water[waterCounter].scaleX, levels[i].rooms[room_Counter].water[waterCounter].scaleY, levels[i].rooms[room_Counter].water[waterCounter].posX, levels[i].rooms[room_Counter].water[waterCounter].posY, poisonMesh);
						}
					}
				}
			}
			for (int door_counter = 0; door_counter < levels[i].doorCount; door_counter++)
			{
				mesh_Draw_With_Texture(0, levels[i].doors[door_counter].scaleX, levels[i].doors[door_counter].scaleY, levels[i].doors[door_counter].posX, levels[i].doors[door_counter].posY, purplesquareMesh, doorTex);
			}
			for (int leverCounter = 0; leverCounter < numLevers; leverCounter++)
			{
				if (levers[leverCounter].locationOfLever == i)
				{
					for (int a = 0; a < 2; a++) // loop through levels
					{
						int totalRoomsInLevelAffected = levers[leverCounter].numAffectedRoomsPerLevel[a];
						//printf("a is %d", a);
						if (totalRoomsInLevelAffected > 0)
						{
							for (int roomInLevelCounter = 0; roomInLevelCounter < totalRoomsInLevelAffected; roomInLevelCounter++)
							{
								//printf("b is %d", a);
								//printf("Room %d", levers[leverCounter].levelAffected[a][roomInLevelCounter]);
								if (levers[leverCounter].levelAffected[a][roomInLevelCounter] == -1)
								{
									continue;
								}
								else // if a room is affected by the lever
								{
									if (levers[leverCounter].leverType == LEVER_WATER_FLOW)
									{
										if (levels[i].rooms[levers[leverCounter].levelAffected[a][roomInLevelCounter]].water[levers[leverCounter].waterAffected[0]].state == WATER_FALLING)
										{
											mesh_Draw_With_Texture(levers[leverCounter].rotation, levers[leverCounter].scaleX, levers[leverCounter].scaleY, levers[leverCounter].posX, levers[leverCounter].posY, redsquareMesh, leverLeftTex);
										}
										else if (levels[i].rooms[levers[leverCounter].levelAffected[a][roomInLevelCounter]].water[levers[leverCounter].waterAffected[0]].state == WATER_STATIC)
										{
											mesh_Draw_With_Texture(levers[leverCounter].rotation, levers[leverCounter].scaleX, levers[leverCounter].scaleY, levers[leverCounter].posX, levers[leverCounter].posY, redsquareMesh, leverStraightTex);
										}
										else if (levels[i].rooms[levers[leverCounter].levelAffected[a][roomInLevelCounter]].water[levers[leverCounter].waterAffected[0]].state == WATER_RISING)
										{
											mesh_Draw_With_Texture(levers[leverCounter].rotation, levers[leverCounter].scaleX, levers[leverCounter].scaleY, levers[leverCounter].posX, levers[leverCounter].posY, redsquareMesh, leverRightTex);
										}
									}
									else if(levers[leverCounter].isTriggered&& levers[leverCounter].leverType == LEVER_TIDAL)
									{
										mesh_Draw_With_Texture(levers[leverCounter].rotation, levers[leverCounter].scaleX, levers[leverCounter].scaleY, levers[leverCounter].posX, levers[leverCounter].posY, redsquareMesh, leverLeftTex);
									}
									else if (!levers[leverCounter].isTriggered && levers[leverCounter].leverType == LEVER_TIDAL)
									{
										mesh_Draw_With_Texture(levers[leverCounter].rotation, levers[leverCounter].scaleX, levers[leverCounter].scaleY, levers[leverCounter].posX, levers[leverCounter].posY, redsquareMesh, leverRightTex);
									}
									
								}
							}
						}
					}
				}
			}
			for (int trapCounter = 0; trapCounter < levels[i].trapCount; trapCounter++)
			{
				if (levels[i].traps[trapCounter].scaleX > 100.f)
				{
					int numOfTrapsToDraw = (int)(levels[i].traps[trapCounter].scaleX / 100.f +1);
					for (int numOfTrapsDrawn = 0; numOfTrapsDrawn < numOfTrapsToDraw; numOfTrapsDrawn++)
					{
						float trapPosX = levels[i].traps[trapCounter].posX - levels[i].traps[trapCounter].scaleX /2 + levels[i].traps[trapCounter].scaleX / numOfTrapsToDraw /2 + numOfTrapsDrawn * levels[i].traps[trapCounter].scaleX / numOfTrapsToDraw;
						mesh_Draw_With_Texture(levels[i].traps[trapCounter].rotation, levels[i].traps[trapCounter].scaleX/numOfTrapsToDraw, levels[i].traps[trapCounter].scaleY, trapPosX, levels[i].traps[trapCounter].posY, redsquareMesh, spikeTrapTex);
					}
				}
				else {
					mesh_Draw_With_Texture(levels[i].traps[trapCounter].rotation, levels[i].traps[trapCounter].scaleX, levels[i].traps[trapCounter].scaleY, levels[i].traps[trapCounter].posX, levels[i].traps[trapCounter].posY, redsquareMesh, spikeTrapTex);
				}
			}
			if (levels[i].trapSwitchCount > 0)
			{
				for (int trapSwitchCounter = 0; trapSwitchCounter < levels[i].trapSwitchCount; trapSwitchCounter++)
				{
					if (levels[i].trapSwitches[trapSwitchCounter].isTriggered)
					{
						mesh_Draw_With_Texture(levels[i].trapSwitches[trapSwitchCounter].rotation, levels[i].trapSwitches[trapSwitchCounter].scaleX, levels[i].trapSwitches[trapSwitchCounter].scaleY, levels[i].trapSwitches[trapSwitchCounter].posX, levels[i].trapSwitches[trapSwitchCounter].posY, blacksquareMesh, buttonPressedTex);
					}
					else
					{
						mesh_Draw_With_Texture(levels[i].trapSwitches[trapSwitchCounter].rotation, levels[i].trapSwitches[trapSwitchCounter].scaleX, levels[i].trapSwitches[trapSwitchCounter].scaleY, levels[i].trapSwitches[trapSwitchCounter].posX, levels[i].trapSwitches[trapSwitchCounter].posY, blacksquareMesh, buttonTex);
					}
				}
			}
			levelCounterForLever++;
		}

		/*
		* applying transformation for platforms
		*/
		for (int i = 0; i < no_of_platform_testLVL; i++) {
			mesh_Draw_With_Texture(platforms[i].rotation, platforms[i].scaleX, platforms[i].scaleY, platforms[i].posX, platforms[i].posY, platformMesh, platformTex);
		}

		for (int i = currentLevel; i < currentLevel+2; i++)
		{
			for (int ropeCounter = 0; ropeCounter < numRopes; ropeCounter++) {
				if (ropes[ropeCounter].locationOfRope == (GS_STATES)i)
				{
					mesh_Draw_With_Texture(ropes[ropeCounter].rotation, ropes[ropeCounter].scaleX, ropes[ropeCounter].scaleY, ropes[ropeCounter].posX, ropes[ropeCounter].posY, blacksquareMesh, ropeTex);
				}
			}
		}

		/*
		*	applying transformation for oxygen level bg
		*/
		mesh_Draw(oxygen_level_bg.rotation, oxygen_level_bg.scaleX, oxygen_level_bg.scaleY, oxygen_level_bg.posX, oxygen_level_bg.posY, whitesquareMesh);

		/*
		*	applying transfomation for player oxygen level
		*/
		mesh_Draw(oxygen_level.rotation, oxygen_level.scaleX, oxygen_level.scaleY, oxygen_level.posX, oxygen_level.posY, darkbluesquareMesh);

		//Draw Health Bar and Background HP Bar
		mesh_Draw(0, healthbar.originalScaleX, healthbar.scaleY, healthbar.originalPosX, healthbar.posY, darkRedSqMesh);
		mesh_Draw(0, healthbar.scaleX, healthbar.scaleY, healthbar.posX, healthbar.posY, redsquareMesh);

		/*
		*	texture for powerups usage
		*/
		if (ability_two(&activeHotkey)) {
			mesh_Draw_With_Texture(player.rotation, player.scaleX, player.scaleY, player.posX, player.posY, abilityMesh, ActiveSpeedText);
		}
		
		/*
		*	applying transfomation for player char
		*/
		if (player.isJumping)
		{
			mesh_Draw_With_Texture(player.rotation, player.scaleX, player.scaleY, player.posX, player.posY, redsquareMesh, playerJumpTex);
		}
		else
		{
			draw_Player(&player, walkTex, crouchTex);
		}

		/*
		*	texture for powerups usage
		*/
		if (ability_one(&activeHotkey)) {
			mesh_Draw_With_Texture(player.rotation, player.scaleX, player.scaleY, player.posX, player.posY, abilityMesh, ActiveShieldText);
		}
		if (ability_three(&activeHotkey)) {
			mesh_Draw_With_Texture(player.rotation, player.scaleX, player.scaleY, player.posX, player.posY, abilityMesh, ActiveBubbleText);
		}


		/*
		*	display hotkey items depending on interaction
		*/
		display_Hotkey_Items(&hotkey_item, &player, &activeHotkey, lightgreensquareMesh, darkbluesquareMesh, blacksquareMesh, shieldTex, speedTex, oxygenTex, pFont);

		

		if (currentLevel == GS_LEVEL1) {
			//Instruction
			mesh_Draw_With_Texture(0, 40.0, 40.f, -650.f, -30.0f, blacksquareMesh, key_W); //W key
			mesh_Draw_With_Texture(0, 40.0, 40.f, -680.f, -75.0f, blacksquareMesh, key_A); //A key
			mesh_Draw_With_Texture(0, 40.0, 40.f, -635.f, -75.0f, blacksquareMesh, key_S); //S key
			mesh_Draw_With_Texture(0, 40.0, 40.f, -590.f, -75.0f, blacksquareMesh, key_D); //D key
			mesh_Draw_With_Texture(0, 100.0, 40.f, -700.f, -140.0f, blacksquareMesh, key_Lshift); // Lshift key
			mesh_Draw_With_Texture(0, 120.0, 40.f, -500.f, -140.f, blacksquareMesh, key_Space); //SPACE key
			mesh_Draw_With_Texture(0, 80.0, 40.f, -450.f, -75.0f, blacksquareMesh, key_Enter); //ENTER key
			mesh_Draw_With_Texture(0, 40.0, 40.f, -695.f, -30.0f, blacksquareMesh, key_Q); //Q key
			mesh_Draw_With_Texture(0, 40.0, 40.f, -605.f, -30.0f, blacksquareMesh, key_E); //E key
			mesh_Draw_With_Texture(0, 40.0, 40.f, -560.f, -30.0f, blacksquareMesh, key_R); //R key
			AEGfxPrint(pFont, "Player movement -> WASD", -0.5f, -0.1f, 0.4f, 1.f, 1.f, 1.f, 1.f);
			AEGfxPrint(pFont, "Crouch -> L-SHIFT  Jump -> SPACE", -0.5f, -0.2f, 0.4f, 1.f, 1.f, 1.f, 1.f);
			AEGfxPrint(pFont, "Action -> E  Respawn -> R", -0.5f, -0.3f, 0.4f, 1.f, 1.f, 1.f, 1.f);
			AEGfxPrint(pFont, "Power Ups -> Q", -0.95f, 0.9f, 0.4f, 1.f, 1.f, 1.f, 1.f);
			AEGfxPrint(pFont, "Usage of Power ups -> ENTER", -0.95f, 0.8f, 0.4f, 1.f, 1.f, 1.f, 1.f);
			AEGfxPrint(pFont, "Remember to use the Power-Ups for the next level", -0.95f, 0.7f, 0.4f, 1.f, 1.f, 1.f, 1.f);
		}

		// Render timer text at top right (adjust coordinates as needed)
		AEGfxPrint(pFont, timerTxt, 0.8f, 0.9f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f);

		// Informing the system about the loop's end
		AESysFrameEnd();
	}


	// free the system
	AEGfxMeshFree(redsquareMesh);
	AEGfxMeshFree(blacksquareMesh);
	AEGfxMeshFree(lightbluesquareMesh);
	AEGfxMeshFree(darkbluesquareMesh);
	AEGfxMeshFree(greysquareMesh);
	AEGfxMeshFree(purplesquareMesh);
	AEGfxMeshFree(lightgreensquareMesh);
	AEGfxMeshFree(abilityMesh);
	AEGfxMeshFree(whitesquareMesh);
	AEGfxMeshFree(darkRedSqMesh);
	AEGfxMeshFree(topPlatformMesh);
	AEGfxMeshFree(bottomPlatformMesh);
	AEGfxMeshFree(poisonMesh);
	AEGfxTextureUnload(backgroundTex);
	AEGfxTextureUnload(playerTex);
	AEGfxTextureUnload(playerJumpTex);
	AEGfxTextureUnload(playerCrouchTex);
	AEGfxTextureUnload(doorTex);
	AEGfxTextureUnload(buttonTex);
	AEGfxTextureUnload(buttonPressedTex);
	AEGfxTextureUnload(leverLeftTex);
	AEGfxTextureUnload(leverStraightTex);
	AEGfxTextureUnload(leverRightTex);
	AEGfxTextureUnload(spikeTrapTex);
	AEGfxTextureUnload(platformTex);
	AEGfxTextureUnload(ActiveShieldText);
	AEGfxTextureUnload(shieldTex);
	AEGfxTextureUnload(ActiveSpeedText);
	AEGfxTextureUnload(speedTex);
	AEGfxTextureUnload(ActiveBubbleText);
	AEGfxTextureUnload(oxygenTex);
	AEGfxTextureUnload(ropeTex);
	AEGfxTextureUnload(walkTex);
	AEGfxTextureUnload(crouchTex);

	AEGfxTextureUnload(key_A);
	AEGfxTextureUnload(key_D);
	AEGfxTextureUnload(key_E);
	AEGfxTextureUnload(key_Lshift);
	AEGfxTextureUnload(key_Q);
	AEGfxTextureUnload(key_R);
	AEGfxTextureUnload(key_S);
	AEGfxTextureUnload(key_W);
	AEGfxTextureUnload(key_Space);
	AEGfxTextureUnload(key_Enter);

	// Release our audios.
	AEAudioUnloadAudio(audio_background);

	// Release our audio groups.
	AEAudioUnloadAudioGroup(audio_group_background);
	AEGfxDestroyFont(pFont);

	unload_mainMenu();

	AESysExit();
}
