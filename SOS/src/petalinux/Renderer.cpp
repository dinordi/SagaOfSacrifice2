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

    dma_virtual_addr = static_cast<unsigned int*>(mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x40000000));
    if (dma_virtual_addr == MAP_FAILED) {
        perror("Failed to mmap DMA virtual address");
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to mmap DMA virtual address");
    }
    virtual_src_addr = static_cast<unsigned int*>(mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x40000000));
    if (virtual_src_addr == MAP_FAILED) {
        perror("Failed to mmap source address");
        munmap(dma_virtual_addr, 4096);
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to mmap source address");
    }
    // Load the sprite data
    sprite = load_sprite("/home/root/SagaOfSacrifice2/SOS/assets/sprites/player.png");
    total_size = sprite.size();
    num_chunks = (total_size + CHUNK_SIZE - 1) / CHUNK_SIZE; // Calculate the number of chunks needed
    printf("Sprite size: %zu bytes, Number of chunks: %zu\n", total_size, num_chunks);
    // Copy the sprite data to the virtual source address
    for(int i = 0; i < 64; i++) {
        uint32_t rgba = (sprite[i] << 24) | (sprite[i+1] << 16) | (sprite[i+2] << 8) | sprite[i+3];
        virtual_src_addr[i%8] = rgba;
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

void Renderer::handleIRQ()
{
    uint32_t irq_count;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }

    printf("Interrupt received! IRQ count: %u\n", irq_count);

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