// HealthBar.h
#pragma once
/* Start Header ************************************************************************/
/*!
\file       health.h
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This header defines the HealthBar struct and declares its associated rendering function.

            The HealthBar structure holds positional and scaling attributes used to visually represent
            the player's current health status in the UI. This module allows the health bar to be rendered
            dynamically in response to gameplay changes such as damage, healing, or special effects.

            The `drawHealthBar` function is responsible for displaying the current state of the health bar
            relative to the player's health, based on real-time updates.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/


#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"

struct HealthBar {
    f32 posX, posY;
    f32 scaleX, scaleY;
    f32 originalPosX, originalScaleX;
    // Optionally store colors, textures, etc.
};

struct Player;

void drawHealthBar(HealthBar* hb, const Player* player);

