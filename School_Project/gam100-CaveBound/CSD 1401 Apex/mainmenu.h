/*
@file       this is file mainmenu.h
@author     Jethro Sung (sung.h)
@course     CSD1401
@section    a
@project    projCavebound
@date       21/10/24
@brief      this file contains a collection of functions to be called, by declaring mainmenu.h, other source files will be able to call upon the the functions in
            mainmenu.h to be used. Furthermore, the definitions of the functions can be assigned via declaring mainmenu.h in other source files, eg. mainmenu.c.
*/

//#pragma once
#ifndef _MAIN_MENU_H_
#define _MAIN_MENU_H_

/*
    The function below provides the declaration of a function Main_Menu_Init. function Main_Menu_Init takes in and outputs void. It initializes the main menu of the game state.
*/
void Main_Menu_Init(void);
/*
    The function below provides the declaration of a function Main_Menu_Update. function Main_Menu_Update takes in and outputs void. It updates the main menu of the game state.
*/
void Main_Menu_Update(void);
/*
    The function below provides the declaration of a function Main_Menu_Exit. function Main_Menu_Exit takes in and outputs void. It exits the main menu of the game state.
*/
void Main_Menu_Exit(void);
#endif _MAIN_MENU_H_