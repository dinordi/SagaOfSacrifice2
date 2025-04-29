#pragma once
#include <vector>
#include <thread>
#include <poll.h>
#include "object.h"
#include "petalinux/fpga/dma.h"


#define BRAM_BASE_ADDR 0x40000000  // Fysiek adres van BRAM
#define BRAM_SIZE      0x10000     // Grootte van de BRAM (pas aan indien nodig)

typedef struct BRAMDATA
{
    int y;
    int id;
};


class Renderer
{
public:
    Renderer();
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

    void* bram_ptr;
};