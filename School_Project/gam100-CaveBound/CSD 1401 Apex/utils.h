/*
@file       this is file utils.h
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    projCavebound
@date       20/10/24
@brief      this file contains a collection of functions to be called, by declaring utils.h, other source files will be able to call upon the the functions in
            utils.h to be used. Furthermore, the definitions of the functions can be assigned via declaring utils.h in other source files, eg. utils.c.
*/
//#pragma once
#ifndef _UTILS_H_
#define _UTILS_H_
#include "cprocessing.h"
#include <stdbool.h>
//#include "enemies.h"
//  @IsAreaClicked:	When IsAreaClicked is called, it takes in the x and y coordinates of the area, as well as the width, height and the values of the clicked location, the clicked location of which is set to 0.0.
//					With this, it first sets the current mouse x and y location to click x and click y respectively. Then it calculates if the area within the area has been clicked or not by checking if click x
//					is greater than the lowest x of the area and lesser than the biggest x of the area, followed by the same for y. If all conditions are met, it returns a 1 to represent that the area has been
//					clicked, else it returns a 0 to represent that none of the area were clicked.
//  @param:         IsAreaClicked takes in a param of multiple floats.
//  @return:        IsAreaClicked returns an integer
int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y);

//  @IsCircleClicked:	When IsCircleClicked is called, it takes in the x and y coordinates of the circle, as well as the diameter and the values of the clicked location, the clicked location of which is set to 0.0.
//						With this, it first sets the current mouse x and y location to click x and click y respectively. Then it calculates if the area within the circle has been clicked or not by checking if click x
//						is greater than the lowest x of the circle and lesser than the biggest x of the circle, followed by the same for y. If all conditions are met, it returns a 1 to represent that the circle has been
//						clicked, else it returns a 0 to represent that none of the circles were clicked.
//  @param:				IsCircleClicked takes in a param of multiple floats.
//  @return:			IsCircleClicked returns an integer
int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y);

float squareThis(float numberToBeSquared);

float health_Bar_Calculate(float currentHealthOfPlayer, float maxHealthOfPlayer, float visualizedHealthBar);

void create_Rect(float x, float y, float width, float height, CP_Color rectcolour);

void updateCooldownTimer(int* activeTriangles, float CP_x, float CP_y, float vertices[5][2], CP_Color colour, double* lastCharacterSwitchTime, bool* characterSwitched);

float damage_Calculation(int character, float initialDamage, int DamageType);

#endif _UTILS_H_