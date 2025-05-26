#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#define FRAME_INFO_ADDR   0x42000000
#define LOOKUP_TABLE_ADDR 0x40000000

#define FRAME_INFO_SIZE 0x2000
#define LOOKUP_TABLE_SIZE 0x2000

#define SPRITE_DATA_BASE 0x0E000000
#define NUM_PIPELINES 4

const uint32_t LOOKUP_TABLE_ADDRS[NUM_PIPELINES] = {
    0x40000000, 0x46000000, 0x80002000, 0x80006000
};
const uint32_t FRAME_INFO_ADDRS[NUM_PIPELINES] = {
    0x42000000, 0x44000000, 0x80000000, 0x80004000
};

int uio_fd;
uint32_t irq_count;
uint32_t clear_value = 1;
int stop_thread = 0;

// ─────────────────────────────────────────────
// Signal handler voor Ctrl+C
void handle_sigint(int sig) {
    printf("\nSIGINT ontvangen, programma wordt afgesloten...\n");
    stop_thread = 1;
}

// ─────────────────────────────────────────────
void write_sprite_to_frame_info(volatile uint64_t *frame_info_arr, int index, uint16_t x, uint16_t y, uint32_t sprite_id) {
    uint64_t base_value = ((uint64_t)x << 22) | ((uint64_t)y << 11) | sprite_id;
    frame_info_arr[index] = base_value;
    printf("Frame info [%d]: X=%u, Y=%u, ID=%u\n", index, x, y, sprite_id);
    printf("  Value (hex): 0x%016llX\n", base_value);
}

void update_and_write_animated_sprite(volatile uint64_t *frame_info_arr, uint32_t sprite_id_to_use) {
    static uint16_t s_sprite_x;
    static uint16_t s_sprite_y;
    static int s_direction;
    static int s_initialized = 0;

    if (!s_initialized) {
        s_sprite_x = 120;
        s_sprite_y = 400;
        s_direction = 1;
        s_initialized = 1;
    }

    write_sprite_to_frame_info(frame_info_arr, 0, s_sprite_x, s_sprite_y, sprite_id_to_use);

    if (s_direction == 1) {
        if (s_sprite_x == 2050) {
            s_direction = -1;
            s_sprite_x--;
        } else {
            s_sprite_x++;
        }
    } else {
        if (s_sprite_x == 120) {
            s_direction = 1;
            s_sprite_x++;
        } else {
            s_sprite_x--;
        }
    }
}

void distribute_sprites_over_pipelines(volatile uint64_t *frame_infos[NUM_PIPELINES]) {
    const int TOTAL_STATIC_SPRITES = 15;
    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 400;
    const uint16_t X_START = 133;
    const uint16_t Y_START = 50;
    const uint16_t X_MAX = 2050 - SPRITE_WIDTH;
    const uint16_t Y_MAX = 1080 - SPRITE_HEIGHT;

    int sprites_in_pipeline[NUM_PIPELINES] = {0};
    uint16_t x = X_START;
    uint16_t y = Y_START;

    for (int sprite_idx = 0; sprite_idx < TOTAL_STATIC_SPRITES; sprite_idx++) {
        int pipeline = sprite_idx % NUM_PIPELINES;
        int index_in_pipeline = sprites_in_pipeline[pipeline];

        write_sprite_to_frame_info(frame_infos[pipeline], index_in_pipeline, x, y, 1);
        sprites_in_pipeline[pipeline]++;

        x += SPRITE_WIDTH;
        if (x > X_MAX) {
            x = X_START;
            y += SPRITE_HEIGHT;
            if (y > Y_MAX) {
                y = Y_START;
            }
        }
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        frame_infos[i][sprites_in_pipeline[i]] = 0xFFFFFFFFFFFFFFFF;
    }
}

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
        int ret = poll(&fds, 1, 1000); // Check elke seconde op interrupt of stop_thread
        if (ret > 0 && (fds.revents & POLLIN)) {
            handleIRQ(frame_infos);
        } else if (ret < 0) {
            perror("poll");
            break;
        }
    }
}

// ─────────────────────────────────────────────
int main() {
    // Ctrl+C handler activeren
    signal(SIGINT, handle_sigint);

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    void *lookup_table_ptrs[NUM_PIPELINES] = {NULL};
    void *frame_info_ptrs[NUM_PIPELINES] = {NULL};
    volatile uint64_t *lookup_tables[NUM_PIPELINES] = {NULL};
    volatile uint64_t *frame_infos[NUM_PIPELINES] = {NULL};

    for (int i = 0; i < NUM_PIPELINES; i++) {
        lookup_table_ptrs[i] = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, LOOKUP_TABLE_ADDRS[i]);
        if (lookup_table_ptrs[i] == MAP_FAILED) {
            perror("mmap lookup table");
            close(fd);
            return -1;
        }
        lookup_tables[i] = (volatile uint64_t *)lookup_table_ptrs[i];

        frame_info_ptrs[i] = mmap(NULL, FRAME_INFO_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd, FRAME_INFO_ADDRS[i]);
        if (frame_info_ptrs[i] == MAP_FAILED) {
            perror("mmap frame info");
            close(fd);
            return -1;
        }
        frame_infos[i] = (volatile uint64_t *)frame_info_ptrs[i];
    }

    const uint16_t SPRITE_WIDTH = 400;
    const uint16_t SPRITE_HEIGHT = 400;
    uint64_t base_lookup_value = ((uint64_t)SPRITE_DATA_BASE << 23) |
                                 ((uint64_t)SPRITE_HEIGHT << 12) |
                                 (SPRITE_WIDTH);

    for (int i = 0; i < NUM_PIPELINES; i++) {
        lookup_tables[i][1] = base_lookup_value;
    }

    distribute_sprites_over_pipelines(frame_infos);

    uio_fd = open("/dev/uio0", O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
    }
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear pending interrupt");
        close(uio_fd);
    }

    irqHandlerThread(frame_infos);

    for (int i = 0; i < NUM_PIPELINES; i++) {
        munmap(lookup_table_ptrs[i], LOOKUP_TABLE_SIZE);
        munmap(frame_info_ptrs[i], FRAME_INFO_SIZE);
    }

    close(fd);
    if (uio_fd >= 0) close(uio_fd);

    printf("Succesvol afgesloten en geheugen vrijgegeven.\n");
    return 0;
}
