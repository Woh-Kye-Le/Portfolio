#include "cprocessing.h"
#include <stdio.h>
#include <stdbool.h>
#include "collectables.h"
#include "game.h"
#include <math.h>

struct Character;

CP_Image speedBoost;
CP_Image hpRestore;

/*
    * Done by: Jia Hui
    * Basically it loads the image speed and hp
*/
void loadImage(void) {
    speedBoost = CP_Image_Load("Assets/speed.png");
    hpRestore = CP_Image_Load("Assets/hp.png");
}
/*
    * Done by: Jia Hui
    * Initialization of the array of collectable structs for level 1 to be collected
*/
void initialiseCollectables(Collectable* collectables) {
    collectables[0] = (Collectable){ 499.0, 182.0, 50.0, 1, false };
    collectables[1] = (Collectable){ 1551.0, 317.0, 50.0, 1, false };
    collectables[2] = (Collectable){ 817.0, 693.0, 50.0, 1, false };
    collectables[3] = (Collectable){ 1002.0, 480.0, 50.0, 0, false };
    collectables[4] = (Collectable){ 1205.0, 670.0, 50.0, 0, false };

    for (int i = 5; i < NUM_COLLECTABLES; i++) {
        collectables[i].isCollected = true;
    }

}
/*
    * Done by: Jia Hui
    * Initialization of the array of collectable structs for level 2 to be collected
*/
void initialiseCollectablesLvl2(Collectable* collectables) {


    collectables[0] = (Collectable){ 50.0, 220.0, 50.0, 1, false };
    collectables[1] = (Collectable){ 1207.0, 172.0, 50.0, 1, false };
    collectables[2] = (Collectable){ 1120.0, 389.0, 50.0, 1, false };
    collectables[3] = (Collectable){ 920.0, 609.0, 50.0, 0, false };
    collectables[4] = (Collectable){ 468.0, 120.0, 50.0, 0, false };

    for (int i = 5; i < NUM_COLLECTABLES; i++) {
        collectables[i].isCollected = true;
    }


}

/*
    * Done by: Jia Hui
    * sets the image to be drawn depending on the collectables type
*/
void spawnCollectables(Collectable* collectables) {
    if (!collectables->isCollected) return;

    CP_Image collectableImg = collectables->type == 0 ? speedBoost : hpRestore;
}
/*
    * Done By: Jia Hui 
    * Draws the collectable based on the image assigned in spawnCollectables
*/
void drawCollectables(Collectable* collectables, int numCollectables) {
    for (int i = 0; i < numCollectables; i++) {
        if (!collectables[i].isCollected) {

            CP_Image collectableImg = collectables[i].type == 0 ? speedBoost : hpRestore;
            CP_Image_Draw(collectableImg, collectables[i].x, collectables[i].y, 50, 50, 255);

        }
    }
}


/*
    * Done By: Jia Hui
    * Basically here we use AABB collision to check if player has collided with collectables, and apply the speed or health boost to the player, and set the var of the collectable struct index in the array of
    * collectables struct to true, and then return true, else if aabb collision does not occur, return false.
*/
bool checkCollisionWithCollectables(Character* player, Collectable* collectables, int numCollectables) {
    for (int i = 0; i < numCollectables; i++) {
        if (collectables[i].isCollected) {
            continue;
        }

        //collectable 
        float collectableCenterX = collectables[i].x;
        float collectableCenterY = collectables[i].y;
        float collectableRadius = collectables[i].radius;

        //player
        float playerLeft = player->x;
        float playerRight = player->x + player->width;
        float playerTop = player->y;
        float playerBottom = player->y + player->height;

        if (collectableCenterX + collectableRadius > playerLeft && collectableCenterX - collectableRadius < playerRight &&
            collectableCenterY + collectableRadius > playerTop && collectableCenterY - collectableRadius < playerBottom) {

            collectables[i].isCollected = true;

            printf("hp before: %f\n", player->health);
            printf("speed before: %f\n", player->speed_x);

            if (collectables[i].type == 0) {
                if (!player->isSpeed) {
                    player->speed_x *= 1.3f;
                    player->isSpeed = true;
                }
                player->speedTimer = 2.0f;
                printf("Speed boost collected! Speed: %f, Timer: %f\n", player->speed_x, player->speedTimer);
            }
            else if (collectables[i].type == 1) {
                player->health = fminf(player->health + 100.0f, player->maxHealth);
                printf("hp after: %f\n", player->health);
            }

            return true;
        }
    }
    return false;
}