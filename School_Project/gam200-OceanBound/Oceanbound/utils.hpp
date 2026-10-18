/* Start Header ************************************************************************/
/*!
\file       utils.hpp
\project    Ocean Bound
\author     Kye Le, Jethro
\date       April 06, 2025
\brief      Utility functions and helpers for rendering, input checks, and text calculations.

            This file contains various reusable utility functions:
            - Mesh creation utilities for drawing squares and platform segments.
            - Generic rendering functions that support both textured and untextured drawing.
            - Mouse input utilities for cursor collision detection.
            - Text scaling utility to help dynamically fit text within a bounding box.

            These functions help abstract rendering and input logic to simplify usage
            across various game systems such as UI, HUD elements, and level components.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef UTILS_HPP
#define UTILS_HPP

#include "AEEngine.h"
#include <math.h>
#include <string>

/*
*	Function to create mesh, one uses 1 color, while the other uses 2
*/
AEGfxVertexList* CreateSquareMesh(uint32_t color);
AEGfxVertexList* CreateSquareMesh(uint32_t color, uint32_t color1);
AEGfxVertexList* PlatformMeshTop(uint32_t color);
AEGfxVertexList* PlatformMeshBottom(uint32_t color);

/*
*	These two functions are to enable the compression of the drawing of the meshes when rendering, one for those with rotation, and one for those without
*/
void mesh_Draw(float rotation, float scaleX, float scaleY, float posX, float posY, AEGfxVertexList* color);
void mesh_Draw_No_Rotate(float scaleX, float scaleY, float posX, float posY, AEGfxVertexList* color);
void mesh_Draw_With_Texture(float rotation, float scaleX, float scaleY, float posX, float posY, AEGfxVertexList* color, AEGfxTexture* texture);

bool checkCursorCollisionWithRectangle(float centerX, float centerY, float width, float height);

float computeTextScale(const std::string& text, float maxWidth, float baseCharWidth = 0.05f);
#endif
