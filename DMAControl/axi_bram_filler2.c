#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

// Physical memory base addresses for the BRAMs
#define FRAME_INFO_ADDR   0x42000000         // Frame info BRAM - 8KB
#define LOOKUP_TABLE_ADDR 0x40000000       // Lookup table BRAM - 16KB

#define FRAME_INFO_SIZE 0x2000             // 8KB
#define LOOKUP_TABLE_SIZE 0x4000           // 16KB

// Sprite data base address
#define SPRITE_DATA_BASE 0x0E000000        // Base address for sprite data (fixed to include leading 0)

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    // Map each BRAM separately with their own physical addresses
    // First map the lookup table BRAM
    void *lookup_table_ptr = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd, LOOKUP_TABLE_ADDR);
    if (lookup_table_ptr == MAP_FAILED) {
        perror("mmap lookup table");
        close(fd);
        return -1;
    }

    // Then map the frame info BRAM
    void *frame_info_ptr = mmap(NULL, FRAME_INFO_SIZE, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, FRAME_INFO_ADDR);
    if (frame_info_ptr == MAP_FAILED) {
        perror("mmap frame info");
        munmap(lookup_table_ptr, LOOKUP_TABLE_SIZE);
        close(fd);
        return -1;
    }

    // Get pointers to the separate BRAMs - using 64-bit words
    volatile uint64_t *lookup_table = (volatile uint64_t *)lookup_table_ptr;
    volatile uint64_t *frame_info = (volatile uint64_t *)frame_info_ptr;

    const int NUM_SPRITES = 1;
    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 400;
    uint16_t current_x = 400;
    uint16_t current_y = 400;

    printf("Writing sprite data with 64-bit words:\n");
    printf("- Lookup table: 0x%08X - 0x%08X (8KB)\n", LOOKUP_TABLE_ADDR, LOOKUP_TABLE_ADDR + LOOKUP_TABLE_SIZE - 1);
    printf("- Frame info: 0x%08X - 0x%08X (16KB)\n", FRAME_INFO_ADDR, FRAME_INFO_ADDR + FRAME_INFO_SIZE - 1);
    printf("- Sprite data base: 0x%08X\n", SPRITE_DATA_BASE);
    // Create lookup table entry with correct field placement
    // Ensure sprite data base is properly aligned for memory access
    uint32_t aligned_sprite_base = SPRITE_DATA_BASE & 0xFFFFFF00; // Ensure alignment
    uint64_t lookup_value = (((uint64_t)aligned_sprite_base) << 24) | // Base address in upper bits
                            ((uint64_t)(SPRITE_HEIGHT) << 12) | // Height (11 bits)
                            (SPRITE_WIDTH);                     // Width (12 bits)
    
    lookup_table[0] = lookup_value;
    // Write frame info entries
    for (int i = 1; i <= NUM_SPRITES; ++i) {
        uint32_t sprite_id = 0;

        // Format: X position and Y position in lower bits
        uint64_t frame_info_value = (current_x << 22) | (current_y << 11) | sprite_id;
        frame_info[i] = frame_info_value;
        printf("Frame info [%d]: X=%u, Y=%u, ID=%u (hex: 0x%016llX)\n", 
               i, current_x, current_y, sprite_id, frame_info_value);

        // Update for next sprite (diagonal pattern)
        current_x += 100;
        current_y += 80;
    }

    // Create lookup table entry with proper alignment
    printf("Writing to lookup table at index 0\n");
    
    
    
    // Print in multiple formats for better visibility
    printf("*** LOOKUP VALUE (64-bit) ***\n");
    printf("  HEX: 0x%016llX\n", lookup_value);
    printf("  Components: base=0x%08X, height=%u, width=%u\n", 
            aligned_sprite_base, SPRITE_HEIGHT, SPRITE_WIDTH);
    
    // Add termination marker properly
    // Set frame_info[0] to all 1s (64 bits)
    frame_info[0] |= 0xFFFFFFFFFFFFFFFF;
    
    // Set only the 3 most significant bits of frame_info[1] to 1s
    // while preserving the rest of the bits (the valid sprite data)
    frame_info[1] |= 0xE000000000000000; // 0xE = 0b1110 (3 most significant bits)
    
    printf("*** FRAME INFO VALUES (64-bit) ***\n");
    printf("  frame_info[0]=0x%016llX\n", frame_info[0]);
    printf("  frame_info[1]=0x%016llX\n", frame_info[1]);

    // Unmap both regions
    munmap(lookup_table_ptr, LOOKUP_TABLE_SIZE);
    munmap(frame_info_ptr, FRAME_INFO_SIZE);
    close(fd);
    printf("Successfully wrote sprite data using 64-bit words\n");
    return 0;
}