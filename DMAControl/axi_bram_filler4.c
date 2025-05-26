#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
 
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
 
#define TOTAL_STATIC_SPRITES 15

// Helper: schrijf sprites verdeeld over pipelines
void distribute_sprites_over_pipelines(volatile uint64_t *frame_infos[NUM_PIPELINES]) {
    int sprites_in_pipeline[NUM_PIPELINES] = {0};
    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 400;
    for (int s = 0; s < TOTAL_STATIC_SPRITES; s++) {
        int pipeline = s % NUM_PIPELINES;
        int idx = sprites_in_pipeline[pipeline];
        uint16_t x = 100 + idx * 50;
        uint16_t y = 100 + pipeline * 100;
        uint32_t sprite_id = 1;
        uint64_t frame_value = ((uint64_t)x << 22) | ((uint64_t)y << 11) | sprite_id;
        frame_infos[pipeline][idx] = frame_value;
        sprites_in_pipeline[pipeline]++;
    }
    for (int i = 0; i < NUM_PIPELINES; i++) {
        frame_infos[i][sprites_in_pipeline[i]] = 0xFFFFFFFFFFFFFFFF;
    }
}

// IRQ handler: schrijf sprites opnieuw bij elke interrupt
int uio_fd;
uint32_t clear_value = 1;
int stop_thread = 0;

void handleIRQ(volatile uint64_t *frame_infos[NUM_PIPELINES]) {
    uint32_t irq_count;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear interrupt");
    }
    printf("Interrupt received! IRQ count: %d\n", irq_count);
    distribute_sprites_over_pipelines(frame_infos);
}

void irqHandlerThread(volatile uint64_t *frame_infos[NUM_PIPELINES]) {
    struct pollfd fds;
    fds.fd = uio_fd;
    fds.events = POLLIN;
    while (!stop_thread) {
        int ret = poll(&fds, 1, -1);
        if (ret > 0 && (fds.revents & POLLIN)) {
            handleIRQ(frame_infos);
        } else if (ret < 0) {
            perror("poll");
            break;
        }
    }
}

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
 
    // Lookup table vullen
    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 400;
    uint64_t base_lookup_value = ((uint64_t)SPRITE_DATA_BASE << 23) |
                                ((uint64_t)SPRITE_HEIGHT << 12) |
                                (SPRITE_WIDTH);
    for (int pipeline = 0; pipeline < NUM_PIPELINES; pipeline++) {
        lookup_tables[pipeline][1] = base_lookup_value;
    }

    // Initieel sprites verdelen
    distribute_sprites_over_pipelines(frame_infos);

    // IRQ setup
    uio_fd = open("/dev/uio0", O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
    }
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear pending interrupt");
        close(uio_fd);
    }
    irqHandlerThread(frame_infos);
 
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
