#include "petalinux/Renderer.h"

// Define MM2S_DMA_BASE_ADDR with the appropriate value
#define MM2S_DMA_BASE_ADDR 0x40400000 // Replace with the correct base address for your hardware

Renderer::Renderer() : stop_thread(false), 
                           uio_fd(-1),
                           ddr_memory(-1),
                           dma_virtual_addr(NULL),
                           virtual_src_addr(NULL),
                           total_size(0),
                           num_chunks(0),
                           bram_ptr(NULL)
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

    
    // Memory map the address of the DMA AXI IP via its AXI lite control interface register block
    ddr_memory = open("/dev/mem", O_RDWR | O_SYNC);
    if (ddr_memory < 0) {
        perror("Failed to open /dev/mem");
        close(uio_fd);
        throw std::runtime_error("Failed to open /dev/mem");
    }
    
    // Map DMA control registers
    dma_virtual_addr = (unsigned int*)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, MM2S_DMA_BASE_ADDR);
    if (dma_virtual_addr == MAP_FAILED) {
        perror("Failed to map DMA registers");
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to map DMA registers");
    }
    
    uint32_t phys_addr = 0x014B2000;  // Start fysiek adres (voorbeeld)
    const char *png_file = "/home/root/SagaOfSacrifice2/SOS/assets/sprites/Solid_blue.png";  // Pad naar je PNG bestand
    
    SpriteLoader spriteLoader;
    uint32_t *sprite_data = nullptr;
    int width = 0, height = 0;
    size_t sprite_size = 0;
    
    // Eerst laden we het PNG bestand
    if (spriteLoader.load_png(png_file, &sprite_data, &width, &height, &sprite_size) != 0) {
        perror("Failed to load PNG file");
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to load PNG file");
    }
    
    // Store the total size for later use
    total_size = sprite_size;
    
    // Daarna mappen we het naar het geheugen
    if (spriteLoader.map_sprite_to_memory(png_file, &phys_addr, sprite_data, sprite_size) != 0) {
        spriteLoader.free_sprite_data(sprite_data);
        perror("Failed to map sprite to memory");
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to map sprite to memory");
    }
    
    // Na het mappen kunnen we de sprite data vrijgeven
    spriteLoader.free_sprite_data(sprite_data);

    BRAMDATA brData = {0, 0};
    sprite_width = 400;
    bytes_per_pixel = 4;
    line_offset = sprite_width * bytes_per_pixel; // Offset for one line in bytes
    offset = brData.y * line_offset; // Calculate the offset for the second line
    bytes_to_transfer = line_offset; // Transfer the entire line

    // Create a thread to handle interrupts
    irq_thread = std::thread(&Renderer::irqHandlerThread, this);
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
    void *bram_ptr = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, BRAM_BASE_ADDR);
    if (bram_ptr == MAP_FAILED) {
        perror("mmap mislukt");
	    close(ddr_memory);
        throw std::runtime_error("Failed to mmap BRAM");
    }
    volatile unsigned int *bram = (unsigned int *) bram_ptr;
    if (bram == nullptr) {
        perror("Failed to map BRAM");
        throw std::runtime_error("Failed to map BRAM");
    }
    // Y - 11 bits
    unsigned int ID = bram[0] & 0x7FF; // Read the first 11 bits for Y coordinate
    unsigned int y = (bram[0] >> 11) & 0x7FF; // Read the next 11 bits for ID
    
    // Unmap the BRAM pointer
    munmap(bram_ptr, BRAM_SIZE);

    BRAMDATA bramData;
    bramData.y = y;
    bramData.id = ID;

    return bramData;
}

void Renderer::dmaTransfer()
{
    // BRAMDATA brData = readBRAM();
    // std::cout << "Read BRAM: Y=" << brData.y << ", ID=" << brData.id << std::endl;
   
    // BRAMDATA brData = {0, 0}; // Initialize to zero
 
    // Ensure the offset and bytes_to_transfer are within the sprite's total size
    if (offset + bytes_to_transfer > total_size) {
        std::cerr << "ERROR: Line exceeds sprite bounds. Offset: " << offset
                 << ", Transfer size: " << bytes_to_transfer
                 << ", Total size: " << total_size << std::endl;
        return;
    }
 
    // Check initial DMA status before reset
    // uint32_t initial_status = read_dma(dma_virtual_addr, MM2S_STATUS_REGISTER);
    // std::cout << "Initial DMA Status: 0x" << std::hex << initial_status << std::dec
    //          << " (Halted: " << ((initial_status & 0x1) ? "Yes" : "No")
    //          << ", Idle: " << ((initial_status & 0x2) ? "Yes" : "No")
    //          << ", Error: " << ((initial_status & 0x70) ? "Yes" : "No") << ")" << std::endl;
   
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RESET_DMA);
    uint32_t post_reset_status = read_dma(dma_virtual_addr, MM2S_STATUS_REGISTER);
    // std::cout << "Post-reset DMA Status: 0x" << std::hex << post_reset_status << std::dec << std::endl;
   
    if (post_reset_status & 0x70) {  // Check error bits (bits 4-6)
        std::cerr << "ERROR: DMA still in error state after reset" << std::endl;
        return;
    }
   
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    uint32_t irq_status = read_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER);
    // std::cout << "IRQ Enabled Status: 0x" << std::hex << irq_status << std::dec << std::endl;
   
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);
    uint32_t run_status = read_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER);
    // std::cout << "DMA Run Status: 0x" << std::hex << run_status << std::dec << std::endl;
 
    // Set DMA source address to the calculated offset
    uint32_t src_addr = 0x014B2000 + offset;
    write_dma(dma_virtual_addr, MM2S_SRC_ADDRESS_REGISTER, src_addr);
    uint32_t read_src_addr = read_dma(dma_virtual_addr, MM2S_SRC_ADDRESS_REGISTER);
    // std::cout << "DMA Source Address: 0x" << std::hex << read_src_addr << std::dec << std::endl;
 
    // Start DMA transfer for the line
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);
    // std::cout << "RUNDMA done" << std::endl;
   
    write_dma(dma_virtual_addr, MM2S_TRNSFR_LENGTH_REGISTER, bytes_to_transfer);
    // std::cout << "Transfer Length Set: " << bytes_to_transfer << " bytes" << std::endl;
 
    // Wait for DMA completion with iteration limit
    // std::cout << "Waiting for DMA completion..." << std::endl;
    const int MAX_ITERATIONS = 100000; // Higher number since we're not adding delays
    int iterations = 0;
    bool success = false;
   
    while (iterations < MAX_ITERATIONS) {
        uint32_t status = read_dma(dma_virtual_addr, MM2S_STATUS_REGISTER);
        if (status & 0x2) {  // Idle bit set
            success = true;
            // std::cout << "DMA transfer completed successfully after " << iterations << " iterations" << std::endl;
            break;
        }
       
        if (status & 0x70) {  // Error bits
            // std::cerr << "ERROR: DMA error detected: 0x" << std::hex << status << std::dec << std::endl;
            break;
        }
       
        // No sleep call
        iterations++;
    }
   
    if (!success) {
        // std::cerr << "ERROR: DMA transfer timed out after " << MAX_ITERATIONS << " iterations" << std::endl;
    }
   
}

void Renderer::handleIRQ()
{
    uint32_t irq_count;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }
    // Clear the interrupt flag by writing to the UIO device
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear interrupt");
    }

    if((irq_count % 24000) == 0)
    {
        std::cout << irq_count << std::endl;
    }
    //printf("Interrupt received! IRQ count: %u\n", irq_count);
    //Start DMA transfer
    dmaTransfer();


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
