#ifndef SPRITE_LOADER_H
#define SPRITE_LOADER_H

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib> // For free
#include <sys/mman.h> // For mmap and munmap
#include <cstdio> // For perror
#include <png.h> // For libpng functions
#include <cstdint>

#define PAGE_SIZE 4096  // Page size is meestal 4096 bytes
#define MAX_WIDTH  2022
#define MAX_HEIGHT 3610


class SpriteLoader 
{
public:
    SpriteLoader();
    // Functie om een PNG-bestand in te laden
    int load_png(const char *filename, uint32_t *sprite_data_out, int *width_out, int *height_out, size_t *sprite_size_out);

    // Functie om een PNG-bestand in te laden, het naar fysiek geheugen te schrijven en de mapping te beheren
    int map_sprite_to_memory(const char *filename, uint32_t *phys_addr, uint32_t *sprite_data, size_t sprite_size);

    // Verwijder de sprite data
    void free_sprite_data(uint32_t *sprite_data);
    ~SpriteLoader();
};

#endif // SPRITE_LOADER_H