#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

// Physical memory base address for the BRAM
#define BRAM_PHYS_ADDR 0x40000000          // Physical BRAM starts at address 0
#define LOOKUP_TABLE_OFFSET 0x0000         // 0x0000-0x3FFF (16KB)
#define FRAME_INFO_OFFSET 0x4000           // 0x4000-0x5FFF (8KB)
#define TOTAL_BRAM_SIZE 0x8000             // 32KB total size

// Sprite data base address
#define SPRITE_DATA_BASE 0xE000000         // Base address for sprite data

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    // Map the BRAM at physical address 0x00000000
    void *bram_void_ptr = mmap(NULL, TOTAL_BRAM_SIZE, PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, BRAM_PHYS_ADDR);
    if (bram_void_ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    // Get pointers to the separate BRAMs - using 64-bit words
    volatile uint64_t *lookup_table = (volatile uint64_t *)bram_void_ptr;
    volatile uint64_t *frame_info = (volatile uint64_t *)((uint8_t*)bram_void_ptr + FRAME_INFO_OFFSET);

    const int NUM_SPRITES = 8;
    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 100;
    uint16_t current_x = 0;
    uint16_t current_y = 0;

    printf("Writing sprite data with 64-bit words:\n");
    printf("- Lookup table: 0x%08X-0x%08X\n", BRAM_PHYS_ADDR, BRAM_PHYS_ADDR + 0x3FFF);
    printf("- Frame info: 0x%08X-0x%08X\n", BRAM_PHYS_ADDR + 0x4000, BRAM_PHYS_ADDR + 0x5FFF);
    printf("- Sprite data base: 0x%08X\n", SPRITE_DATA_BASE);

    for (int i = 1; i <= NUM_SPRITES; ++i) {
        uint32_t sprite_id = 1;

        // Write to frame_info BRAM (64-bit word)
        // Format: X position and Y position in lower bits
        uint64_t frame_info_value = (current_x << 22) | (current_y << 11) | sprite_id;
        frame_info[i] = frame_info_value;



        // Update for next sprite (diagonal pattern)
        current_x += 100;
        current_y += 80;
    }
    uint64_t lookup_value = (SPRITE_DATA_BASE << 24) | // Put address in upper bits
                               (SPRITE_HEIGHT << 12) | // Height in bits 22:12
                               SPRITE_WIDTH;           // Width in bits 11:0

        lookup_table[1] = lookup_value;
    // Add termination marker in frame_info BRAM
    frame_info[8] = 0xFFFFFFFFFFFFFFFF;  // All 1s as termination marker
    printf("Added termination marker at frame_info[8]\n");

    munmap(bram_void_ptr, TOTAL_BRAM_SIZE);
    close(fd);
    printf("Successfully wrote sprite data using 64-bit words\n");
    return 0;
}