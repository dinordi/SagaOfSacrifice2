#include "petalinux/Renderer.h"

Renderer::Renderer() : stop_thread(false)
{
    // Open the UIO device
    uio_fd = open("/dev/uio0", O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
        throw std::runtime_error("Failed to open UIO device");
    }

    // Clear any pending interrupts at the start by writing to the UIO device
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear pending interrupt");
        close(uio_fd);
        throw std::runtime_error("Failed to clear pending interrupt");
    }

    // Create a thread to handle interrupts
    std::thread irq_thread(&Renderer::irqHandlerThread, this);

    // Memory map the address of the DMA AXI IP via its AXI lite control interface register block
    ddr_memory = open("/dev/mem", O_RDWR | O_SYNC);
    if (ddr_memory < 0) {
        perror("Failed to open /dev/mem");
        close(uio_fd);
        throw std::runtime_error("Failed to open /dev/mem");
    }

    dma_virtual_addr = static_cast<unsigned int*>(mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x40400000));
    if (dma_virtual_addr == MAP_FAILED) {
        perror("Failed to mmap DMA virtual address");
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to mmap DMA virtual address");
    }
    virtual_src_addr = static_cast<unsigned int*>(mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x0e000000));
    if (virtual_src_addr == MAP_FAILED) {
        perror("Failed to mmap source address");
        munmap(dma_virtual_addr, 4096);
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to mmap source address");
    }
    // Load the sprite data
    sprite = load_sprite("/home/root/SagaOfSacrifice2/SOS/assets/sprites/tung.png");
    total_size = sprite.size();
    num_chunks = (total_size + CHUNK_SIZE - 1) / CHUNK_SIZE; // Calculate the number of chunks needed
    printf("Sprite size: %zu bytes, Number of chunks: %zu\n", total_size, num_chunks);
    // Copy the sprite data to the virtual source address
    for(int i = 0; i < 64; i++) {
        uint32_t rgba = (sprite[i] << 24) | (sprite[i+1] << 16) | (sprite[i+2] << 8) | sprite[i+3];
        virtual_src_addr[i%8] = rgba;
    }


    //BRAM
    // Map het fysieke BRAM-adres naar gebruikersruimte
    bram_ptr = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, BRAM_BASE_ADDR);
    if (bram_ptr == MAP_FAILED) {
        perror("mmap mislukt");
        close(ddr_memory);
        throw std::runtime_error("Failed to mmap BRAM");
    }

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

void Renderer::init()
{
    
}

void Renderer::render(std::vector<Object*> objects)
{
    
}

BRAMDATA Renderer::readBRAM()
{
    volatile unsigned int *bram = (unsigned int *) bram_ptr;

    // Y - 11 bits
    int y = bram[0] & 0x7FF; // Read the first 11 bits for Y coordinate
    int ID = (bram[0] >> 11) & 0x7F; // Read the next 11 bits for ID

    BRAMDATA bramData;
    bramData.y = y;
    bramData.id = ID;

    return bramData;
}

void Renderer::dmaTransfer()
{
    BRAMDATA brData = readBRAM(); // The second horizontal line (y=2)

    size_t sprite_width = 64;
    size_t bytes_per_pixel = 4;
    size_t line_offset = sprite_width * bytes_per_pixel; // Offset for one line in bytes
    size_t offset = brData.y * line_offset; // Calculate the offset for the second line
    size_t bytes_to_transfer = line_offset; // Transfer the entire line

    // Ensure the offset and bytes_to_transfer are within the sprite's total size
    if (offset + bytes_to_transfer > total_size) {
        fprintf(stderr, "Error: Line exceeds sprite bounds.\n");
        return;
    }

    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RESET_DMA);
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);

    // Set DMA source address to the calculated offset
    write_dma(dma_virtual_addr, MM2S_SRC_ADDRESS_REGISTER, 0x0e000000 + offset);

    // Start DMA transfer for the line
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);
    write_dma(dma_virtual_addr, MM2S_TRNSFR_LENGTH_REGISTER, bytes_to_transfer);

    // Wait for DMA completion
    dma_mm2s_sync(dma_virtual_addr);
    dma_mm2s_status(dma_virtual_addr);

}

void Renderer::handleIRQ()
{
    uint32_t irq_count;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }

    printf("Interrupt received! IRQ count: %u\n", irq_count);
    //Start DMA transfer
    dmaTransfer();


    // Clear the interrupt flag by writing to the UIO device
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear interrupt");
    }
}

void Renderer::irqHandlerThread()
{
    struct pollfd fds;
    fds.fd = uio_fd;
    fds.events = POLLIN;

    while (!stop_thread) {
        int ret = poll(&fds, 1, -1); // Wait indefinitely for an event
        if (ret > 0 && (fds.revents & POLLIN)) {
            if (fds.revents & POLLIN) {
                handleIRQ();
            }
        } else if (ret < 0) {
            perror("poll");
            break;
        }
    }
}