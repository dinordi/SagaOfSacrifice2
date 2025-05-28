#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
 
// Memory sizes
#define FRAME_INFO_SIZE 0x2000          // 8KB
#define LOOKUP_TABLE_SIZE 0x2000        // 16KB (all pipelines)
 
// Define number of pipelines
#define NUM_PIPELINES 4
 
// Sprite data base address
#define SPRITE_DATA_BASE 0xE0000000     // Adjust as needed
 
// Physical memory base addresses for all 4 pipeline BRAMs as arrays
uint32_t FRAME_INFO_ADDRS[NUM_PIPELINES] = {
    0x42000000,  // Frame info BRAM 0 - 8KB
    0x44000000,  // Frame info BRAM 1 - 8KB
    0x80000000,  // Frame info BRAM 2 - 8KB
    0x80004000   // Frame info BRAM 3 - 8KB
};
 
uint32_t LOOKUP_TABLE_ADDRS[NUM_PIPELINES] = {
    0x40000000,  // Lookup table BRAM 0 - 16KB
    0x46000000,  // Lookup table BRAM 1 - 16KB
    0x80002000,  // Lookup table BRAM 2 - 16KB
    0x80006000   // Lookup table BRAM 3 - 16KB
};
 
int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }
 
    // Arrays for pointers to mapped memory
    void *lookup_table_ptrs[NUM_PIPELINES] = {NULL};
    void *frame_info_ptrs[NUM_PIPELINES] = {NULL};
   
    // Arrays for volatile pointers for actual access
    volatile uint64_t *lookup_tables[NUM_PIPELINES] = {NULL};
    volatile uint64_t *frame_infos[NUM_PIPELINES] = {NULL};
   
    // Map all BRAMs in a loop
    int success = 1;
    for (int i = 0; i < NUM_PIPELINES && success; i++) {
        // Map lookup table BRAM
        lookup_table_ptrs[i] = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, LOOKUP_TABLE_ADDRS[i]);
        if (lookup_table_ptrs[i] == MAP_FAILED) {
            printf("Failed to map lookup table for pipeline %d\n", i);
            success = 0;
            break;
        }
       
        // Map frame info BRAM
        frame_info_ptrs[i] = mmap(NULL, FRAME_INFO_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd, FRAME_INFO_ADDRS[i]);
        if (frame_info_ptrs[i] == MAP_FAILED) {
            printf("Failed to map frame info for pipeline %d\n", i);
            success = 0;
            break;
        }
       
        lookup_tables[i] = (volatile uint64_t *)lookup_table_ptrs[i];
        frame_infos[i] = (volatile uint64_t *)frame_info_ptrs[i];
    }
 
    if (!success) {
        // Clean up any successful mappings before exit
        for (int i = 0; i < NUM_PIPELINES; i++) {
            if (lookup_table_ptrs[i] != NULL && lookup_table_ptrs[i] != MAP_FAILED)
                munmap(lookup_table_ptrs[i], LOOKUP_TABLE_SIZE);
            if (frame_info_ptrs[i] != NULL && frame_info_ptrs[i] != MAP_FAILED)
                munmap(frame_info_ptrs[i], FRAME_INFO_SIZE);
        }
        close(fd);
        return -1;
    }
 
    printf("Successfully mapped all memory regions\n");
 
    // Common sprite configuration
    const int NUM_SPRITES = 1;
    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 400;
   
    // Create lookup table entry with correct field placement
    uint64_t base_lookup_value = (SPRITE_DATA_BASE << 23) | // Base address in upper bits
                                (SPRITE_HEIGHT << 12) |     // Height (11 bits)
                                (SPRITE_WIDTH);             // Width (12 bits)
                               
    printf("*** COMMON LOOKUP TABLE CONFIGURATION ***\n");
    printf("- Sprite width: %u\n", SPRITE_WIDTH);
    printf("- Sprite height: %u\n", SPRITE_HEIGHT);
    printf("- Sprite data base: 0x%08X\n", SPRITE_DATA_BASE);
    printf("- Lookup value: 0x%016llX\n", base_lookup_value);
   
    // Fill all lookup tables with the same information
    printf("\nFilling all lookup tables...\n");
    for (int pipeline = 0; pipeline < NUM_PIPELINES; pipeline++) {
        // Write the same lookup value to each pipeline
        lookup_tables[pipeline][0] = base_lookup_value;
       
        printf("Pipeline %d:\n", pipeline);
        printf("- Lookup table: 0x%08X - filled with value 0x%016llX\n",
               LOOKUP_TABLE_ADDRS[pipeline], base_lookup_value);
    }
   
    // Fill frame info tables with sprite information
    // Each pipeline can have slightly different positioning
    printf("\nFilling all frame info tables...\n");
    for (int pipeline = 0; pipeline < NUM_PIPELINES; pipeline++) {
        // Different starting position for each pipeline
        uint16_t current_x = 400 + (pipeline * 100);
        uint16_t current_y = 400 + (pipeline * 50);
       
        // Fill each sprite entry
        for (int i = 0; i < NUM_SPRITES; i++) {
            uint32_t sprite_id = 0; // Using the same sprite ID for simplicity
           
            uint64_t frame_value = (current_x << 22) |
                                  (current_y << 11) |
                                  (sprite_id);
           
            frame_infos[pipeline][i] = frame_value;
           
            printf("Pipeline %d, Sprite %d: X=%u, Y=%u, ID=%u, Value=0x%016llX\n",
                   pipeline, i, current_x, current_y, sprite_id, frame_value);
           
            // Update for next sprite (diagonal pattern)
            current_x += 100;
            current_y += 80;
        }
       
        // Add termination marker to each frame info table
        frame_infos[pipeline][NUM_SPRITES] = 0xFFFFFFFFFFFFFFFF;
        printf("Pipeline %d: Added termination marker at index %d\n",
               pipeline, NUM_SPRITES);
    }
   
    // Unmap all memory regions
    for (int i = 0; i < NUM_PIPELINES; i++) {
        if (lookup_table_ptrs[i] != NULL && lookup_table_ptrs[i] != MAP_FAILED)
            munmap(lookup_table_ptrs[i], LOOKUP_TABLE_SIZE);
        if (frame_info_ptrs[i] != NULL && frame_info_ptrs[i] != MAP_FAILED)
            munmap(frame_info_ptrs[i], FRAME_INFO_SIZE);
    }
   
    close(fd);
    printf("\nSuccessfully wrote sprite data to all pipelines\n");
    return 0;
}
 