#include <iostream>
#include <fstream>
#include <thread>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "Renderer.h"
#include "SpriteLoader.h"

Renderer::Renderer(const std::string& img_path)
    : stop_thread(false),
      uio_fd(-1)
{
    loadSprite(img_path);
    init_lookup_tables();
    init_frame_infos();
    initUIO();
}

void Renderer::loadSprite(const std::string& img_path) {
    SpriteLoader spriteLoader;
    uint32_t* sprite_data = nullptr;
    int width = 0, height = 0;
    size_t sprite_size = 0;
    uint32_t phys_addr = SPRITE_DATA_BASE;  // Page-aligned startadres

    const char* png_file = img_path.c_str();
    std::cout << "PNG file path img_path: " << img_path << std::endl;

    if (spriteLoader.load_png(png_file, &sprite_data, &width, &height, &sprite_size) != 0) {
        perror("Failed to load PNG file");
        throw std::runtime_error("Failed to load PNG file");
    }

    if (spriteLoader.map_sprite_to_memory(png_file, &phys_addr, sprite_data, sprite_size) != 0) {
        spriteLoader.free_sprite_data(sprite_data);
        perror("Failed to map sprite to memory");
        throw std::runtime_error("Failed to map sprite to memory");
    }

    spriteLoader.free_sprite_data(sprite_data);

    std::cout << "Sprite mapped to physical memory: 0x"
              << std::hex << phys_addr << std::dec << std::endl;
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
    // Unmap the DMA virtual address
    if (dma_virtual_addr != MAP_FAILED) {
        munmap(dma_virtual_addr, 4096);
    }
    if (virtual_src_addr != MAP_FAILED) {
        munmap(virtual_src_addr, 4096);
    }
    
    stop_thread = true;
    if (irq_thread.joinable()) {
        irq_thread.join();
    }

    close(ddr_memory);
    close(uio_fd);

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
    stop_thread = false;

    std::cout << "IRQ thread running, waiting for interrupts on fd: " << uio_fd << std::endl;

    while (stop_thread == false) {
        std::cout << "Waiting for interrupt..." << std::endl;
        int ret = poll(&fds, 1, -1); // Wait indefinitely for an event
        if (ret > 0 && (fds.revents & POLLIN)) {
            if (fds.revents & POLLIN) {
                std::cout << "Interrupt detected!" << std::endl;
                handleIRQ();
            }
        } else if (ret < 0) {
            perror("poll");
            break;
        }
    }
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

        lookup_tables[i] = reinterpret_cast<volatile uint64_t*>(lookup_table_ptrs[i]);
        // <<Write lookup table values here>>
        write_lookup_table_entry(lookup_tables[i], 1, SPRITE_DATA_BASE, SPRITE_WIDTH, SPRITE_HEIGHT);
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

        frame_infos[i] = reinterpret_cast<volatile uint64_t*>(frame_info_ptrs[i]);
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