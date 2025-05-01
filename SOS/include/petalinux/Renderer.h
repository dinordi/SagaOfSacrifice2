#pragma once
#include <vector>
#include <thread>
#include <poll.h>
#include "object.h"
#include "petalinux/fpga/dma.h"
#include "petalinux/fpga/spriteloader.h"


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
    void render(std::vector<Object*> objects);
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

    size_t sprite_width;

    size_t sprite_offset;
    size_t bytes_to_transfer;
    size_t offset;
    size_t bytes_per_pixel;
    size_t line_offset;

    void* bram_ptr;
};
