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

// Function to write a single sprite's information to the frame_info BRAM
void write_sprite_to_frame_info(volatile uint64_t *frame_info_arr, int index, uint16_t x, uint16_t y, uint32_t sprite_id) {
    // Construct the 64-bit value (34 bits effective, but stored in uint64_t)
    // X: bits 33-22 (12 bits), Y: bits 21-11 (11 bits), Sprite ID: bits 10-0 (11 bits)
    uint64_t base_value = ((uint64_t)x << 22) | ((uint64_t)y << 11) | sprite_id;
    frame_info_arr[index] = base_value;
    printf("Frame info [%d]: X=%u, Y=%u, ID=%u\n", index, x, y, sprite_id);
    printf("  Value (hex): 0x%016llX\n", base_value);
}

// Function to update animation state and write to frame_info at index 0
// This function manages its own state for x, y, and direction.
void update_and_write_animated_sprite(volatile uint64_t *frame_info_arr,
                                      uint32_t sprite_id_to_use) {
    static uint16_t s_sprite_x;
    static uint16_t s_sprite_y;
    static int s_direction;
    static int s_initialized = 0;

    if (!s_initialized) {
        s_sprite_x = 120;
        s_sprite_y = 400; // Y is constant for this animation
        s_direction = 1;  // Start by moving right
        s_initialized = 1;
    }

    // Write current sprite state to index 0
    write_sprite_to_frame_info(frame_info_arr, 0, s_sprite_x, s_sprite_y, sprite_id_to_use);

    // Update X position for next frame
    if (s_direction == 1) {
        if (s_sprite_x == 2050) {
            s_direction = -1;
            s_sprite_x--;
        } else {
            s_sprite_x++;
        }
    } else { // s_direction == -1
        if (s_sprite_x == 120) {
            s_direction = 1;
            s_sprite_x++;
        } else {
            s_sprite_x--;
        }
    }
}

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

    const int NUM_SPRITES = 8; // Number of animation steps to record
    const uint16_t SPRITE_WIDTH = 400; // Width for lookup table entry
    const uint16_t SPRITE_HEIGHT = 400; // Height for lookup table entry

    printf("Writing sprite data with 64-bit words:\n");
    printf("- Lookup table: 0x%08X - 0x%08X (8KB)\n", LOOKUP_TABLE_ADDR, LOOKUP_TABLE_ADDR + LOOKUP_TABLE_SIZE - 1);
    printf("- Frame info: 0x%08X - 0x%08X (16KB)\n", FRAME_INFO_ADDR, FRAME_INFO_ADDR + FRAME_INFO_SIZE - 1);
    printf("- Sprite data base: 0x%08X\n", SPRITE_DATA_BASE);
    // Create lookup table entry with correct field placement
    // First create the lookup value without shifting (55 bits total)
    uint64_t base_lookup_value = ((uint64_t)SPRITE_DATA_BASE << 23) | // Base address in upper bits
                                (SPRITE_HEIGHT << 12) | // Height (11 bits)
                                (SPRITE_WIDTH);                     // Width (12 bits)

    lookup_table[1] = base_lookup_value; // Using sprite ID 1

    // Animation parameters
    // animated_sprite_x, animated_sprite_y, and direction are now static within update_and_write_animated_sprite
    uint32_t sprite_id_to_animate = 1; // Corresponds to lookup_table[1]

    printf("Writing animated sprite data to frame info (always at index 0):\n");
    // Write frame info entries based on animation
    // Each call updates frame_info[0] with the next animation step.
    // The loop runs NUM_SPRITES times to show NUM_SPRITES steps of animation.
    
    //update_and_write_animated_sprite(frame_info, sprite_id_to_animate);
    

    // Create lookup table entry with proper alignment
    printf("Writing to lookup table at index 0\n"); // This line was present after the user's selection. Kept as is.

    // Add termination marker properly
    // Since we are always writing to frame_info[0], the list effectively has one active sprite.
    // The termination marker should be at the next position.
    frame_info[1] = 0xFFFFFFFFFFFFFFFF;

    // Unmap both regions
    munmap(lookup_table_ptr, LOOKUP_TABLE_SIZE);
    munmap(frame_info_ptr, FRAME_INFO_SIZE);
    close(fd);
    printf("Successfully wrote sprite data using 64-bit words\n");
    return 0;
}
