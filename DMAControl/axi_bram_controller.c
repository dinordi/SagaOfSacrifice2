#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
 
#define BRAM_BASE 0x80000000
#define BRAM_SIZE 0x2000  // 8 KB

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }
 
    void *bram_void_ptr = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BRAM_BASE);
    if (bram_void_ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }
    // Cast to a 64-bit pointer type
    volatile uint64_t *bram = (volatile uint64_t *)bram_void_ptr;
    
    
    const int NUM_SPRITES = 8; // Number of sprites in the diagonal line
    const uint64_t SPRITE_WIDTH = 400;
    const uint64_t SPRITE_HEIGHT = 100;
    uint64_t current_x = 0;
    uint64_t current_y = 0;

    printf("Writing %d sprites to BRAM...\n", NUM_SPRITES);

    for (int i = 0; i < NUM_SPRITES; ++i) {
        // Use a unique ID for each sprite, e.g., starting from 1
        // Ensure sprite ID fits within 11 bits (0-2047)
        uint64_t sprite_id = (i + 1) % 2048; 
        if (sprite_id == 0) sprite_id = 1; // Avoid ID 0 if it has special meaning

        uint64_t image_value = (current_x << 44) |        // X position
                               (current_y << 33) |        // Y position
                               (SPRITE_WIDTH << 22) |     // Width
                               (SPRITE_HEIGHT << 11) |    // Height
                               (sprite_id);               // ID
        
        bram[i] = image_value;
        printf("Sprite %d: X=%llu, Y=%llu, W=%llu, H=%llu, ID=%llu\n", 
               i + 1, current_x, current_y, SPRITE_WIDTH, SPRITE_HEIGHT, sprite_id);

        // Update coordinates for the next sprite to start at the bottom-right of the current one
        current_x += SPRITE_WIDTH;
        current_y += SPRITE_HEIGHT;
    }
    
    // Termination value (bits 54-33 all set to 1)
    // For termination: bits 54-33 (X and Y fields) must all be 1
    uint64_t termination_value = ((uint64_t)0x3FFFFF << 33); // Set bits 54-33 to all 1's (22 bits for X and Y)
    // Write the 64-bit termination value directly after the last image descriptor
    bram[NUM_SPRITES] = termination_value;
    printf("Wrote termination descriptor at BRAM index %d\n", NUM_SPRITES);
 
    munmap(bram_void_ptr, BRAM_SIZE); // Use the original void* for munmap
    close(fd);
    printf("Successfully wrote %d sprites and termination to BRAM\n", NUM_SPRITES);
    return 0;
}