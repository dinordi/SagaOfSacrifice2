/**
 * @file sprites.h
 * @author -
 * @brief File containing the sprite IDs for the different entities in the game.
 * @version 0.1
 * @date 2024-04-21
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <map>


const int player1Sprites[13] = {0,1,2,3,2+512,3+512, 115, 116, 117, 115+512, 116+512, 117+512, 4};
const int fatbatSprites[3] = {5,6,55};
const int wherewolfSprites[6] = {7,8,9,10,53,54}; // 7 en 8 idle 9 en 10 lopen 53 en 54 attack
                            // Idle _______ Walking _______________________ Attacking    
const int samuraiSprites[17] = {112,113,114,104,105,106,107,108,109,110,111,200, 201, 202, 203, 204, 205};
const int platformIDs[4] = {100,101,102,103};
const int bulletID[1] = {47};
const int empty15x15[1] = {52};

extern std::map<char, int> characters;

void old_initializeCharacters();