/*
@file       this is file utls.c
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    projCavebound
@date       10/2/24
@brief      this file contains a collection of definitions of a series of functions in utls.h, by including utls.h in the file, and utls.h in game.c,
			game.c is able to call on the functions in utls.h, which processes the input according to the definitions in utls.c
*/
#include <math.h>// is included so that various mathematical functions can be defined
#include "cprocessing.h"//To be able to access the CProcessing functions
#include "game.h"//To be able to access the functions in game.h
#include "mainmenu.h"//To be able to access the functions in mainmenu.h
#include "utils.h"//To be able to access the functions in utils.h
#include <stdio.h>

//  @IsAreaClicked:	When IsAreaClicked is called, it takes in the x and y coordinates of the area, as well as the width, height and the values of the clicked location, the clicked location of which is set to 0.0.
//					With this, it first sets the current mouse x and y location to click x and click y respectively. Then it calculates if the area within the area has been clicked or not by checking if click x
//					is greater than the lowest x of the area and lesser than the biggest x of the area, followed by the same for y. If all conditions are met, it returns a 1 to represent that the area has been
//					clicked, else it returns a 0 to represent that none of the area were clicked.
//  @param:         IsAreaClicked takes in a param of multiple floats.
//  @return:        IsAreaClicked returns an integer
int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y)
{
	click_x = CP_Input_GetMouseX();
	click_y = CP_Input_GetMouseY();
	if (click_x >= (area_center_x - area_width / 2) && click_x <= (area_center_x + area_width / 2) && click_y >= (area_center_y - area_height / 2) && click_y <= (area_center_y + area_height / 2))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
//  @IsCircleClicked:	When IsCircleClicked is called, it takes in the x and y coordinates of the circle, as well as the diameter and the values of the clicked location, the clicked location of which is set to 0.0.
//						With this, it first sets the current mouse x and y location to click x and click y respectively. Then it calculates if the area within the circle has been clicked or not by checking if click x
//						is greater than the lowest x of the circle and lesser than the biggest x of the circle, followed by the same for y. If all conditions are met, it returns a 1 to represent that the circle has been
//						clicked, else it returns a 0 to represent that none of the circles were clicked.
//  @param:				IsCircleClicked takes in a param of multiple floats.
//  @return:			IsCircleClicked returns an integer
int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y)
{
	click_x = CP_Input_GetMouseX();
	click_y = CP_Input_GetMouseY();
	if (click_x > circle_center_x - diameter / 2 && click_x < circle_center_x + diameter / 2 && click_y > circle_center_y - diameter / 2 && circle_center_y < circle_center_y + diameter / 2)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

float squareThis(float numberToBeSquared)
{
	return numberToBeSquared * numberToBeSquared;
}
/*
	*Done By: Jethro
	* Basically calculates the current health of the player and ratios it to their original max health
*/
float health_Bar_Calculate(float currentHealthOfPlayer, float maxHealthOfPlayer, float visualizedHealthBar)
{
	return currentHealthOfPlayer / maxHealthOfPlayer * visualizedHealthBar;
}
/*
	*Done By: Jethro
	* Basically draws a rect with the given values.
*/
void create_Rect(float x, float y, float width, float height, CP_Color rectColour) {
	CP_Settings_Fill(rectColour);
	CP_Graphics_DrawRect(x, y, width, height);
}

/*
* Done by: Kye Le
* this function updates the the active number of triangle needed for the visual cooldown timer. this function starts by checking if characterSwitched is true, this indicates 
* that the cooldown is active. currentTime retrieves the current time in seconds and elapsed calculate the time that has passed since the cooldown started. based on the elapsed 
* time if its less than 1 sec, all 5 triangle will still be active, if less than 2 seconds, 4 will be active, and it decrements accordingly until elapsed is more than equal to 5 
* all triangles are inactive and characterSwitched flag is reset to false ending the cooldown.
* to draw the triangles, a loop is iterates through the active triangles. for each triangle, The nextIndex connects the current vertex to the next one in the array (wrapping back to the first vertex when necessary).
*/
void updateCooldownTimer(int* activeTriangles, float CP_x, float CP_y, float vertices[5][2], CP_Color colour, double* lastCharacterSwitchTime, bool* characterSwitched) {
	if (*characterSwitched) {
		float currentTime = CP_System_GetSeconds();
		double elapsed = currentTime - *lastCharacterSwitchTime;
		if (elapsed < 1) {
			*activeTriangles = 5;
		}
		else if (elapsed < 2) {
			*activeTriangles = 4;
		}
		else if (elapsed < 3) {
			*activeTriangles = 3;
		}
		else if (elapsed < 4) {
			*activeTriangles = 2;
		}
		else if (elapsed < 5) {
			*activeTriangles = 1;
		}
		else {
			*activeTriangles = 0;
			*characterSwitched = false; // End cooldown, reset the flag
		}
	}
	// Drawing the triangles based on activeTriangles
	for (int i = 0; i < *activeTriangles; i++) {
		int nextIndex = (i + 1) % 5;
		CP_Settings_Fill(colour);
		CP_Graphics_DrawTriangle(CP_x, CP_y, vertices[i][0], vertices[i][1], vertices[nextIndex][0], vertices[nextIndex][1]);
	}
}
/*
	* Done By: Jethro
	* This function basically takes in the currentCharIndex, the initial damge to be dealt to the player, and the type of damage.
	* Based on the current char index and the type of damage, we switch the the various cases in which a modifier would be applied to the damage dealt to the player.
*/
float damage_Calculation(int character, float initialDamage, int DamageType)
{
	if (0 == character)
	{
		switch (DamageType)
		{
		case 1:
			return initialDamage * 0.5f;
			break;
		case 2:
			return initialDamage * 1.5f;
			break;
		case 3:
			return initialDamage * 0.5f;
			break;
		case 4:
			return initialDamage * 1.5f;
			break;
		case 5:
			return initialDamage * 0.5f;
			break;
		case 6:
			return initialDamage * 1.5f;
			break;
		default:
			return initialDamage;
			break;
		}
	}
	else if (1 == character)
	{
		switch (DamageType)
		{
		case 1:
			return initialDamage * 1.5f;
			break;
		case 2:
			return initialDamage * 0.5f;
			break;
		case 3:
			return initialDamage * 1.5f;
			break;
		case 4:
			return initialDamage * 0.5f;
			break;
		case 5:
			return initialDamage * 1.5f;
			break;
		case 6:
			return initialDamage * 0.5f;
			break;
		default:
			return initialDamage;
			break;
		}
	}
	return initialDamage;
}