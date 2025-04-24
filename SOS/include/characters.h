#ifndef CHARACTERS_H
#define CHARACTERS_H

enum SpriteTypes
{
    NONE = 0,
    COMMANDO = 1,
    FATBAT = 2,
    WEREWOLF = 3,
    SAMURAI = 4,
    PLATFORM1 = 5,
    PLATFORM2 = 6,
    PLATFORM3 = 7,
};

typedef struct
{
    SpriteTypes type;
    int width;
    int height;
    int srcX;
    int srcY;
} SpriteInfo;

SpriteInfo Platform1Info = {
    .type = PLATFORM1,
    .width = 128,
    .height = 128,
    .srcX = 0,
    .srcY = 0
};

SpriteInfo Platform2Info = {
    .type = PLATFORM2,
    .width = 128,
    .height = 128,
    .srcX = 0,
    .srcY = 128
};

#endif // CHARACTERS_H