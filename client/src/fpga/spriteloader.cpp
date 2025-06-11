#include "fpga/spriteloader.h"
#include <iostream>



// Functie om naar boven af te ronden naar de dichtstbijzijnde page size
size_t round_up_to_page_size(size_t size) {
    return ((size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
}

SpriteLoader::SpriteLoader()
{}




int SpriteLoader::load_png(const char *filename, uint32_t *sprite_data_out, int *width_out, int *height_out, size_t *sprite_size_out) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Fout bij openen PNG bestand");
        return 1;
    }

    unsigned char header[8];
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
        std::cerr << "Invalid PNG signature" << std::endl;
        fclose(fp);
        return 1;
    }
    fseek(fp, 0, SEEK_SET);

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (!png || !info) {
        perror("Error initializing libpng");
        fclose(fp);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        perror("Error during PNG decoding");
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

    if (width > MAX_WIDTH || height > MAX_HEIGHT) {
        std::cerr << "PNG image too large for buffer (max " << MAX_WIDTH << "x" << MAX_HEIGHT << ")" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    // Format conversie naar RGBA
    if (color_type != PNG_COLOR_TYPE_RGBA) {
        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);
        if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

        png_read_update_info(png, info);

        png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
                     &interlace_type, NULL, NULL);
    }

    if (color_type != PNG_COLOR_TYPE_RGBA || bit_depth != 8) {
        std::cerr << "Unsupported PNG format after conversion" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    // Stack buffer voor 1 rij pixels (RGBA = 4 bytes per pixel)
    png_byte row_buffer[MAX_WIDTH * 4];

    for (unsigned y = 0; y < height; y++) {
        png_read_row(png, row_buffer, NULL);

        for (unsigned x = 0; x < width; x++) {
            png_bytep px = &(row_buffer[x * 4]);

            uint32_t rgba =
                ((uint32_t)px[0] << 24) |  // R
                ((uint32_t)px[1] << 16) |  // G
                ((uint32_t)px[2] << 8)  |  // B
                ((uint32_t)px[3]);          // A

            sprite_data_out[y * width + x] = rgba;
        }
    }

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    *width_out = width;
    *height_out = height;
    *sprite_size_out = width * height * sizeof(uint32_t);

    // std::cout << "Sprite succesvol geladen: " << width << "x" << height
    //           << ", " << *sprite_size_out << " bytes" << std::endl;

    return 0;
}



// Functie om een PNG-bestand in te laden, het naar fysiek geheugen te schrijven en de mapping te beheren (Vereenvoudigd)
int SpriteLoader::map_sprite_to_memory(const char *filename, volatile uint32_t *phys_addr, uint32_t *sprite_data, size_t sprite_size) {
    int mem_fd = -1;
    void *mapped_mem = MAP_FAILED;
    size_t mapped_size;
    uint32_t original_phys_addr = *phys_addr;
    int result = 0;

    if ((*phys_addr & (PAGE_SIZE - 1)) != 0) {
        std::cerr << "ERROR: Physical address 0x" << std::hex << *phys_addr
                  << " is NOT page aligned!" << std::dec << std::endl;
        return 1;
    }

    if (!sprite_data || sprite_size == 0) {
        std::cerr << "ERROR: sprite_data is null or sprite_size is zero" << std::endl;
        return 1;
    }

    mapped_size = round_up_to_page_size(sprite_size);

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Fout bij openen /dev/mem");
        return 1;
    }

    mapped_mem = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, *phys_addr);
    if (mapped_mem == MAP_FAILED) {
        perror("Fout bij mmap");
        std::cerr << "Failed mapping address: 0x" << std::hex << *phys_addr
                  << " size: " << std::dec << mapped_size << std::endl;
        close(mem_fd);
        return 1;
    }

    //std::cout << "Initializing mapped region (" << mapped_size << " bytes) to 0xFFFFFFFF..." << std::endl;
    memset(mapped_mem, 0xFF, mapped_size);

    std::memcpy(mapped_mem, sprite_data, sprite_size);

    *phys_addr += mapped_size;

    munmap(mapped_mem, mapped_size);
    close(mem_fd);

    return result;
}

int SpriteLoader::load_png_spritesheet(const char *filename, uint32_t *sprite_data_out,
                                       int sprite_width, int sprite_height,
                                       int x, int y, png_bytep* row_pointers)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Fout bij openen PNG bestand");
        return 1;
    }

    unsigned char header[8];
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
        std::cerr << "Ongeldige PNG signature" << std::endl;
        fclose(fp);
        return 1;
    }
    fseek(fp, 0, SEEK_SET);

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (!png || !info) {
        std::cerr << "libpng initialisatie mislukt" << std::endl;
        fclose(fp);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        std::cerr << "Fout tijdens PNG lezen" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 img_width, img_height;
    int bit_depth, color_type, interlace_type;
    png_get_IHDR(png, info, &img_width, &img_height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

    // Conversie naar RGBA8 indien nodig
    if (color_type != PNG_COLOR_TYPE_RGBA) {
        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);
        if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

        png_read_update_info(png, info);
        png_get_IHDR(png, info, &img_width, &img_height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
    }

    if (color_type != PNG_COLOR_TYPE_RGBA || bit_depth != 8) {
        std::cerr << "Niet-ondersteund PNG formaat na conversie" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    // Controleer of de sprite binnen de afbeelding valt
    if (x + sprite_width > (int)img_width || y + sprite_height > (int)img_height) {
        std::cerr << "Sprite valt buiten de grenzen van de afbeelding" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    // Use pre-allocated row pointers instead of malloc/free
    png_read_image(png, row_pointers);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);

    // Sprite uitsnijden vanaf pixel (x, y)
    for (int row = 0; row < sprite_height; row++) {
        png_bytep src_row = row_pointers[y + row];
        for (int col = 0; col < sprite_width; col++) {
            png_bytep px = &src_row[(x + col) * 4];

            uint32_t rgba =
                ((uint32_t)px[0] << 24) |  // R
                ((uint32_t)px[1] << 16) |  // G
                ((uint32_t)px[2] << 8)  |  // B
                ((uint32_t)px[3]);         // A

            sprite_data_out[row * sprite_width + col] = rgba;
        }
    }

    static int sprite_count = 0;
    sprite_count++;
    if(sprite_count % 20 == 0) {
        std::cout << "Sprite: " << sprite_count <<  " succesvol geknipt op pixel (" << x << ", " << y << ") met grootte "
                  << sprite_width << "x" << sprite_height << std::endl;
    }

    return 0;
}


// Verwijder de sprite data
void SpriteLoader::free_sprite_data(uint32_t *sprite_data) {
    if (sprite_data) {
        free(sprite_data);
    }
}

SpriteLoader::~SpriteLoader() {
    // Geen automatische cleanup meer nodig omdat sprite_data geen klasseattribuut meer is
}