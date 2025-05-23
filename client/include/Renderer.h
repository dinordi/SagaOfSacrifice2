#pragma once
#include <vector>
#include <thread>
#include <poll.h>
#include <memory>
#include "object.h"
#include "fpga/dma.h"
#include "fpga/spriteloader.h"


#define BRAM_BASE_ADDR 0x40000000  // Fysiek adres van BRAM
#define BRAM_SIZE      0x1FFF     // Grootte van de BRAM (pas aan indien nodig)

struct BRAMDATA
{
    int y;
    int id;
};


class Renderer
{
public:
    Renderer(const std::string& img_path);
    ~Renderer();

    void init();
    void render(std::vector<std::shared_ptr<Object>>& objects);
private:
    void handleIRQ();
    void irqHandlerThread();
    bool stop_thread;
    std::thread irq_thread;


    void dmaTransfer();
    BRAMDATA readBRAM();

    int uio_fd;
    uint32_t irq_count;
    uint32_t clear_value = 1; // Value to acknowledge/clear the interrupt
    
    int ddr_memory;
    unsigned int *dma_virtual_addr;
    unsigned int *virtual_src_addr;
    std::vector<unsigned char> sprite;
    const size_t CHUNK_SIZE = 32;
    size_t total_size;
    size_t num_chunks;

    // Sprite properties
    size_t sprite_width;
    size_t sprite_height;     // Height in pixels
    uint32_t sprite_base_addr; // Physical base address in memory
    size_t sprite_lines;      // Number of lines in sprite (equal to height)
    
    size_t sprite_offset;
    size_t bytes_to_transfer;
    size_t offset;
    size_t bytes_per_pixel;
    size_t line_offset;

    void* bram_ptr;

    // New BRAM management
private:
    // BRAM management
    static constexpr uint32_t FRAME_INFO_ADDR = 0x42000000;
    static constexpr uint32_t LOOKUP_TABLE_ADDR = 0x40000000;
    static constexpr uint32_t FRAME_INFO_SIZE = 0x2000;     // 8KB
    static constexpr uint32_t LOOKUP_TABLE_SIZE = 0x4000;   // 16KB
    static constexpr uint32_t SPRITE_DATA_BASE = 0x0E000000;
    
    // BRAM pointers
    void* lookup_table_ptr;
    void* frame_info_ptr;
    volatile uint64_t* lookup_table;
    volatile uint64_t* frame_info;
    
    // Sprite animation state
    uint16_t sprite_x;
    uint16_t sprite_y;
    int sprite_direction;
    
    // Helper methods
    void write_sprite_to_frame_info(int index, uint16_t x, uint16_t y, uint32_t sprite_id);
    void update_and_write_animated_sprite(uint32_t sprite_id_to_use);
};
