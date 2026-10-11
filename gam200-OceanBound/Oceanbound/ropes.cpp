/* Start Header ************************************************************************/
/*!
\file       ropes.cpp
\project    Ocean Bound
\author     Kye Le, Jethro, Reginald
\date       April, 06, 2025
\brief      This file initializes rope placements across multiple levels and sets their properties for
            interaction mechanics. Ropes are used for vertical navigation, and this setup defines their
            position, size, and level association.

            Ropes Initialization and Level Allocation -> Reginald
            Logic -> Kye Le, Jethro

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "ropes.h"

void initializeRopes(Rope ropes[]) {
    //level 1
    {
        //note, for numof affected rooms per level, we are checking the 1st level, then 2nd level.
        ropes[0] = { 20.0f, 320.0f, -440.0f, 287.0f, 0.f, GS_LEVEL5 }; // For levels with no rooms affected by said lever, use -1 to avoid 0 null conflict for c
        ropes[1] = { 20.0f, 160.0f, 125.0f, 137.0f, 0.f, GS_LEVEL5 };
        ropes[2] = { 20.0f, 160.0f, 25.0f, 137.0f, 0.f, GS_LEVEL5 };
        ropes[3] = { 20.0f, 220.0f, -460.0f, -113.0f, 0.f, GS_LEVEL6 };
        ropes[4] = { 20.0f, 160.0f, -385.0f, -83.0f, 0.f, GS_LEVEL6 };
        ropes[5] = { 20.0f, 220.0f, -310.0f, -113.0f, 0.f, GS_LEVEL6 };
        ropes[6] = { 20.0f, 160.0f, -235.0f, -83.0f, 0.f, GS_LEVEL6 };
        ropes[7] = { 20.0f, 220.0f, -160.0f, -113.0f, 0.f, GS_LEVEL6 };
        ropes[8] = { 20.0f, 160.0f, -85.0f, -83.0f, 0.f, GS_LEVEL6 };
        ropes[9] = { 20.0f, 220.0f, -10.0f, -113.0f, 0.f, GS_LEVEL6 };
        ropes[10] = { 20.0f, 160.0f, 65.0f, -83.0f, 0.f, GS_LEVEL6 };
        ropes[11] = { 20.0f, 220.0f, 140.0f, -113.0f, 0.f, GS_LEVEL6 };
        ropes[12] = { 20.0f, 160.0f, 215.0f, -83.0f, 0.f, GS_LEVEL6 };
        ropes[13] = { 20.0f, 350.0f, -580.0f, 275.0f, 0.f, GS_LEVEL7 };
        ropes[14] = { 20.0f, 250.0f, -105.0f, 322.0f, 0.f, GS_LEVEL7 };
        ropes[15] = { 20.0f, 220.0f, -360.0f, -113.0f, 0.f, GS_LEVEL8 };
        ropes[16] = { 20.0f, 220.0f, -460.0f, -113.0f, 0.f, GS_LEVEL8 };
        ropes[17] = { 20.0f, 220.0f, -160.0f, -113.0f, 0.f, GS_LEVEL8 };
        ropes[18] = { 20.0f, 220.0f, -260.0f, -113.0f, 0.f, GS_LEVEL8 };
        ropes[19] = { 20.0f, 220.0f, 290.0f, -113.0f, 0.f, GS_LEVEL6 };
    }
}