/* Start Header ************************************************************************/
/*!
\file       room.cpp
\project    Ocean Bound
\author     Kye Le, Jethro, Reginald , Dillon  
\date       April, 06, 2025
\brief      This file contains the core logic for initializing all level layouts and managing dynamic in-game systems
            such as tidal water behavior, trap activation, door teleportation, and per-room player constraints.
            It includes functionality for:
                - Configuring room/door/trap setups per level
                - Linked tidal water rising/falling behavior
                - Gravity and collision boundaries per room
                - AABB-based player interactions with doors, traps, and switches
                - Context-aware room teleportation and level progression logic

            Water/Tidal Mechanics/Door/Traps/Switch/Room/Player-Enviroment/Gravity -> Jethro
            AABB Collision/Gravity -> Kye Le
            Initialization Rooms -> Reginald
            Multi Water block -> dillon  

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include "Room.hpp"
#include "player.h"
#include <math.h>
#include "pch.h"
#include "GameStateManager.h"

//Level levels[4];
/*
    Function that initializes all levels
*/
void initializeLevels(Level levels[]) {
    //level 1
    levels[0].roomCount = 3;
    levels[0].doorCount = 5;
    levels[0].trapCount = 1;
    levels[0].trapSwitchCount = 0;
    levels[0].rooms[0] = { 995.0f, 445.f, -300.0f, -225.0f,false };
    levels[0].rooms[1] = { 295.f, 445.f, 350.0f, -225.0f, true, true, 1, { {295.0f, 0.0f, 350.f, groundLevel, groundLevel, WATER_RISING,200.f, false, WATER_POISON,-1} } };
    levels[0].rooms[2] = { 295.f, 445.f, 650.0f, -225.0f, false };
    levels[0].doors[0] = { 100.f, 100.f, 150.f, -210.0f, 0, 1, false };
    levels[0].doors[1] = { 100.f, 100.f, 400.f, -397.5f, 0, 0, false };
    levels[0].doors[2] = { 100.f, 100.f, 300.f, -322.5f, 0, 3, false };
    levels[0].doors[3] = { 100.f, 100.f, 700.f, -322.5f, 0, 2, false };
    levels[0].doors[4] = { 100.f, 100.f, 575.f, -240.f, 1, 0, false };
    levels[0].traps[0] = { 100.f, 50.f, -250.f, -265.f, 0.f, false, PROXIMITY_TRAP, false, 50.f, -265.f, false, 200.f, 0.f, 1.5f };

    //level 2
    levels[1].doorCount = 4;
    levels[1].roomCount = 2;
    levels[1].trapCount = 0;
    levels[1].trapSwitchCount = 0;
    levels[1].rooms[0] = { 995.0f, 445.f, -300.0f, 225.0f, false, false, 1, {{995.f, 0.0f, -300.f, 0.5f, 0.f, WATER_RISING,300.f, false,WATER_DEFAULT,-1}} };
    levels[1].rooms[1] = { 595.f, 445.f, 500.0f, 225.0f, true, false, 1, {{ 595.f, 395.0f, 500.f, 199.5f, 200.f, WATER_STATIC, 0.f, false,WATER_DEFAULT ,-1}} };
    levels[1].doors[0] = { 100.f, 100.f, -650.f, 190.0f, 0, 4, false };
    levels[1].doors[1] = { 100.f, 100.f, 150.f, 390.0f, 1, 2, false };
    levels[1].doors[2] = { 100.f, 100.f, 250.f, 50.0f, 1, 1, false };
    levels[1].doors[3] = { 100.f, 100.f, 750.f, 50.0f, 2, 0, true };

    //level 3
    levels[2].roomCount = 3;
    levels[2].doorCount = 7;
    levels[2].trapCount = 1;
    levels[2].trapSwitchCount = 1;
    levels[2].rooms[0] = { 350.0f, 545.f, -622.0f, -175.0f,false, false, 2, {{ 350.0f, 0.f, -622.0f, groundLevel, groundLevel, WATER_RISING, 200.0f, false, WATER_DEFAULT,1, false },{268.f, 0.f, -406.f, groundLevel+195.f, groundLevel+195.f, WATER_RISING, 200.f, false, WATER_DEFAULT,0, true}} };
    levels[2].rooms[1] = { 350.0f, 345.f, -622.0f, 275.0f,true, true, 1, {{350.0f, 0.0f,-622.0f, 100.0f, 300.0f, WATER_RISING,170.0f,false,WATER_LAVA,-1} }};
    levels[2].rooms[2] = { 1240.0f, 345.f, 177.0f, 275.0f,true, false, 1, {{1240.0f, 350.0f, 177.0f, 275.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_DEFAULT,-1}} };
    levels[2].doors[0] = { 100.f, 100.f, -650.f, -395.0f, 1, 3, true };
    levels[2].doors[1] = { 100.f, 100.f, -750.f, -200.0f, 2, 2, false };
    levels[2].doors[2] = { 100.f, 100.f, -750.f, 155.0f, 2, 1, false };
    levels[2].doors[3] = { 100.f, 100.f, -750.f, 325.0f, 2, 0, false };
    levels[2].doors[4] = { 100.f, 100.f, -500.f, 325.0f, 2, 5, false };
    levels[2].doors[5] = { 100.f, 100.f, -390.f, 355.0f, 2, 4, false };
    levels[2].doors[6] = { 100.f, 100.f, 730.f, 155.0f, 3, 0, false };
    levels[2].traps[0] = { 110.f, 25.f, -650.f, -228.f, 0.f, false, PROXIMITY_TRAP, false, 25.f, -228.f, false, 200.f, 0.f, 1.5f };
    levels[2].trapSwitches[0] = { 60.f, 50.f, -570.f, -35.f,false, {0}, 1 };

    //level 4
    levels[3].doorCount = 10;
    levels[3].roomCount = 4;
    levels[3].trapCount = 3;
    levels[3].trapSwitchCount = 0;
    levels[3].rooms[0] = { 300.f, 270.f, -293.0f, -35.0f, false };
    levels[3].rooms[1] = { 630.f, 270.f, 177.0f, -35.0f, false, false, 1, {{630.0f,0.0f, 177.0f, -170.0f, -170.0f, WATER_STATIC, 150.f,false, WATER_DEFAULT, -1, false,false, false,WATER_FALLING} } };
    levels[3].rooms[2] = { 300.f, 270.f, 647.0f, -35.0f, false,false, 1, {{300.0f, 0.0f,647.0f, -170.0f,-170.0f, WATER_STATIC,200.0f,false,WATER_DEFAULT, -1, false, false, false, WATER_FALLING}} };
    levels[3].rooms[3] = { 1240.f, 275.f, 177.0f, -311.0f, false, false, 1, {{1240.0f, 0.0f, 177.0f, groundLevel, groundLevel, WATER_STATIC, 150.0f,false,WATER_DEFAULT ,-1, false, false, false, WATER_FALLING }} };
    levels[3].doors[0] = { 100.f, 100.f, -390.f, -119.0f, 2, 6, false };
    levels[3].doors[1] = { 100.f, 100.f, -200.f, -119.0f, 3, 2, false };
    levels[3].doors[2] = { 100.f, 100.f, -85.f, 45.0f, 3, 1, false };
    levels[3].doors[3] = { 100.f, 100.f, 440.f, 45.0f, 3, 4, false };
    levels[3].doors[4] = { 100.f, 100.f, 740.f, -119.0f, 3, 3, false };
    levels[3].doors[5] = { 100.f, 100.f, 560.f, 15.0f, 3, 6, false };
    levels[3].doors[6] = { 100.f, 100.f, -390.f, -230.0f, 3, 5, false };
    levels[3].doors[7] = { 100.f, 100.f, 440.f, -230.0f, 3, 8, false };
    levels[3].doors[8] = { 100.f, 100.f, 100.f, (groundLevel +55), 3, 7, false };
    levels[3].doors[9] = { 100.f, 100.f, 740.f, -230, 4, 0, true };
    levels[3].traps[0] = { 500.f, 25.f, 242.f, -158.f, 0.f, false, DEFAULT_SPIKE_TRAP, true, 25.f, -158.f };
    levels[3].traps[1] = { 350.f, 25.f, -145.f, (groundLevel +15), 0.f, false, TIMED_TRAP, true, 25.f, (groundLevel + 15), false, 0.f, 2.0f, 1.5f };
    levels[3].traps[2] = { 250.f, 25.f, 670.f, (groundLevel + 15), 0.f, false, DEFAULT_SPIKE_TRAP, true, 25.f, (groundLevel + 15), false, 0.f, 0.f, 1.5f };

    //level 5
    levels[4].doorCount = 6;
    levels[4].roomCount = 2;
    levels[4].trapCount = 2;
    levels[4].trapSwitchCount = 0;
    levels[4].rooms[0] = { 290.0f, 445.f, -653.0f, 225.0f,true, true, 1, {{290.0f, 0.0f,-653.0f, 0.0f, 0.0f, WATER_RISING,300.0f,false,WATER_LAVA,-1, false} } };
    levels[4].rooms[1] = { 1300.0f, 445.f, 147.0f, 225.0f,true, false,5,{{270.0f, 230.0f, -240.0f, 335.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_POISON,-1}, //1
                                                                         {897.0f, 0.0f,347.0f, 230.0f, 230.0f, WATER_STATIC,225.0f,false,WATER_DEFAULT,3, true, false, false, WATER_FALLING }, // 2 on top
                                                                         {320.0f, 225.0f, -213.0f, 115.0f, 115.0f, WATER_STATIC,225.0f,false,WATER_LAVA,-1, false}, //3
                                                                         {250.0f, 0.0f,325.0f, 0.0f, 0.0f, WATER_STATIC,245.0f,true,WATER_DEFAULT,1, false, false, false, WATER_FALLING }, // 4 below
                                                                         {350.0f, 0.0f,625.0f, 0.0f, 0.0f, WATER_STATIC,225.0f,false,WATER_DEFAULT,-1, false, false, false, WATER_FALLING }} };//5
    levels[4].doors[0] = { 100.f, 100.f, -740.f, 360.0f, 3, 9, true };
    levels[4].doors[1] = { 100.f, 100.f, -580.f, 53.0f, 4, 2, false };
    levels[4].doors[2] = { 100.f, 100.f, -440.f, 53.0f, 4, 1, false };
    levels[4].doors[3] = { 100.f, 100.f, -160.f, 280.0f, 4, 4, false };
    levels[4].doors[4] = { 100.f, 100.f, -40.f, 390.0f, 4, 3, false };
    levels[4].doors[5] = { 100.f, 100.f, 740.f, 53.0f, 5, 0, false };
    levels[4].traps[0] = { 200.f, 25.f, 0.f, 243.0f, 0.f, false, DEFAULT_SPIKE_TRAP, true, 25.f, 243.0f, false, 0.f, 0.f, 1.5f };
    levels[4].traps[1] = { 500.f, 25.f, 200.f, 15.0f, 0.f, false, TIMED_TRAP, true, 25.f, 15.f, false, 0.f, 1.5f, 1.5f };

    //level 6
    levels[5].doorCount = 10;
    levels[5].roomCount = 1;
    levels[5].trapCount = 1;
    levels[5].trapSwitchCount = 0;
    levels[5].rooms[0] = { 1595.0f, 445.f, 0.0f, -225.0f,true,true , 2, {{1145.0f, 0.0f,-225.0f, groundLevel, groundLevel, WATER_RISING,200.0f,false,WATER_LAVA,-1, false},
                                                                          {445.0f, 440.0f,575.0f, groundLevel + 225, groundLevel + 220, WATER_STATIC,0.0f,false,WATER_DEFAULT,-1, false}} };
    levels[5].doors[0] = { 100.f, 100.f, -630.f, -90.0f, 4, 5, false };
    levels[5].doors[1] = { 100.f, 100.f, 450.f, -90.0f, 5, 0, false };
    levels[5].doors[2] = { 100.f, 100.f, 580.f, -90.0f, 5, 0, false };
    levels[5].doors[3] = { 100.f, 100.f, 710.f, -90.0f, 5, 0, false };
    levels[5].doors[4] = { 100.f, 100.f, 450.f, -220.0f, 5, 0, false };
    levels[5].doors[5] = { 100.f, 100.f, 580.f, -220.0f, 5, 0, false };
    levels[5].doors[6] = { 100.f, 100.f, 710.f, -220.0f, 6, 0, true };
    levels[5].doors[7] = { 100.f, 100.f, 450.f, -350.0f, 5, 0, false };
    levels[5].doors[8] = { 100.f, 100.f, 580.f, -350.0f, 5, 0, false };
    levels[5].doors[9] = { 100.f, 100.f, 710.f, -350.0f, 5, 0, false };
    levels[5].traps[0] = { 900.f, 25.f, -100.f, groundLevel + 17, 0.f, false, DEFAULT_SPIKE_TRAP, true, 25.f, groundLevel + 17, false, 0.f, 0.f, 1.5f };

    //level 7
    levels[6].doorCount = 8;
    levels[6].roomCount = 3;
    levels[6].trapCount = 2;
    levels[6].trapSwitchCount = 0;
    levels[6].rooms[0] = { 1155.0f, 445.f, -220.0f, 225.0f,true, false,4, {{145.0f, 445.0f, -725.0f, 224.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_POISON,-1},
                                                                        {240.0f, 0.0f, -278.0f, 248.0f, 0.f,WATER_STATIC, 130.0f,false,WATER_DEFAULT,-1, false, false, false, WATER_FALLING},
                                                                        {200.0f, 0.0f, 45.0f, 248.0f, 0.f,WATER_STATIC, 130.0f,false,WATER_DEFAULT,-1, false, false, false, WATER_FALLING},
                                                                        {410.0f, 0.0f, 152.0f, 0.0f, 0.0f, WATER_STATIC,300.0f,false,WATER_DEFAULT,-1, false, false, false, WATER_FALLING} } };
    levels[6].rooms[1] = { 435.0f, 445.f, 580.0f, 225.0f,true, false,1,{{435.0f, 445.0f, 580.0f, 225.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_LAVA,-1, false, false,false,WATER_RISING} } };
    levels[6].rooms[2] = { 435.0f, 445.f, 580.0f, -225.0f,true, false, 1, {435.0f, 445.0f, 580.0f, -225.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_POISON,-1} };
    levels[6].doors[0] = { 100.f, 100.f, -740.f, 53.0f, 5, 6, true };
    levels[6].doors[1] = { 100.f, 100.f, -580.f, 380.0f, 6, 2, false };
    levels[6].doors[2] = { 100.f, 100.f, 50.f, 380.0f, 6, 1, false };
    levels[6].doors[3] = { 100.f, 100.f, 300.f, 380.0f, 6, 4, false };
    levels[6].doors[4] = { 100.f, 100.f, 420.f, 53.0f, 6, 3, false };
    levels[6].doors[5] = { 100.f, 100.f, 740.f, 53.0f, 6, 6, false };
    levels[6].doors[6] = { 100.f, 100.f, 420.f, -63.0f, 6, 5, false };
    levels[6].doors[7] = { 100.f, 100.f, 740.f, groundLevel + 53, 7, 1, false };
    levels[6].traps[0] = { 240.f, 25.f, -275.f, 260.0f, 0.f, false, DEFAULT_SPIKE_TRAP, true, 25.f, 260.0f, false, 0.f, 0.f, 1.5f };
    levels[6].traps[1] = { 240.f, 25.f, -275.f, 230.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 25.f, 230.0f, false, 0.f, 0.f, 1.5f };

    //level 8
    levels[7].doorCount = 4;
    levels[7].roomCount = 1;
    levels[7].trapCount = 6;
    levels[7].trapSwitchCount = 0;
    levels[7].rooms[0] = { 1155.0f, 445.f, -220.0f, -225.0f,true,false, 2, {{450.0f, 185.0f, 132.0f, groundLevel + 95.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_DEFAULT,-1},{705.0f, 185.0f, -445.0f, groundLevel + 95.0f, 0.f,WATER_STATIC, 0.0f,false,WATER_LAVA,-1} } };
    levels[7].doors[0] = { 100.f, 100.f, -650.f, -200.0f, 8, 0, true }; // go to end game page
    levels[7].doors[1] = { 100.f, 100.f, 300.f, -63.0f, 6, 7, false };
    levels[7].doors[2] = { 100.f, 100.f, 0.f, -63.0f, 7, 3, false };
    levels[7].doors[3] = { 100.f, 100.f, 300.f, groundLevel + 53, 7, 2, false };
    levels[7].traps[0] = { 200.f, 40.f, 150.f, -22.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 40.f,  -22.0f, false, 0.f, 0.f, 1.5f };
    levels[7].traps[1] = { 70.f, 100.f, -110.f, -52.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 100.f,  -52.0f, false, 0.f, 0.f, 1.5f };
    levels[7].traps[2] = { 70.f, 100.f, -210.f, -52.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 100.f,  -52.0f, false, 0.f, 0.f, 1.5f };
    levels[7].traps[3] = { 70.f, 100.f, -310.f, -52.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 100.f,  -52.0f, false, 0.f, 0.f, 1.5f };
    levels[7].traps[4] = { 70.f, 100.f, -410.f, -52.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 100.f,  -52.0f,false, 0.f, 0.f, 1.5f };
    levels[7].traps[5] = { 70.f, 100.f, -510.f, -52.0f, 3.14f, false, DEFAULT_SPIKE_TRAP, true, 100.f,  -52.0f,false, 0.f, 0.f, 1.5f };

    //9
    levels[8].doorCount = 1;
    levels[8].roomCount = 1;
    levels[8].trapCount = 0;
    levels[8].trapSwitchCount = 0;
    levels[8].rooms[0] = { 1595.0f, 445.f, 0.0f, -225.0f, false };
    levels[8].doors[0] = { 100.f, 100.f, -630.f, groundLevel +55, 7, 0, true };

    //10
    levels[9].doorCount = 0;
    levels[9].roomCount = 1;
    levels[9].trapCount = 0;
    levels[9].trapSwitchCount = 0;
    levels[9].rooms[0] = { 1595.0f, 445.f, 0.0f, 225.0f, false };

}

/*
    Here, we basically check, if the water in the room has reached the max tidal height, if it has not, the water level will rise (Handles case of when tidal level rises)
    else, if the max tidal height has been reached, and scale is less than or equal to 0, we set the water back to ground level of the room and set max tidal to false.(Handles case of when tidal level has been reached at some point, and tidal decrease has reached below minimum)
    else, if we have reached the max tidal height, and water scale is above 0, we will have the water level drop. (Handles decrease of tidal levels.)
    lastly, if maxtidal reached bool is not true, but max tidal height has been reached, the max tidal bool will be set to true to begin decrease of water levels.
    Together this function will enable the rising and decline of tidal levels within a level over multiple frames.
*/
void handle_Tidal(Room * room, float dt)
{
    for (int i = 0; i < room->waterCount; i++)
    {
        Water* waterBlock = room->water + i;
        if (waterBlock->permaFlow && waterBlock->state == WATER_RISING && !waterBlock->maxTidalReached)
        {
            bool rise = true;
            if (waterBlock->linkedID != -1) // basically if we find this has a water block linked to it.
            {
                Water* linkedWater = room->water + (waterBlock->linkedID);
                if (waterBlock->LinkedAbove) // if block is above linked block
                {
                    if (!linkedWater->maxTidalReached) // if block below has not reached the max tidal
                    {
                        rise = false;
                    }
                }
                else if (linkedWater->scaleY > 0) // if block is below linked block, and linked block is still rising or falling
                {
                    rise = false;
                }
            }
            if (rise)
            {
                waterBlock->scaleY += 50.f * dt;
                waterBlock->posY += 25.f * dt;
                if (waterBlock->scaleY >= waterBlock->maxTidalHeight)
                {
                    waterBlock->scaleY = waterBlock->maxTidalHeight;
                    waterBlock->maxTidalReached = true;
                }
            }
        }
        else if (waterBlock->state == WATER_FALLING && waterBlock->permaFlow && waterBlock->scaleY>0.f)
        {
            bool fall = true;
            if (waterBlock->linkedID != -1) // basically if we find this has a water block linked to it.
            {
                Water* linkedWater = room->water + (waterBlock->linkedID);
                if (!waterBlock->LinkedAbove) // if block below linked block
                {
                    if (linkedWater->scaleY > 0) // if block above has not bottomed out
                    {
                        fall = false;
                    }
                }
            }
            if (fall)
            {
                waterBlock->scaleY -= 50.f * dt;
                waterBlock->posY -= 25.f * dt;
                waterBlock->maxTidalReached = false;
                if (waterBlock->scaleY <= 0.f)
                {
                    waterBlock->scaleY = 0.f;
                }

            }
        }
        else if (waterBlock->state == WATER_RISING && !waterBlock->maxTidalReached && room->hasTidal)
        {
            bool rise = true;
            if (waterBlock->linkedID != -1) // basically if we find this has a water block linked to it.
            {
                Water* linkedWater = room->water+ (waterBlock->linkedID);
                if (waterBlock->LinkedAbove) // if block is above linked block
                {
                    if (!linkedWater->maxTidalReached) // if block below has not reached the max tidal
                    {
                        rise = false;
                    }
                }
                else if (linkedWater->scaleY > 0) // if block is below linked block, and linked block is still rising or falling
                {
                    rise = false;
                }
            }
            if (rise)
            {
                waterBlock->scaleY += 50.f * dt;
                waterBlock->posY += 25.f * dt;
                if (waterBlock->scaleY >= waterBlock->maxTidalHeight)
                {
                    waterBlock->scaleY = waterBlock->maxTidalHeight;
                    waterBlock->maxTidalReached = true;
                    waterBlock->state = WATER_FALLING; // Basically, after the blockhas reached its maximum it is time for this block to start decreasing. Thus, we do this to flip the state of the water.
                    //Do note, since in falling, we check the linked block above if it has fallen back to zero, this logic is correct.
                }
            }
        }
        else if (waterBlock->state == WATER_FALLING && waterBlock->waterMustBeDrained && waterBlock->scaleY > 0.f && room->hasTidal)
        {
            bool fall = true;
            if (waterBlock->linkedID != -1) // basically if we find this has a water block linked to it.
            {
                Water* linkedWater = room->water + (waterBlock->linkedID);
                if (!waterBlock->LinkedAbove) // if block below linked block
                {
                    if (linkedWater->scaleY > 0.f) // if block above has not bottomed out
                    {
                        fall = false;
                    }
                }
            }
            if (fall)
            {
                waterBlock->scaleY -= 50.f * dt;
                waterBlock->posY -= 25.f * dt;
                waterBlock->maxTidalReached = false;
                if (waterBlock->scaleY <= 0.0f)
                {
                    waterBlock->scaleY = 0.0f;
                    waterBlock->waterMustBeDrained = false;
                    if (!waterBlock->LinkedAbove)
                    {
                        room->hasTidal = false;
                    }
                    waterBlock->state = WATER_RISING; // Basically, after the block above has fallen to 0 , it is time for this block to start decreasing. Thus, we do this to flip the state of the water.
                }

            }
        }
        else if (waterBlock->state == WATER_FALLING && waterBlock->scaleY > 0.f && room->hasTidal)
        {
            bool fall = true;
            if (waterBlock->linkedID != -1) // basically if we find this has a water block linked to it.
            {
                Water* linkedWater = room->water + (waterBlock->linkedID);
                if (!waterBlock->LinkedAbove) // if block below linked block
                {
                    if (linkedWater->scaleY>0) // if block above has not bottomed out
                    {
                        fall = false;
                    }
                }
            }
            if (fall)
            {
                waterBlock->scaleY -= 50.f * dt;
                waterBlock->posY -= 25.f * dt;
                waterBlock->maxTidalReached = false;
                if (waterBlock->scaleY <= 0.f)
                {
                    waterBlock->scaleY = 0.f;
                    waterBlock->state = WATER_RISING; // Basically, after the block above has fallen to 0 , it is time for this block to start decreasing. Thus, we do this to flip the state of the water.
                }

            }
        }
    }
}

/*
	Check Each Room for Tidal
	Regarding this, the logic is that we loop through each level based on the current level up til less than current level +2
	This is because we are displaying 2 levels at a time.
	We then loop through the room counter, based on the room count of the currently looped level, and check if the room has both water and tidal
	If so, we implement the tidal function by passing in the room and dt.
*/
void update_tidal(Level levels[], int currentLevel, float dt) {
    for (int i = currentLevel; i < currentLevel + 2; i++)
    {
        for (int room_Counter = 0; room_Counter < levels[i].roomCount; room_Counter++)
        {
            for (int water_counter = 0; water_counter < 5; water_counter++)
            {
                if (levels[i].rooms[room_Counter].hasWater && levels[i].rooms[room_Counter].water[water_counter].state != WATER_STATIC)
                {
                    handle_Tidal(&levels[i].rooms[room_Counter], dt);
                }
            }
        }
    }
}

/*
    Simple Bool check AABB if player has collided with the door (player's poxX and posY with door)
*/
bool check_Collision_Player_Door(Player* player, Door* doors) {

    f32 door_left = doors->posX - doors->scaleX / 2;
    f32 door_top = doors->posY + doors->scaleY / 2;
    f32 door_right = doors->posX + doors->scaleX / 2;
    f32 door_bottom = doors->posY - doors->scaleY / 2;

    return ( player->posX < door_right &&  player->posX > door_left &&  player->posY > door_bottom &&  player->posY < door_top);
}

/*
    If player activates the door via pressing we, we first initialize a destination room index. This will be used later to point to the room where the player will teleport to
    Next, we set the position of the player x and y to the door exit.
    After this, remember, we are using level-room boundary to define the boundary limit of the player.
    Hence, we need to set the player current level to the destination level of the door.
    Following this, we loop through all the rooms wihtin the level the player is teleporting to, and from there, we determine if the destination door position is within the coordinates or
    said room, enabling us to set the destinationRoomIndex to the correct room. This finally allows us to assign the new room the player is in, to the player tracker.
*/
void handle_Collision_Player_Door(Player* player, Door* doorToHandle, Level* levels, int* currLevel) {
    if (AEInputCheckTriggered(AEVK_E)) {
        int destinationRoomIndex = 0;
        if (doorToHandle->destinationLevel == 8)
        {
            *currLevel = GS_GAME_END;
            return;
        }
        player->posX = levels[doorToHandle->destinationLevel].doors[doorToHandle->destinationDoor].posX;
        player->posY = levels[doorToHandle->destinationLevel].doors[doorToHandle->destinationDoor].posY;
        player->currLevel = doorToHandle->destinationLevel;
        for (int i = 0; i < levels[doorToHandle->destinationLevel].roomCount; i++)
        {
            if (levels[doorToHandle->destinationLevel].doors[doorToHandle->destinationDoor].posX < levels[doorToHandle->destinationLevel].rooms[i].posX+ levels[doorToHandle->destinationLevel].rooms[i].scaleX/2 &&
                levels[doorToHandle->destinationLevel].doors[doorToHandle->destinationDoor].posX > levels[doorToHandle->destinationLevel].rooms[i].posX- levels[doorToHandle->destinationLevel].rooms[i].scaleX/2 &&
                levels[doorToHandle->destinationLevel].doors[doorToHandle->destinationDoor].posY < levels[doorToHandle->destinationLevel].rooms[i].posY + levels[doorToHandle->destinationLevel].rooms[i].scaleY/2 &&
				levels[doorToHandle->destinationLevel].doors[doorToHandle->destinationDoor].posY > levels[doorToHandle->destinationLevel].rooms[i].posY - levels[doorToHandle->destinationLevel].rooms[i].scaleY/ 2
                )
            {
                destinationRoomIndex = i;
                break;
            }
        }
        player->currRoom = destinationRoomIndex;
        if (doorToHandle->triggerNextLevel)
        {
            if (doorToHandle->destinationLevel > *currLevel)
            {
                *currLevel += 2;
            }
            else
            {
                *currLevel -= 2;
            }
        }
    }
}

/*
	Collision between player and door
	NOTE: Here we initialize a bool called passing now.
	This is required, because in the case where a player on level 1 access the door and jumps to level 2,
	the player would not return back to level 1 in the same frame. This is because after looping through level 1, the loop
	proceeds to level 2, triggering the level 2 door since in the current frame, E is still being pressed until the frame ends.
	Thus, passing Now is needed in order to skip the next loop in the case where the door has been activated by the player.
*/
void update_Player_Door(Player *player, Level levels[], int *currentLevel) {
    bool passingNow = false;
    for (int i = *currentLevel; i < *currentLevel + 2; i++)
    {
        if (passingNow)
        {
            break;
        }
        for (int door_Count = 0; door_Count < levels[i].doorCount; door_Count++)
        {
            if (check_Collision_Player_Door(player, &levels[i].doors[door_Count]) && passingNow == false) {
                if (player->isCrouching)
                {
                    break;
                }
                handle_Collision_Player_Door(player, &levels[i].doors[door_Count], &levels[0], currentLevel);
                passingNow = true;
            }
        }
    }
}

/*
    This basically limits the player to the boundary of the room.
*/
void apply_Player_Room_Boundary(Player* player, Room* room) {
    if (player->posX - player->scaleX / 2 <= room->posX - room->scaleX / 2) {
        player->posX = room->posX - room->scaleX / 2 + player->scaleX / 2;
        player->velX = 0;
    }
    if (player->posX + player->scaleX / 2 >= room->posX + room->scaleX / 2) {
        player->posX = room->posX + room->scaleX / 2 - player->scaleX / 2;
        player->velX = 0;
    }
    if (player->posY + player->scaleY / 2 >= room->posY + room->scaleY / 2) {
        player->posY = room->posY + room->scaleY / 2 - player->scaleY / 2;
        /*if (player->vertical_velocity > 0.f)
        {
            player->vertical_velocity = 0.0f;
        }
        player->isJumping = false;*/
    }
    if (player->posY - player->scaleY / 2 <= room->posY - room->scaleY / 2)
    {
        player->posY = room->posY - room->scaleY / 2 + player->scaleY / 2;
        player->isJumping = false;
        player->onGround = true;
    }
}

/*
    Since the ground level is no longer a strict const, but rather determined by the floor of the room, we pass in the room, and use the bottom of the room
    to determine the 'ground level'
*/
void apply_gravity_Per_Room(Player* player, Room* room, float dt) {
    if ((!player->isJumping || player->posY > (room->posY-room->scaleY/2 - player->scaleY / 2)) && !player->inWater && !player->onRope) {
        player->vertical_velocity -= player->gravity * dt;
        player->posY += player->vertical_velocity * dt;

        // Check if the player has reached the ground
        if (player->posY <= (room->posY - room->scaleY / 2 + player->scaleY / 2)) {
            player->posY = (room->posY - room->scaleY / 2 + player->scaleY / 2);
            player->vertical_velocity = 0.0;
            player->isJumping = false;
        }
    }
}

bool check_Collision_Player_Trap(Player* player, Trap* trap)
{
    f32 player_left = player->posX - player->scaleX / 2;
    f32 player_top = player->posY + player->scaleY / 2;
    f32 player_right = player->posX + player->scaleX / 2;
    f32 player_bottom = player->posY - player->scaleY / 2;

    f32 trap_left = trap->posX - trap->scaleX / 2;
    f32 trap_top = trap->posY + trap->scaleY / 2;
    f32 trap_right = trap->posX + trap->scaleX / 2;
    f32 trap_bottom = trap->posY - trap->scaleY / 2;

    return (player_left < trap_right && player_right > trap_left && player_top > trap_bottom && player_bottom < trap_top);
}



void update_Trap_Activation(Player* player, Trap* trap, float dt) {
    if (trap->deactivated)
    {
        return;
    }

    switch (trap->trapType)
    {
        case DEFAULT_SPIKE_TRAP:
        {
            if (trap->deactivated) {
                trap->active = false;
                break;
            }
            if (trap->timer > 0.f && trap->timer < trap->activationInterval)
            {
                trap->timer += dt;
                if (trap->timer >= trap->activationInterval)
                {
                    trap->active = true;
                    trap->timer = 0.f;
                }
                else
                {
                    trap->active = false;
                }
            }
            float directionMultiplier = (trap->rotation >= 3.14f) ? 1.f : -1.f;
            if (trap->active)
            {
                trap->posY = trap->originalPosY;
                trap->scaleY = trap->originalScaleY;
            }
            else
            {
                //trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
                trap->posY = trap->originalPosY + directionMultiplier *trap->originalScaleY / 4.f;
                trap->scaleY = trap->originalScaleY / 2.f;
            }
            break;
        }
		case TIMED_TRAP:
		{
			trap->timer += dt;
			if (trap->timer >= trap->activationInterval)
			{
				trap->active = !(trap->active);
				trap->timer = 0.f;
			}
            if (trap->active)
            {
                trap->posY = trap->originalPosY;
                trap->scaleY = trap->originalScaleY;
            }
            else
            {
                trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
                trap->scaleY = trap->originalScaleY / 2.f;
            }
			break;
		}
		case PROXIMITY_TRAP:
		{
            // Calculate squared distance between player and trap to avoid a costly square root.
            float dx = static_cast<float>(fabs(player->posX - trap->posX));
            float dy = static_cast<float>(fabs(player->posY - trap->posY));
            float distanceSq = dx * dx + dy * dy;
            float thresholdSq = trap->proximityDist * trap->proximityDist; // max allowed sq dist for trap to activate
			if (distanceSq <= thresholdSq)// if player is within the proximity distance
            {
                if (trap->isTriggered)
                {
                    trap->timer += dt;
                    if (trap->timer < trap->activationInterval)
                    {
                        trap->active = false;
                        trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
                        trap->scaleY = trap->originalScaleY / 2.f;
                    }
                    else
                    {
                        trap->active = true;
                        trap->posY = trap->originalPosY;
                        trap->scaleY = trap->originalScaleY;
                        trap->timer = 0.f;
                    }
                    break;
                }
                else
                {
                    trap->active = true;
                    trap->posY = trap->originalPosY;
                    trap->scaleY = trap->originalScaleY;
                    break;
                }
            }
            else
            {
				trap->active = false;
                trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
                trap->scaleY = trap->originalScaleY /2.f;
            }
            break;
		}
    }
}

void handle_Collision_Player_Trap(Player* player, Trap* trap, int *activeHotkey) {
    if (trap->deactivated || trap->active == false)
    {
        return;
    }

    
    if (trap->trapType == DEFAULT_SPIKE_TRAP)
    {
        trap->isTriggered = true;
        if (trap->active)
        {
            trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
            trap->scaleY = trap->originalScaleY / 2.f;
            if (player->currhealth <= 0.f)
            {
                player->currhealth = 0.f;
            }
            else if(*activeHotkey != 1)
            {
                player->currhealth -= 100.f;
            }
            trap->active = false;
            trap->timer = 0.5f;
        }
    }
    else if (trap->trapType == TIMED_TRAP)
    {
        trap->isTriggered = true;
        if (trap->active)
        {
            trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
            trap->scaleY = trap->originalScaleY / 2.f;
            if (player->currhealth <= 0.f)
            {
                player->currhealth = 0.f;
            }
            else if (*activeHotkey != 1)
            {
                player->currhealth -= 100.f;
            }
			trap->active = false;
			trap->timer = 0.5f;
        }
    }
    if (trap->trapType == PROXIMITY_TRAP && check_Collision_Player_Trap(player, trap))
    {
        trap->isTriggered = true;
        if (trap->active)
        {
            trap->posY = trap->originalPosY - trap->originalScaleY / 4.f;
            trap->scaleY = trap->originalScaleY / 2.f;
            if (player->currhealth <= 0.f)
            {
                player->currhealth = 0.f;
            }
            else if (*activeHotkey != 1)
            {
                player->currhealth -= 100.f;
            }
            trap->active = false;
        }
        else if (trap->trapType == PROXIMITY_TRAP)
        {
			trap->isTriggered = false;
        }
    }
    if (check_Collision_Player_Trap(player, trap))
    {
        // Calculate overlap along the X-axis
        float overlapX = fminf((player->posX + player->scaleX / 2) - (trap->posX - trap->scaleX / 2),
            (trap->posX + trap->scaleX / 2) - (player->posX - player->scaleX / 2));

        // Calculate overlap along the Y-axis
        float overlapY = fminf((player->posY + player->scaleY / 2) - (trap->posY - trap->scaleY / 2),
            (trap->posY + trap->scaleY / 2) - (player->posY - player->scaleY / 2));

        if (overlapX < overlapY) {
            // Horizontal collision
            if (player->posX < trap->posX) {
                player->posX -= overlapX;  // Collision from the left of the platform
            }
            else {
                player->posX += overlapX;  // Collision from the right of the platform
            }
        }
        else {
            // Vertical collision
            if (player->posY < trap->posY) { // Collision from below the platform
                player->posY -= overlapY;
                player->vertical_velocity = 0;  // Stop downward movement
            }
            else { // Collision from above the platform
                player->posY += overlapY;
                player->vertical_velocity = 0;  // Stop upward movement
                player->isJumping = false; // End jumping state
            }
        }
    }
}

void update_Collision_Player_Trap(Player* player, Level levels[], int *currentLevel, int* activeHotkey, float dt) {
    for (int i = *currentLevel; i < *currentLevel + 2; i++) {  // Check traps in current & next level
        for (int trapIndex = 0; trapIndex < levels[i].trapCount; trapIndex++) {
            if (levels[i].trapCount > 0) {  // Ensure traps exist in the level
                // Get a pointer to the current trap for easier access.
                Trap* currentTrap = &levels[i].traps[trapIndex];

                // First update the trap's active state for timed and proximity traps.
                update_Trap_Activation(player, currentTrap, dt);

                bool trapCollision = check_Collision_Player_Trap(player, &levels[i].traps[trapIndex]);

                if (trapCollision || (currentTrap->trapType == PROXIMITY_TRAP && currentTrap->active)) {
                    /*
                     * Collision between player and trap
                     */
                    handle_Collision_Player_Trap(player, &levels[i].traps[trapIndex], activeHotkey);
                }
                else if (currentTrap->trapType == DEFAULT_SPIKE_TRAP || currentTrap->trapType == PROXIMITY_TRAP || currentTrap->trapType == TIMED_TRAP) {  // No collision, reset trap
                    if (levels[i].traps[trapIndex].isTriggered) {
                        // Reset trap properties
                        levels[i].traps[trapIndex].isTriggered = false;
                    }
                }
            }
        }
    }
}


bool check_Collision_Player_Trap_Switch(Player* player, TrapSwitch* trapSwitch)
{
    f32 player_left = player->posX - player->scaleX / 2;
    f32 player_top = player->posY + player->scaleY / 2;
    f32 player_right = player->posX + player->scaleX / 2;
    f32 player_bottom = player->posY - player->scaleY / 2;

    f32 trapSwitch_left = trapSwitch->posX - trapSwitch->scaleX / 2;
    f32 trapSwitch_top = trapSwitch->posY + trapSwitch->scaleY / 2;
    f32 trapSwitch_right = trapSwitch->posX + trapSwitch->scaleX / 2;
    f32 trapSwitch_bottom = trapSwitch->posY - trapSwitch->scaleY / 2;

    return (player_left < trapSwitch_right && player_right > trapSwitch_left && player_top > trapSwitch_bottom && player_bottom < trapSwitch_top);
}

void handle_Collision_Player_Trap_Switch( TrapSwitch* trapSwitch, Level* level, float dt) {
    // Update the cooldown timer each frame.
    if (trapSwitch->cooldown > 0.0f) {
        trapSwitch->cooldown -= dt;
        if (trapSwitch->cooldown < 0.0f)
            trapSwitch->cooldown = 0.0f;
    }

    // Check if the player pressed the interact key and the switch cooldown is 0, similar to lvl implementation
    if (AEInputCheckCurr(AEVK_E) && trapSwitch->cooldown <= 0.0f) {
        // Toggle the switch's state.
        trapSwitch->isTriggered = !trapSwitch->isTriggered;
        // Set the cooldown
        trapSwitch->cooldown = 1.f;

        // Iterate through the traps affected by this switch.
        for (int i = 0; i < trapSwitch->numTrapsAffected; i++) {
            int trapIndex = trapSwitch->trapIndices[i];
            // Ensure the index is valid.
            if (trapIndex != -1 && trapIndex < level->trapCount) {
                // Toggle the trap's active state based on the switch state.
                level->traps[trapIndex].deactivated = trapSwitch->isTriggered;
            }
        }
    }
}

void update_Collision_Player_Trap_Switch(Player* player, Level levels[], int *currentLevel, float dt) {
    for (int i = *currentLevel; i < *currentLevel + 2; i++) {  // Check trap switches in current & next level
        if (levels[i].trapSwitchCount > 0) {  // Ensure trap switches exist in the level
            for (int trapSwitchIndex = 0; trapSwitchIndex < levels[i].trapSwitchCount; trapSwitchIndex++) {
                bool trapSwitchCollision = check_Collision_Player_Trap_Switch(player, &levels[i].trapSwitches[trapSwitchIndex]);

                if (trapSwitchCollision) {
                    /*
                     * Collision between player and trap switch
                     */
                    handle_Collision_Player_Trap_Switch( &levels[i].trapSwitches[trapSwitchIndex], &levels[i], dt);
                }
            }
        }
    }
}

bool check_Collision_Player_Water_For_Jump(Player* player, Room* room)
{
    bool landedOnWater = false;
    float highestWaterTop = groundLevel;
    bool foundWater = false;
    for (int i = 0; i < room->waterCount; i++)
    {
        Water* waterBlock = &room->water[i];

        float player_left = player->posX - player->scaleX / 2.f;
        float player_right = player->posX + player->scaleX / 2.f;
        float water_right = waterBlock->posX + waterBlock->scaleX / 2.f;
        float water_left = waterBlock->posX - waterBlock->scaleX / 2.f;
        float water_top = waterBlock->posY + waterBlock->scaleY / 2.f;

        if (player_left > water_right || player_right < water_left || waterBlock->scaleY <= 0.f)
        {
            continue;
        }

        if (player->posY <= water_top)
        {
            if (water_top > highestWaterTop)
            {
                highestWaterTop = water_top; // Set to highest possible water Top
                foundWater = true;
            }
        }
    }
    if (foundWater)
    {
        player->posY = highestWaterTop;
        landedOnWater = true;
    }
    return landedOnWater;
}


// Returns the highest water top that overlaps horizontally with the player.
float get_Highest_Water_Top_Above_Player(const Player* player, const Room* room)
{
    float playerLeft = player->posX - (player->scaleX / 2.0f);
    float playerRight = player->posX + (player->scaleX / 2.0f);

    float highestWaterTop = groundLevel;
    for (int i = 0; i < room->waterCount; i++)
    {
        const Water* waterBlock = &room->water[i];
        if (waterBlock->scaleY < 1.f)
        {
            continue;
        }
        //horizontal check
        float wLeft = waterBlock->posX - (waterBlock->scaleX / 2.0f);
        float wRight = waterBlock->posX + (waterBlock->scaleX / 2.0f);
        float wTop = waterBlock->posY + (waterBlock->scaleY / 2.0f);
        if (playerRight < wLeft || playerLeft > wRight)
        {
            continue;
        }
        if (player->posY <= wTop && wTop > highestWaterTop)
        {
            highestWaterTop = wTop;
        }
    }
    return highestWaterTop;
}