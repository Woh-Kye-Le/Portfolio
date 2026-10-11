/*
@file       instructions.h
@author     Celest Chang (c.qixuancelest)
@course     CSD1401
@section    a
@project    projCavebound
@date       11/11/24
@brief      This file contains the function declaration of instructions_init, instructions_update and instructions_exit.
It will allow other files to reference the instructions page and call upon the functions.
*/

#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

/*
    The function declaration of instructions_init returns void and takes in void. The function initialises the game state of instructions.
*/
void instructions_init(void);
/*
    The function declaration of instructions_update returns void and takes in void. The function updates the game state of instructions.
*/
void instructions_update(void);
/*
    The function declaration of instructions_exit returns void and takes in void. The function exits the game state of instructions.
*/
void instructions_exit(void);

#endif
