/* Start Header ************************************************************************/
/*!
\file       health.cpp
\project    Ocean Bound
\author     Jethro Sung
\date       April, 06, 2025
\brief      Jethro - drawHealthBar
			This file implements the drawing and updating of the health bar for the game	

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "health.h"
#include "player.h"
#include "pch.h"

void drawHealthBar(HealthBar* hb, const Player* player) {
	f32 healthRatio = player->currhealth/player->originalhealth;
	hb->scaleX = hb->originalScaleX * healthRatio;
	hb->originalPosX = player->posX;
	hb->posX = player->posX - (hb->originalScaleX - hb->scaleX) / 2;
	hb->posY = player->posY + player->scaleY / 2 + 32.0f;

	if (player->isCrouching)
	{
		//ensures oxygen level and bg is always above player	
		hb->posY = player->posY + player->scaleY / 2 + 40.0f;
	}

}