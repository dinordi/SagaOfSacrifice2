#include <filesystem>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdexcept>
#include <cstring>

#include "Renderer.h"
#include "fpga/spriteloader.h"

#define MAX_SPRITE_WIDTH 512
#define MAX_SPRITE_HEIGHT 512

Renderer::Renderer(const std::filesystem::path& basePath)
    : uio_fd(-1)
{
    init_frame_infos();
    loadAllSprites(basePath);
    init_lookup_tables();
    
    initUIO();
}

int Renderer::loadSprite(const std::string& img_path, uint32_t* sprite_data, uint32_t* phys_addr_out) {
    SpriteLoader spriteLoader;

    int width = 0, height = 0;
    size_t sprite_size = 0;
    const char* png_file = img_path.c_str();

    if (spriteLoader.load_png(png_file, sprite_data, &width, &height, &sprite_size) != 0) {
        std::cerr << "Failed to load PNG file: " << img_path << std::endl;
        return -1;
    }

    if (spriteLoader.map_sprite_to_memory(png_file, phys_addr_out, sprite_data, sprite_size) != 0) {
        std::cerr << "Failed to map sprite to memory: " << img_path << std::endl;
        return -2;
    }

    std::cout << "Sprite '" << img_path << "' mapped to address: 0x"
              << std::hex << *phys_addr_out << std::dec << std::endl;
    return 0;
}

void Renderer::loadAllSprites(const std::filesystem::path& basePath) {

    uint32_t* sprite_data = new uint32_t[MAX_SPRITE_WIDTH * MAX_SPRITE_HEIGHT];
    uint32_t phys_addr = SPRITE_DATA_BASE;
    
    for (const auto& entry : std::filesystem::directory_iterator(basePath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            std::filesystem::path fullPath = entry.path();
            std::string fileStem = fullPath.stem().string();  // "player" uit "player.png"

            int result = loadSprite(fullPath.string(), sprite_data, &phys_addr);

            if (result == 0) {
                spriteAddressMap[fileStem] = phys_addr;
            } else {
                std::cerr << "Failed to load sprite: " << fullPath << std::endl;
            }
        }
    }

    delete[] sprite_data;
}

void Renderer::initUIO() {
    uio_fd = open("/dev/uio0", O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
        throw std::runtime_error("Failed to open UIO device");
    }

    uint32_t clear_value = 1;
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear pending interrupt");
        close(uio_fd);
        throw std::runtime_error("Failed to clear pending interrupt");
    }

    std::cout << "Starting thread to handle interrupts..." << std::endl;
    irq_thread = std::thread(&Renderer::irqHandlerThread, this);
    if (!irq_thread.joinable()) {
        perror("Failed to create IRQ handler thread");
        close(uio_fd);
        throw std::runtime_error("Failed to create IRQ handler thread");
    }

    std::cout << "IRQ handler thread started." << std::endl;
}

Renderer::~Renderer()
{
    // Signal thread to stop
    stop_thread = true;
    
    // Wait for thread to finish first
   if (irq_thread.joinable()) {
       irq_thread.join();
   }

    // NOW it's safe to close the fd
    if (uio_fd >= 0) {
        close(uio_fd);
        uio_fd = -1;
    }

    
    // Clean up memory mappings
    for (int i = 0; i < NUM_PIPELINES; i++) {
        if (frame_info_ptrs[i] != nullptr && frame_info_ptrs[i] != MAP_FAILED) {
            munmap(frame_info_ptrs[i], FRAME_INFO_SIZE);
            frame_info_ptrs[i] = nullptr;
        }
    }
    
    std::cout << "Renderer cleanup completed" << std::endl;
}

void Renderer::handleIRQ()
{
    uint32_t irq_count;
    uint32_t clear_value = 1;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }
    // Clear the interrupt flag by writing to the UIO device
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear interrupt");
    }
    distribute_sprites_over_pipelines();
    //std::cout << "Interrupt received! IRQ count: " << irq_count << std::endl;
    //update_and_write_animated_sprite(1);

}

void Renderer::irqHandlerThread()
{
    struct pollfd fds;
    fds.fd = uio_fd;
    fds.events = POLLIN;
    stop_thread = 0;
    std::cout << "IRQ thread running, waiting for interrupts on fd: " << uio_fd << " " << stop_thread << std::endl;

    while (!stop_thread) {
        if (uio_fd >= 0)
        {
            int ret = poll(&fds, 1, 1000); // 1 second timeout instead of -1
            
            if (ret > 0 && (fds.revents & POLLIN)) {
                std::cout << "Interrupt detected!" << std::endl;
                handleIRQ();
            } else if (ret < 0) {
                if (!stop_thread) {  // Only log if not shutting down
                    perror("poll");
                }
                break;
            }
            // ret == 0 means timeout, check stop_thread and continue
        }
    }
    
    std::cout << "IRQ thread exiting" << std::endl;
}

void Renderer::init_lookup_tables() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        throw std::runtime_error("Cannot open /dev/mem");
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        lookup_table_ptrs[i] = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, LOOKUP_TABLE_ADDRS[i]);
        if (lookup_table_ptrs[i] == MAP_FAILED) {
            perror("mmap lookup table");
            throw std::runtime_error("mmap failed");
        }

        lookup_tables[i] = (volatile uint64_t *)(lookup_table_ptrs[i]);
        // <<Write lookup table values here>>
        write_lookup_table_entry(lookup_tables[i], 1, SPRITE_DATA_BASE, SPRITE_WIDTH, SPRITE_HEIGHT);
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        munmap(lookup_table_ptrs[i], LOOKUP_TABLE_SIZE);
    }
    close(fd);
}

void Renderer::init_frame_infos() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        throw std::runtime_error("Cannot open /dev/mem");
    }

    for (int i = 0; i < NUM_PIPELINES; i++) {
        frame_info_ptrs[i] = mmap(NULL, FRAME_INFO_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd, FRAME_INFO_ADDRS[i]);
        if (frame_info_ptrs[i] == MAP_FAILED) {
            perror("mmap frame info");
            throw std::runtime_error("mmap failed");
        }

        frame_infos[i] = (volatile uint64_t *)(frame_info_ptrs[i]);
        frame_infos[i][0] = 0xFFFFFFFFFFFFFFFF; // Initialize first entry to end marker
    }

    close(fd);
}

void Renderer::distribute_sprites_over_pipelines() {
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

// Lookup table entry schrijven
void Renderer::write_lookup_table_entry(volatile uint64_t *lookup_table, int index, uint32_t sprite_data_base, uint16_t width, uint16_t height) {
    if (lookup_table == nullptr) {
        printf("Error: lookup_table pointer is NULL\n");
        return; // Of: throw std::runtime_error("lookup_table pointer is NULL");
    }

    uint64_t value = ((uint64_t)sprite_data_base << 23) |
                     ((uint64_t)height << 12) |
                     width;

    lookup_table[index] = value;

    printf("Lookup Table [%d]:\n", index);
    printf("  Base Addr = 0x%08X\n", sprite_data_base);
    printf("  Width     = %u\n", width);
    printf("  Height    = %u\n", height);
    printf("  Value(hex)= 0x%016llX\n", value);
}

// ─────────────────────────────────────────────
void Renderer::write_sprite_to_frame_info(volatile uint64_t *frame_info_arr, int index, uint16_t x, uint16_t y, uint32_t sprite_id) {
    if (frame_info_arr == nullptr) {
        printf("Error: frame_info_arr pointer is NULL\n");
        return; // Of: throw std::runtime_error("frame_info_arr pointer is NULL");
    }

    uint64_t base_value = ((uint64_t)x << 22) | ((uint64_t)y << 11) | sprite_id;
    frame_info_arr[index] = base_value;

    //printf("Frame info [%d]: X=%u, Y=%u, ID=%u\n", index, x, y, sprite_id);
    //printf("  Value (hex): 0x%016llX\n", base_value);
}
