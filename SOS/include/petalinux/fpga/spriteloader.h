#ifndef SPRITE_LOADER_H
#define SPRITE_LOADER_H

#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#define PAGE_SIZE 4096  // Page size is meestal 4096 bytes

class SpriteLoader 
{
    private:
    uint32_t *sprite_data;
    int width, height;
    size_t sprite_size;

public:
    SpriteLoader();
    // Functie om een PNG-bestand in te laden
    int load_png(const char *filename);

    uint32_t *get_sprite_data();

    size_t get_sprite_size();

    int get_width();

    int get_height();

    // Functie om een PNG-bestand in te laden, het naar fysiek geheugen te schrijven en de mapping te beheren
    int map_sprite_to_memory(const char *filename, uint32_t *phys_addr);

    // Verwijder de sprite data
    void free_sprite_data();
    ~SpriteLoader();
};


#endif // SPRITE_LOADER_H