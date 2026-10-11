/* Start Header ************************************************************************/
/*!
\file       utils.cpp
\project    Ocean Bound
\author     Kye Le, Jethro
\date       April 06, 2025
\brief      This file provides various utility functions used throughout the game,
			including mesh creation (color and texture), transformation and drawing logic,
			cursor collision detection, and dynamic text scaling support for UI elements.

			Mesh Drawing / Cursor Collision -> Kye Le
			Text Scaling/ Mesh Drawing -> Jethro

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "AEEngine.h"
#include <math.h>
#include "objects.hpp"
#include "pch.h"
#include "utils.hpp"

AEGfxVertexList* CreateSquareMesh(uint32_t color) {
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, color, 0.0f, 1.0f,
		0.5f, -0.5f, color, 1.0f, 1.0f,
		-0.5f, 0.5f, color, 0.0f, 0.0f);

	AEGfxTriAdd(
		0.5f, -0.5f, color, 1.0f, 1.0f,
		0.5f, 0.5f, color, 1.0f, 0.0f,
		-0.5f, 0.5f, color, 0.0f, 0.0f);
	return AEGfxMeshEnd();
}

AEGfxVertexList* CreateSquareMesh(uint32_t color, uint32_t color1) {
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, color, 0.0f, 1.0f,
		0.5f, -0.5f, color1, 1.0f, 1.0f,
		-0.5f, 0.5f, color, 0.0f, 0.0f);

	AEGfxTriAdd(
		0.5f, -0.5f, color, 1.0f, 1.0f,
		0.5f, 0.5f, color1, 1.0f, 0.0f,
		-0.5f, 0.5f, color, 0.0f, 0.0f);
	return AEGfxMeshEnd();
}

AEGfxVertexList* PlatformMeshTop(uint32_t color) {//returns 1/5 of top of texture platform
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, color, 0.5f, 0.2f,
		0.5f, -0.5f, color, 0.6f, 0.2f,
		-0.5f, 0.5f, color, 0.5f, 0.0f);

	AEGfxTriAdd(
		0.5f, -0.5f, color, 0.6f, 0.2f,
		0.5f, 0.5f, color, 0.6f, 0.0f,
		-0.5f, 0.5f, color, 0.5f, 0.0f);
	return AEGfxMeshEnd();
}

AEGfxVertexList* PlatformMeshBottom(uint32_t color) {//returns 4/5 of bottom of texture platform
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, color, 0.0f, 1.0f,
		0.5f, -0.5f, color, 1.0f, 1.0f,
		-0.5f, 0.5f, color, 0.0f, 0.2f);

	AEGfxTriAdd(
		0.5f, -0.5f, color, 1.0f, 1.0f,
		0.5f, 0.5f, color, 1.0f, 0.2f,
		-0.5f, 0.5f, color, 0.0f, 0.2f);
	return AEGfxMeshEnd();
}



void mesh_Draw(float rotation, float scaleX, float scaleY, float posX, float posY, AEGfxVertexList* color) {
	AEMtx33 transformation;
	AEMtx33Identity(&transformation);
	AEMtx33 orbit;
	AEMtx33Rot(&orbit, rotation);
	AEMtx33 scale;
	AEMtx33Scale(&scale, scaleX, scaleY);
	AEMtx33 translate;
	AEMtx33Trans(&translate, posX, posY);

	AEMtx33Concat(&transformation, &orbit, &scale);
	AEMtx33Concat(&transformation, &translate, &transformation);

	AEGfxSetTransform(transformation.m);
	AEGfxMeshDraw(color, AE_GFX_MDM_TRIANGLES);
}

void mesh_Draw_No_Rotate(float scaleX, float scaleY, float posX, float posY, AEGfxVertexList* color) {
	AEMtx33 transformation;
	AEMtx33Identity(&transformation);
	AEMtx33 scale;
	AEMtx33Scale(&scale, scaleX, scaleY);
	AEMtx33 translate;
	AEMtx33Trans(&translate, posX, posY);

	AEMtx33Concat(&transformation, &translate, &scale);

	AEGfxSetTransform(transformation.m);
	AEGfxMeshDraw(color, AE_GFX_MDM_TRIANGLES);
}

void mesh_Draw_With_Texture(float rotation, float scaleX, float scaleY, float posX, float posY, AEGfxVertexList* color, AEGfxTexture* texture) {
	// Tell the engine to get ready to draw something with texture.
	AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);

	// Set the the color to multiply to white, so that the sprite can 
	// display the full range of colors (default is black).
	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);

	// Set the color to add to nothing, so that we don't alter the sprite's color
	AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);
	AEGfxSetTransparency(1.0f);
	AEGfxTextureSet(texture, 0, 0);
	AEMtx33 transformation;
	AEMtx33Identity(&transformation);
	AEMtx33 orbit;
	AEMtx33Rot(&orbit, rotation);
	AEMtx33 scale;
	AEMtx33Scale(&scale, scaleX, scaleY);
	AEMtx33 translate;
	AEMtx33Trans(&translate, posX, posY);

	AEMtx33Concat(&transformation, &orbit, &scale);
	AEMtx33Concat(&transformation, &translate, &transformation);

	AEGfxSetTransform(transformation.m);
	AEGfxMeshDraw(color, AE_GFX_MDM_TRIANGLES);

	// Tell the engine to get ready to draw something with texture.
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
}

// Function to check cursor collision with a given rectangle/ square
bool checkCursorCollisionWithRectangle(float centerX, float centerY, float width, float height) {
	s32 cursorX, cursorY;
	AEInputGetCursorPosition(&cursorX, &cursorY);
	f32 cursorX1 = static_cast<f32> (cursorX);
	f32 cursorY1 = static_cast<f32> (cursorY);

	// Calculate the top-left and bottom-right corners of the rectangle based on its center position
	float leftX = centerX - width / 2;
	float rightX = centerX + width / 2;
	float topY = centerY + height / 2;
	float bottomY = centerY - height / 2;

	// Check if the cursor is within the rectangle
	if (cursorX1 >= leftX && cursorX1 <= rightX && cursorY1 <= topY && cursorY1 >= bottomY) {
		return true;
	}
	else {
		return false;
	}
}

float computeTextScale(const std::string& text, float maxWidth, float baseCharWidth) {
	if (text.empty())
		return 1.0f;
	return maxWidth / (text.length() * baseCharWidth);
}