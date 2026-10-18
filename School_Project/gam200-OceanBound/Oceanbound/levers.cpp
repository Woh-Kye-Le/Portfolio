/* Start Header ************************************************************************/
/*!
\file       levers.cpp
\project    Ocean Bound
\author     Jethro Sung, Reginald Lew
\date       April, 06, 2025
\brief      This file initializes an array of Lever structs with predefined positions, orientations, 
            and level-specific configurations. Each lever stores data about which rooms and water blocks it affects, 
            enabling environmental changes when activated during gameplay.
            Logic-Jethro
            Implementation/Intialization Of Levers: Reginald

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "levers.h"
#include "pch.h"

void initializeLevers(Lever levers[], int numLevers) {
    //level 1
    {
        //note, for numof affected rooms per level, we are checking the 1st level, then 2nd level.
        levers[0] = { 25.0f, 50.0f, -250.0f, -425.0f, 0.f, 0, false, { { -1 } , { 0 }}, {0,1} , LEVER_TIDAL,{0} }; // For levels with no rooms affected by said lever, use -1 to avoid 0 null conflict for c
        levers[1] = { 25.0f, 50.0f, -500.0f, -425.0f, 0.f, 2,false, { { 0 } , { -1 }}, {1} , LEVER_TIDAL, {0,1} }; // lvl 3
        levers[2] = { 25.0f, 50.0f, -385.0f, -15.0f, 0.f, 3,false, { { -1 } , { 1,-1 }}, {0, 1} , LEVER_WATER_FLOW,{0} }; // lvl 4
        levers[3] = { 25.0f, 50.0f, -80.0f, -145.0f, 0.f, 3,false, { { -1 } , { 2,-1 }}, {0, 1} , LEVER_WATER_FLOW, {0} }; // lvl 4
        levers[4] = { 25.0f, 50.0f, -380.0f, -425.0f, 0.f, 3,false, { { -1 } , { 3,-1 }}, {0, 1} , LEVER_WATER_FLOW,{0} }; // lvl 4
        levers[5] = { 25.0f, 50.0f, -580.0f, 180.0f, 0.f, 4,true, { { 0 } , { -1 }}, {1} , LEVER_TIDAL,{0} }; // lvl 5 room[0] self room
        levers[6] = { 25.0f, 50.0f, -250.0f, 420.0f, 3.14f, 4,false, { { 1,-1 } , { -1 }}, {1} , LEVER_WATER_FLOW,{1, -1} }; // lvl 5 room[1] affects lvl 5 room[1] water block [1] 
        levers[7] = { 25.0f, 50.0f, -320.0f, 30.0f, 0.0f, 4,false, { { 1, -1 } , { -1 }}, {1} , LEVER_WATER_FLOW,{3, -1} }; // lvl 5 room[1] affects lvl 5 room[1] water block [3]
        levers[8] = { 25.0f, 50.0f, 770.0f, 400.0f, 1.57f, 4,false, { { 1, -1 } , { -1 }}, {1} , LEVER_WATER_FLOW,{4, -1} }; // lvl 5 room[1] affects lvl 5 room[1] water block [4]
        levers[9] = { 25.0f, 50.0f, -700.0f, 420.0f, 3.14f, 6,false, { { 1, -1} , { -1 }}, {1} , LEVER_WATER_FLOW,{0, -1} }; // lvl 7 room[0] affects lvl 7 room[1] water block[4]
        levers[10] = { 25.0f, 50.0f, -590.0f, 30.0f, 0.f, 6,false, { { 0, -1} , { -1 }}, {2} , LEVER_WATER_FLOW,{1 , 2, -1} }; // lvl 7 room[0] affects lvl 7 room[0] water block[2 && 3]
        //levers[11] = { 25.0f, 50.0f, -373.0f, 400.0f, 4.71f, 6,false, { { 1, -1} , { -1 }}, {1} , LEVER_WATER_FLOW,{2, -1} }; // lvl 7 room[0] affects lvl 7 room[0] water block[4]
        levers[11] = { 25.0f, 50.0f, -373.0f, 400.0f, 4.71f, 6,false, { { 0, -1} , { -1 }}, {1} , LEVER_WATER_FLOW,{3, -1} };
    }
}