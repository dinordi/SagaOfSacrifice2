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

    // Check PNG signature to verify it's a valid PNG file
    unsigned char header[8];
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
        std::cerr << "Invalid PNG signature" << std::endl;
        fclose(fp);
        return 1;
    }
    fseek(fp, 0, SEEK_SET); // Reset file position

    // Initialize libpng structures
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);

    if (!png || !info) {
        perror("Error initializing libpng");
        fclose(fp);
        return 1;
    }

    // Set error handling
    if (setjmp(png_jmpbuf(png))) {
        perror("Error during PNG decoding");
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    // Get image info
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);

    // Check if the image has the correct format for our needs
    std::cout << "PNG Info - Width: " << width << ", Height: " << height 
              << ", Bit depth: " << bit_depth << ", Color type: " << color_type << std::endl;
    
    // Convert to RGBA if necessary
    if (color_type != PNG_COLOR_TYPE_RGBA) {
        std::cout << "Converting PNG to RGBA format..." << std::endl;
        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);
        if (color_type == PNG_COLOR_TYPE_RGB || 
            color_type == PNG_COLOR_TYPE_GRAY || 
            color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
            
        // Update with the transformations
        png_read_update_info(png, info);
        png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
                    &interlace_type, NULL, NULL);
    }
    
    // Now we should have RGBA data, Verify it
    if (color_type != PNG_COLOR_TYPE_RGBA || bit_depth != 8) {
        std::cerr << "Could not convert PNG to required format (RGBA, 8bit)" << std::endl;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    // Allocate memory for the image
    *width_out = width;
    *height_out = height;
    size_t sprite_buffer_size = width * height * sizeof(uint32_t);
    *sprite_data_out = (uint32_t *)malloc(sprite_buffer_size);
    
    if (!(*sprite_data_out)) {
        perror("Failed to allocate memory for sprite");
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }
    
    // Set up row pointers
    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        perror("Failed to allocate memory for PNG rows");
        free(*sprite_data_out);
        *sprite_data_out = NULL;
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }
    
    // Read the image data
    for (size_t y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
    }
    
    png_read_image(png, row_pointers);
    
    // Copy image to output buffer with byte order matching hardware expectations
    // Note: We're using RGBA byte order here (MSB to LSB: R,G,B,A)
    // Adjust if your hardware expects a different order (e.g., BGRA, ARGB)
    for (size_t y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (size_t x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            
            // Store in R,G,B,A order (R is most significant byte)
            // This format works for most FPGA implementations
            uint32_t rgba = 
                  ((uint32_t)px[0] << 24)  // R
                | ((uint32_t)px[1] << 16)  // G
                | ((uint32_t)px[2] << 8)   // B
                | ((uint32_t)px[3]);       // A
                
            (*sprite_data_out)[y * width + x] = rgba;
        }
    }

    // Debug output for first few pixels
    std::cout << "Debug: First 5 pixels in RGBA (hex): ";
    for (int i = 0; i < std::min(5, (int)width); i++) {
        std::cout << "0x" << std::hex << (*sprite_data_out)[i] << std::dec << " ";
    }
    std::cout << std::endl;
    
    // Clean up
    for (size_t y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    *sprite_size_out = width * height * sizeof(uint32_t);
    std::cout << "Sprite successfully loaded: " << width << "x" << height 
              << ", " << *sprite_size_out << " bytes" << std::endl;
    return 0;
}

// Functie om een PNG-bestand in te laden, het naar fysiek geheugen te schrijven en de mapping te beheren (Vereenvoudigd)
int SpriteLoader::map_sprite_to_memory(const char *filename, uint32_t *phys_addr, uint32_t *sprite_data, size_t sprite_size) {
    int mem_fd = -1;
    void *mapped_mem = MAP_FAILED; // Use void* for mmap, cast later
    size_t mapped_size;
    bool should_free_data = false;
    uint32_t *data_to_use = sprite_data;
    uint32_t original_phys_addr = *phys_addr;
    int result = 0;

    // --- Potential Issue 1: Address Alignment ---
    // Although you mentioned it's page aligned, let's add a check/assertion for safety.
    if ((*phys_addr & (PAGE_SIZE - 1)) != 0) {
        std::cerr << "ERROR: Physical address 0x" << std::hex << *phys_addr
                  << " is NOT page aligned!" << std::dec << std::endl;
        // Optionally, force alignment (though this might shift your intended start address)
        // *phys_addr = *phys_addr & ~(PAGE_SIZE - 1);
        // std::cout << "Forcing alignment to: 0x" << std::hex << *phys_addr << std::dec << std::endl;
        result = 1; // Treat non-alignment as an error
        goto cleanup;
    }

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

    // Map the required region based on the page-aligned physical address
    mapped_mem = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, *phys_addr);
    if (mapped_mem == MAP_FAILED) {
        perror("Fout bij mmap");
        std::cerr << "Failed mapping address: 0x" << std::hex << *phys_addr
                  << " size: " << std::dec << mapped_size << std::endl;
        result = 1;
        goto cleanup;
    }

    // Initialize the mapped region to 0xFFFFFFFF before copying
    std::cout << "Initializing mapped region (" << mapped_size << " bytes) to 0xFFFFFFFF..." << std::endl;
    memset(mapped_mem, 0xFF, mapped_size);

    // Copy the actual sprite data
    std::cout << "Copying " << sprite_size << " bytes to virtual address " << mapped_mem
              << " (physical 0x" << std::hex << *phys_addr << std::dec << ")" << std::endl;
    memcpy(mapped_mem, data_to_use, sprite_size);

    // --- Debugging before msync ---
    std::cout << "Preparing for msync:" << std::endl;
    std::cout << "  mapped_mem address: " << mapped_mem << std::endl;
    std::cout << "  mapped_size: " << mapped_size << " bytes" << std::endl;
    if (mapped_mem == NULL || mapped_mem == MAP_FAILED) {
        std::cerr << "  ERROR: mapped_mem pointer is invalid!" << std::endl;
    }
    if (mapped_size == 0) {
        std::cerr << "  ERROR: mapped_size is zero!" << std::endl;
    }
    // --- End Debugging ---

    // Explicitly synchronize the memory: Flush CPU cache and invalidate others
    // --- Potential Fix: Try syncing only the actual sprite_size ---
    // Sometimes msync is sensitive to the length argument, especially with /dev/mem.
    // Let's try syncing only the bytes we actually wrote (sprite_size) instead of the
    // full page-aligned mapped_size.
    size_t size_to_sync = sprite_size;
    std::cout << "Synchronizing memory (msync with MS_SYNC | MS_INVALIDATE) for "
              << size_to_sync << " bytes..." << std::endl;

    if (msync(mapped_mem, size_to_sync, MS_SYNC | MS_INVALIDATE) == -1) { // Using size_to_sync
         // Capture errno immediately
         int msync_errno = errno;
         perror("Error during msync");
         std::cerr << "  msync errno: " << msync_errno << " (" << strerror(msync_errno) << ")" << std::endl;
         std::cerr << "  msync arguments: addr=" << mapped_mem << ", len=" << size_to_sync << std::endl; // Log the size used
         // Decide if this is a fatal error
         // result = 1;
         // goto cleanup;
    } else {
         std::cout << "msync completed successfully for mapped region." << std::endl;
    }

    // Update the physical address pointer for the caller to the next available address
    // *phys_addr += sprite_size; // Point right after the actual data
    // Or round up to the next page if you want page-aligned blocks
    *phys_addr += mapped_size;

cleanup:
    // Verwijder de mapping
    // munmap requires the starting virtual address returned by mmap and the original mapped size.
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

// Verwijder de sprite data
void SpriteLoader::free_sprite_data(uint32_t *sprite_data) {
    if (sprite_data) {
        free(sprite_data);
    }
}

SpriteLoader::~SpriteLoader() {
    // Geen automatische cleanup meer nodig omdat sprite_data geen klasseattribuut meer is
}