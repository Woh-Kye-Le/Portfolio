#include "cprocessing.h"
//#include "mainmenu.h"
#include "game.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
//#include "points.h"




CP_Image gemImage;

Gem gems[MAX_GEMS];

int playerPoints = 0; //score of player 
/*
    *Done By Jia Hui
    *This is to initialize the initial points of the players
*/
void initialisingPointSystem() {
    playerPoints = 0;
}
/*
    *Done By Jia Hui
    *This basically draws the continuosly updating points in real time while the player is playing the game.
*/
void drawPoints(int PlayerPoints) {

    gemImage = CP_Image_Load("./Assets/GEM2.png");

    CP_Image_Draw(gemImage, 50, 55, 30, 55, 255);
    char score[10];
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    snprintf(score, sizeof(score), "%d", PlayerPoints);
    CP_Font_DrawText(score, 80, 70);
}
/*
    *Done By Jia Hui
    *This basically initializes the locations for the gems in level 1
    * In order to prevent conflict with level 2, all gems in level 2 are assigned to collected as long as their index are in the range
    * of the gems initialized in level 2.
*/
void intialising_Gems() {
    gemImage = CP_Image_Load("./Assets/GEM2.png");  // Load the image first

    for (int i = 0; i < MAX_GEMS; i++) {
        gems[i].isCollected = false;  // initially no gem is collected
    }

    /*if (level == 1) {*/
    gems[0] = (Gem){ 515, 545, 30, 50, false, 30, 15 };
    gems[1] = (Gem){ 580, 545, 30, 50, false, 30, 15 };
    gems[2] = (Gem){ 645, 545, 30, 50, false, 30, 15 };
    gems[3] = (Gem){ 710, 545, 30, 50, false, 30, 15 };
    gems[4] = (Gem){ 565, 810, 30, 50, false, 30, 15 }; //gems are 65 x cords apart
    gems[5] = (Gem){ 630, 810, 30, 50, false, 30, 15 };
    gems[6] = (Gem){ 1051, 810, 30, 50, false, 30, 15 };
    gems[7] = (Gem){ 1116, 810, 30, 50, false, 30, 15 };
    gems[8] = (Gem){ 1181, 810, 30, 50, false, 30, 15 };
    gems[9] = (Gem){ 1246, 810, 30, 50, false, 30, 15 };
    gems[10] = (Gem){ 255, 610, 30, 50, false, 30, 15 };
    gems[11] = (Gem){ 320, 610, 30, 50, false, 30, 15 };
    gems[12] = (Gem){ 1074, 473, 30, 50, false, 30, 15 };
    gems[13] = (Gem){ 1139, 473, 30, 50, false, 30, 15 };
    gems[14] = (Gem){ 1204, 473, 30, 50, false, 30, 15 };
    gems[15] = (Gem){ 510, 357, 30, 50, false, 30, 15 };
    gems[16] = (Gem){ 575, 357, 30, 50, false, 30, 15 };


}
/*
    *Done By Jia Hui
    *This basically initializes the locations for the gems in level 2
    * In order to prevent conflict with level 1, all gems in level 1 are assigned to collected as long as their index are in the range
    * of the gems initialized in level 1.
*/
void intialising_GemsLvl2() {
    gemImage = CP_Image_Load("./Assets/GEM2.png");

    for (int i = 0; i < 17; i++) {
        gems[i].isCollected = true;
    }

    for (int i = 17; i < MAX_GEMS; i++) {
        gems[i].isCollected = false;
    }



    gems[17] = (Gem){ 1548, 811, 30, 50, false, 30, 15 };
    gems[18] = (Gem){ 308, 623, 30, 50, false, 30, 15 };
    gems[19] = (Gem){ 370, 623, 30, 50, false, 30, 15 };
    gems[20] = (Gem){ 724, 606, 30, 50, false, 30, 15 };
    gems[21] = (Gem){ 789, 606, 30, 50, false, 30, 15 };
    gems[22] = (Gem){ 854, 606, 30, 50, false, 30, 15 };
    gems[23] = (Gem){ 919, 606, 30, 50, false, 30, 15 };
    gems[24] = (Gem){ 477, 119, 30, 50, false, 30, 15 };
    gems[23] = (Gem){ 542, 117, 30, 50, false, 30, 15 };
    gems[24] = (Gem){ 53, 391, 30, 50, false, 30, 15 };
    gems[25] = (Gem){ 121, 391, 30, 50, false, 30, 15 };
    gems[26] = (Gem){ 912, 389, 30, 50, false, 30, 15 };
    gems[27] = (Gem){ 977, 389, 30, 50, false, 30, 15 };
    gems[28] = (Gem){ 1042, 389, 30, 50, false, 30, 15 };




}

/*
    * Done By: Jia Hui
    * Here we simply check the current level of the game, and set two int vars to have the start and end values of the indexes of all the gemes to be initialized for that level.'
    * And as long as the struct var of an individual struct of gems in the array of gems for isCollected is zero, we draw the gem.
*/
void draw_Gems(int level) {
    int start = 0, end = MAX_GEMS;

    if (level == 1) {
        start = 0;
        end = 17;  // Level 1 gems  (0-16) - levl 1 17 gems
    }
    else if (level == 2) {
        start = 17;
        end = MAX_GEMS;  // Level 2 gems (17-50 - max gems)
    }

    for (int i = start; i < end; i++) {
        if (!gems[i].isCollected) {
            CP_Image_Draw(gemImage, (float)gems[i].x, (float)gems[i].y, (float)gems[i].width, (float)gems[i].height, 255);
        }
    }
}

/*
    * Done By: Jia Hui
    * Basically here we use AABB collision to check if player has collided with gems, and increment the extern int player points by 10.
*/
bool checkCollisionWithGem(Character* player, Gem* gem, int* playerPoints) {
    //gem boundary
    float rectLeft = gem->x - gem->width / 2;
    float rectRight = gem->x + gem->width / 2;
    float rectTop = gem->y - gem->height / 2;
    float rectBottom = gem->y + gem->height / 2;

    //player collision w gem
    bool collidesWithRectangle = player->x + player->width / 2 > rectLeft &&
        player->x - player->width / 2 < rectRight &&
        player->y + player->height / 2 > rectTop &&
        player->y - player->height / 2 < rectBottom;

    //collect gem if collision
    if (collidesWithRectangle && !(gem->isCollected)) {
        gem->isCollected = true;
        *playerPoints += 10;
        return true;
    }

    return false;  //no collision
}