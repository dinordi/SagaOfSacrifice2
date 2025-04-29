#include "petalinux/fpga/spriteloader.h"
#include <iostream>



// Functie om naar boven af te ronden naar de dichtstbijzijnde page size
size_t round_up_to_page_size(size_t size) {
    return ((size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
}

SpriteLoader::SpriteLoader()
{}

// Functie om een PNG-bestand in te laden
int SpriteLoader::load_png(const char *filename, uint32_t **sprite_data_out, int *width_out, int *height_out, size_t *sprite_size_out) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Fout bij openen PNG bestand");
        return 1;
    }

    // Initialiseer de libpng structuren
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    png_infop end_info = png_create_info_struct(png);

    if (!png || !info || !end_info) {
        perror("Fout bij initialiseren libpng");
        fclose(fp);
        return 1;
    }

    // Stel foutafhandelingsmechanisme in
    if (setjmp(png_jmpbuf(png))) {
        perror("Fout bij PNG-decoding");
        fclose(fp);
        return 1;
    }

    png_init_io(png, fp);
    png_read_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    *width_out = png_get_image_width(png, info);
    *height_out = png_get_image_height(png, info);

    // Alloceer geheugen voor de afbeelding
    *sprite_data_out = (uint32_t *)malloc((*width_out) * (*height_out) * sizeof(uint32_t));
    png_bytep *rows = png_get_rows(png, info);

    // Kopieer de afbeelding naar een array van 32-bit integers
    for (int y = 0; y < *height_out; y++) {
        for (int x = 0; x < *width_out; x++) {
            png_byte *pixel = &(rows[y][x * 4]);
            uint32_t rgba = (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
            (*sprite_data_out)[y * (*width_out) + x] = rgba;
        }
    }

    // Opruimen van libpng
    png_destroy_read_struct(&png, &info, &end_info);
    fclose(fp);

    *sprite_size_out = (*width_out) * (*height_out) * sizeof(uint32_t);
    std::cout << "Sprite size: " << *sprite_size_out << " bytes" << std::endl;
    return 0;
}

// Functie om een PNG-bestand in te laden, het naar fysiek geheugen te schrijven en de mapping te beheren (Vereenvoudigd)
int SpriteLoader::map_sprite_to_memory(const char *filename, uint32_t *phys_addr, uint32_t *sprite_data, size_t sprite_size) {
    int mem_fd = -1; // Initialize to -1
    uint32_t *mapped_mem = nullptr; // Initialize to MAP_FAILED
    size_t mapped_size;
    bool should_free_data = false;
    uint32_t *data_to_use = sprite_data;
    uint32_t original_phys_addr = *phys_addr; // Store original address if needed later
    int result = 0; // Success by default

    if (!sprite_data) {
        // Als er geen sprite_data is meegegeven, laad het dan vanuit het bestand
        uint32_t *loaded_sprite_data = nullptr;
        int width, height;
        size_t loaded_sprite_size;
        if (load_png(filename, &loaded_sprite_data, &width, &height, &loaded_sprite_size) != 0) {
            return 1; // Error loading PNG
        }
        data_to_use = loaded_sprite_data;
        sprite_size = loaded_sprite_size;
        should_free_data = true;  // Flag om aan te geven dat we dit geheugen moeten vrijgeven
    }

    if (sprite_size == 0) {
         // Nothing to map or copy
         if (should_free_data) {
             free_sprite_data(data_to_use);
         }
         return 0; // Or handle as an error if needed
    }

    mapped_size = round_up_to_page_size(sprite_size);  // Rond af naar de dichtstbijzijnde page size

    // Open /dev/mem voor directe toegang tot fysiek geheugen
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Fout bij openen /dev/mem");
        result = 1;
        goto cleanup; // Use goto for centralized cleanup
    }

    // Map het gehele benodigde gebied in één keer
    mapped_mem = (uint32_t *)mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, *phys_addr);
    if (mapped_mem == MAP_FAILED) {
        perror("Fout bij mmap");
        result = 1;
        goto cleanup;
    }

    // Kopieer de *werkelijke* sprite data (niet de afgeronde grootte)
    memcpy(mapped_mem, data_to_use, sprite_size);

    // Optioneel: Werk het fysieke adres bij voor de aanroeper
    // *phys_addr += mapped_size; // Update pointer like the original loop did

    // Geen loop meer nodig

cleanup:
    // Verwijder de mapping
    if (mapped_mem != MAP_FAILED) {
        munmap(mapped_mem, mapped_size);
    }

    // Sluit /dev/mem
    if (mem_fd >= 0) {
        close(mem_fd);
    }

    // Maak het geheugen vrij als we het zelf hebben gealloceerd
    if (should_free_data) {
        free_sprite_data(data_to_use);
    }

    return result;
}

// ...existing code...

// Verwijder de sprite data
void SpriteLoader::free_sprite_data(uint32_t *sprite_data) {
    if (sprite_data) {
        free(sprite_data);
    }
}

SpriteLoader::~SpriteLoader() {
    // Geen automatische cleanup meer nodig omdat sprite_data geen klasseattribuut meer is
}