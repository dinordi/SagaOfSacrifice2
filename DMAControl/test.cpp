#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <png.h>

#define PAGE_SIZE 4096  // Page size is meestal 4096 bytes

// Functie om naar boven af te ronden naar de dichtstbijzijnde page size
size_t round_up_to_page_size(size_t size) {
    return ((size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
}

class SpriteLoader {
private:
    uint32_t *sprite_data;
    int width, height;
    size_t sprite_size;

public:
    SpriteLoader() : sprite_data(NULL), width(0), height(0), sprite_size(0) {}

    // Functie om een PNG-bestand in te laden
    int load_png(const char *filename) {
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

        width = png_get_image_width(png, info);
        height = png_get_image_height(png, info);

        // Alloceer geheugen voor de afbeelding
        sprite_data = (uint32_t *)malloc(width * height * sizeof(uint32_t));
        png_bytep *rows = png_get_rows(png, info);

        // Kopieer de afbeelding naar een array van 32-bit integers
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                png_byte *pixel = &(rows[y][x * 4]);
                uint32_t rgba = (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
                sprite_data[y * width + x] = rgba;
            }
        }

        // Opruimen van libpng
        png_destroy_read_struct(&png, &info, &end_info);
        fclose(fp);

        sprite_size = width * height * sizeof(uint32_t);
        return 0;
    }

    uint32_t *get_sprite_data() {
        return sprite_data;
    }

    size_t get_sprite_size() {
        return sprite_size;
    }

    int get_width() {
        return width;
    }

    int get_height() {
        return height;
    }

    // Functie om een PNG-bestand in te laden, het naar fysiek geheugen te schrijven en de mapping te beheren
    int map_sprite_to_memory(const char *filename, uint32_t *phys_addr) {
        int mem_fd;
        uint32_t *mapped_mem;
        size_t mapped_size;
        size_t sprite_size;

        // Laad de PNG afbeelding
        if (load_png(filename) != 0) {
            return 1;
        }

        uint32_t *sprite_data = get_sprite_data();
        sprite_size = get_sprite_size();
        mapped_size = round_up_to_page_size(sprite_size);  // Rond af naar de dichtstbijzijnde page size

        // Open /dev/mem voor directe toegang tot fysiek geheugen
        mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (mem_fd < 0) {
            perror("Fout bij openen /dev/mem");
            return 1;
        }

        // Itereer door alle pagina's die we moeten mappen
        size_t offset = 0;
        while (offset < sprite_size) {
            size_t remaining_size = sprite_size - offset;
            size_t page_size_to_map = (remaining_size < PAGE_SIZE) ? remaining_size : PAGE_SIZE;

            // Map de pagina in het fysieke geheugen
            mapped_mem = (uint32_t *)mmap(NULL, page_size_to_map, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, *phys_addr);
            if (mapped_mem == MAP_FAILED) {
                perror("Fout bij mmap");
                close(mem_fd);
                return 1;
            }

            // Kopieer de sprite data naar de gemapte geheugenpagina
            memcpy(mapped_mem, sprite_data + offset, page_size_to_map);

            // Verhoog de offset en fysiek adres voor de volgende pagina
            offset += page_size_to_map;
            *phys_addr += page_size_to_map;

            // Verwijder de mapping van de huidige pagina
            munmap(mapped_mem, page_size_to_map);
        }

        close(mem_fd);
        return 0;
    }

    // Verwijder de sprite data
    void free_sprite_data() {
        if (sprite_data) {
            free(sprite_data);
            sprite_data = NULL;
        }
    }

    ~SpriteLoader() {
        free_sprite_data();
    }
};

// Functie om het fysieke geheugen te controleren
void check_sprite_in_memory(uint32_t *phys_addr, size_t size) {
    int mem_fd;
    uint32_t *mapped_mem;

    // Open /dev/mem voor directe toegang tot fysiek geheugen
    mem_fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (mem_fd < 0) {
        perror("Fout bij openen /dev/mem");
        return;
    }

    // Memory-map de fysieke pagina om te lezen
    mapped_mem = (uint32_t *)mmap(NULL, size, PROT_READ, MAP_SHARED, mem_fd, *phys_addr);
    if (mapped_mem == MAP_FAILED) {
        perror("Fout bij mmap");
        close(mem_fd);
        return;
    }

    // Controleer of de data correct is gemapt door een paar waarden af te drukken
    printf("Data op fysiek adres 0x%08x:\n", *phys_addr);
    for (size_t i = 0; i < size / sizeof(uint32_t); i++) {
        printf("0x%08x ", mapped_mem[i]);
        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }

    // Verwijder de mapping na gebruik
    munmap(mapped_mem, size);
    close(mem_fd);
}

int main() {
    uint32_t phys_addr = 0x014B2000;  // Start fysiek adres (voorbeeld)
    const char *png_file = "/home/root/SagaOfSacrifice2/SOS/assets/sprites/player.png";  // Pad naar je PNG bestand

    SpriteLoader spriteLoader;

    if (spriteLoader.map_sprite_to_memory(png_file, &phys_addr) != 0) {
        return 1;
    }

    printf("Sprite succesvol gemapt naar geheugen vanaf fysiek adres: 0x%08x\n", phys_addr);
    uint32_t startAddress = 0x014B2000;
    // Controleer of de data goed in geheugen staat
    check_sprite_in_memory(&startAddress, spriteLoader.get_sprite_size());
    return 0;
}
