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
    
    // Map BRAM once during initialization to avoid mapping/unmapping on every read
    bram_ptr = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, BRAM_BASE_ADDR);
    if (bram_ptr == MAP_FAILED) {
        perror("Failed to map BRAM");
        munmap(dma_virtual_addr, 4096);
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to map BRAM");
    }
    
    uint32_t phys_addr = 0x014B2000;  // Start fysiek adres (voorbeeld)
    const char *png_file = "/home/root/SagaOfSacrifice2/SOS/assets/sprites/tung.png";  // Pad naar je PNG bestand
    
    SpriteLoader spriteLoader;
    uint32_t *sprite_data = nullptr;
    int width = 0, height = 0;
    size_t sprite_size = 0;
    
    // Eerst laden we het PNG bestand
    if (spriteLoader.load_png(png_file, &sprite_data, &width, &height, &sprite_size) != 0) {
        perror("Failed to load PNG file");
        munmap(bram_ptr, BRAM_SIZE);
        munmap(dma_virtual_addr, 4096);
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
        munmap(bram_ptr, BRAM_SIZE);
        munmap(dma_virtual_addr, 4096);
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to map sprite to memory");
    }
    
    // Na het mappen kunnen we de sprite data vrijgeven
    spriteLoader.free_sprite_data(sprite_data);
    
    std::cout << "Initializing DMA..." << std::endl;
    // Initialize DMA controller once
    // Reset DMA
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RESET_DMA);
    // Wait for reset to complete
    while (!(read_dma(dma_virtual_addr, MM2S_STATUS_REGISTER) & 0x2)) {
        // Brief wait for reset to complete
        int initial_status = read_dma(dma_virtual_addr, MM2S_STATUS_REGISTER);
        std::cout << "Initial DMA Status: 0x" << std::hex << initial_status << std::dec
             << " (Halted: " << ((initial_status & 0x1) ? "Yes" : "No")
             << ", Idle: " << ((initial_status & 0x2) ? "Yes" : "No")
             << ", Error: " << ((initial_status & 0x70) ? "Yes" : "No") << ")" << std::endl;
   
    }
    
    // Enable interrupts
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    
    // Put DMA in run state
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);

    // Pre-calculate memory parameters
    sprite_width = 400;
    bytes_per_pixel = 4;
    line_offset = sprite_width * bytes_per_pixel;
    base_address = 0x014B2000;

    // Create a thread to handle interrupts
    std::cout << "Starting thread." << std::endl;
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
    if (bram_ptr != MAP_FAILED) {
        munmap(bram_ptr, BRAM_SIZE);
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
    // Direct access to the pre-mapped BRAM memory
    volatile unsigned int *bram = (unsigned int *)bram_ptr;
    unsigned int value = bram[0];
    
    // Bitwise operations are faster than division and modulo
    BRAMDATA bramData;
    bramData.id = value & 0x7FF;         // Extract ID (first 11 bits)
    bramData.y = (value >> 11) & 0x7FF;  // Extract Y (next 11 bits)
    
    return bramData;
}

void Renderer::dmaTransfer()
{
    BRAMDATA brData = readBRAM();
    
    // Calculate the memory offset based on the Y value
    size_t offset = brData.y * line_offset;
    
    // Ensure the offset is within bounds
    if (offset + line_offset > total_size) {
        return;
    }
    
    // Set DMA source address directly
    uint32_t src_addr = base_address + offset;
    write_dma(dma_virtual_addr, MM2S_SRC_ADDRESS_REGISTER, src_addr);
    
    // Set transfer length
    write_dma(dma_virtual_addr, MM2S_TRNSFR_LENGTH_REGISTER, line_offset);
    
    // Start DMA transfer (single write)
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);
    
    // No waiting for completion - the IRQ will trigger when done
}

void Renderer::handleIRQ()
{
    uint32_t irq_count;
    if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
        perror("read");
        return;
    }

    //printf("Interrupt received! IRQ count: %u\n", irq_count);
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
