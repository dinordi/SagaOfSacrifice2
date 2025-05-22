#include "Renderer.h"

// Define MM2S_DMA_BASE_ADDR with the appropriate value
#define MM2S_DMA_BASE_ADDR 0x40400000 // Replace with the correct base address for your hardware

Renderer::Renderer(const std::string& img_path) : stop_thread(false), 
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

    // Open /dev/mem for BRAM and memory access
    ddr_memory = open("/dev/mem", O_RDWR | O_SYNC);
    if (ddr_memory < 0) {
        perror("Failed to open /dev/mem");
        close(uio_fd);
        throw std::runtime_error("Failed to open /dev/mem");
    }
    
    // Map lookup table BRAM
    lookup_table_ptr = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_SHARED, ddr_memory, LOOKUP_TABLE_ADDR);
    if (lookup_table_ptr == MAP_FAILED) {
        perror("Failed to map lookup table BRAM");
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to map lookup table BRAM");
    }
    
    // Map frame info BRAM
    frame_info_ptr = mmap(NULL, FRAME_INFO_SIZE, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ddr_memory, FRAME_INFO_ADDR);
    if (frame_info_ptr == MAP_FAILED) {
        perror("Failed to map frame info BRAM");
        munmap(lookup_table_ptr, LOOKUP_TABLE_SIZE);
        close(ddr_memory);
        close(uio_fd);
        throw std::runtime_error("Failed to map frame info BRAM");
    }
    
    // Get pointers to the BRAMs as 64-bit words
    lookup_table = static_cast<volatile uint64_t*>(lookup_table_ptr);
    frame_info = static_cast<volatile uint64_t*>(frame_info_ptr);
    
    
    // Use a properly page-aligned physical address
    uint32_t phys_addr = 0x0e000000;  // Ensure this is page-aligned (multiple of 0x1000)
    sprite_base_addr = phys_addr;     // Store base address for future reference
    
    const char *png_file = img_path.c_str();
    std::cout << "PNG file path img_path: " << img_path << std::endl;
    
    SpriteLoader spriteLoader;
    uint32_t *sprite_data = nullptr;
    int width = 0, height = 0;
    size_t sprite_size = 0;
    
    // Load the PNG file
    if (spriteLoader.load_png(png_file, &sprite_data, &width, &height, &sprite_size) != 0) {
        perror("Failed to load PNG file");
        close(uio_fd);
        throw std::runtime_error("Failed to load PNG file");
    }
    
    // Store sprite properties from actual loaded data
    sprite_width = width;
    sprite_height = height;
    bytes_per_pixel = 4;  // RGBA format = 4 bytes per pixel
    line_offset = sprite_width * bytes_per_pixel;
    total_size = sprite_size;
    
    std::cout << "Loaded sprite: " << sprite_width << "x" << sprite_height 
              << ", " << bytes_per_pixel << " bytes per pixel" << std::endl;
    
    // Map the sprite data to physical memory
    if (spriteLoader.map_sprite_to_memory(png_file, &phys_addr, sprite_data, sprite_size) != 0) {
        spriteLoader.free_sprite_data(sprite_data);
        perror("Failed to map sprite to memory");
        close(uio_fd);
        throw std::runtime_error("Failed to map sprite to memory");
    }
    
    std::cout << "Sprite mapped to physical memory: 0x" << std::hex << sprite_base_addr 
              << " - 0x" << phys_addr << std::dec << std::endl;
    
    // Calculate how many lines/rows are in the sprite
    sprite_lines = sprite_height;
    
    // Free temporary sprite data (it's now in physical memory)
    spriteLoader.free_sprite_data(sprite_data);
    sprite_data = nullptr;  // Avoid dangling pointers

    BRAMDATA brData = {0, 0};
    sprite_width = 400;
    bytes_per_pixel = 4;
    line_offset = sprite_width * bytes_per_pixel; // Offset for one line in bytes
    offset = brData.y * line_offset; // Calculate the offset for the second line
    bytes_to_transfer = line_offset; // Transfer the entire line

    // After loading sprite data, set up the lookup table entry
    const uint16_t SPRITE_WIDTH = 400;  // Use your actual sprite width
    const uint16_t SPRITE_HEIGHT = 400; // Use your actual sprite height
    
    // Create lookup table entry with correct field placement
    uint64_t base_lookup_value = ((uint64_t)SPRITE_DATA_BASE << 23) | 
                                 (SPRITE_HEIGHT << 12) | 
                                 (SPRITE_WIDTH);
                                 
    lookup_table[1] = base_lookup_value;  // Using sprite ID 1
    
    // Add termination marker
    frame_info[1] = 0xFFFFFFFFFFFFFFFF;

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

    // Unmap memory regions
    if (lookup_table_ptr != MAP_FAILED) {
        munmap(lookup_table_ptr, LOOKUP_TABLE_SIZE);
    }
    if (frame_info_ptr != MAP_FAILED) {
        munmap(frame_info_ptr, FRAME_INFO_SIZE);
    }


    close(ddr_memory);
    close(uio_fd);

}

void Renderer::init()
{
    
}

void Renderer::render(std::vector<std::shared_ptr<Object>>& objects)
{
    
}

BRAMDATA Renderer::readBRAM()
{
    volatile unsigned int *bram = (unsigned int *) bram_ptr;
    if (bram == nullptr) {
        perror("Failed to map BRAM");
        throw std::runtime_error("Failed to map BRAM");
    }
    // Y - 11 bits
    unsigned int ID = bram[0] & 0x7FF; // Read the first 11 bits for Y coordinate
    unsigned int y = (bram[0] >> 11) & 0x7FF; // Read the next 11 bits for ID
    
    // Unmap the BRAM pointer
    // munmap(bram_ptr, BRAM_SIZE);

    BRAMDATA bramData;
    bramData.y = y;
    bramData.id = ID;

    return bramData;
}

void Renderer::dmaTransfer()
{
    // BRAMDATA brData = readBRAM();
    BRAMDATA brData = {0, 0}; // Placeholder for BRAM data
   
    // Only check DMA status if there was a previous error, skip during normal operation
    static bool had_previous_error = false;
    if (had_previous_error) {
        uint32_t status = read_dma(dma_virtual_addr, MM2S_STATUS_REGISTER);
        if (status & 0x70) {  // Error bits
            // Only reset on error
            write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RESET_DMA);
            write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ | RUN_DMA);
            had_previous_error = false;
        }
    }
   
    // No need to reset or initialize for every transfer
    // Just set source address and length
   
    // Set source address directly
    uint32_t src_addr = 0x0e000000 + (brData.y * sprite_width * bytes_per_pixel);
    write_dma(dma_virtual_addr, MM2S_SRC_ADDRESS_REGISTER, src_addr);
   
    // Start transfer immediately by setting length
    write_dma(dma_virtual_addr, MM2S_TRNSFR_LENGTH_REGISTER, sprite_width * bytes_per_pixel);
 
    // No waiting for completion - assume DMA hardware completes the transfer
    // The next interrupt will either find the DMA idle or in error state
    // and handle it appropriately
   
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

    // if((irq_count % 24000) == 0)
    // {
    //     std::cout << irq_count << std::endl;
    // }
    //printf("Interrupt received! IRQ count: %u\n", irq_count);
    //Start DMA transfer
    // dmaTransfer();
    std::cout << "Interrupt received! IRQ count: " << irq_count << std::endl;
    update_and_write_animated_sprite(1);

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


void Renderer::write_sprite_to_frame_info(int index, uint16_t x, uint16_t y, uint32_t sprite_id) {
    // Construct the 64-bit value
    // X: bits 33-22 (12 bits), Y: bits 21-11 (11 bits), Sprite ID: bits 10-0 (11 bits)
    uint64_t base_value = ((uint64_t)x << 22) | ((uint64_t)y << 11) | sprite_id;
    frame_info[index] = base_value;
    
    std::cout << "Frame info [" << index << "]: X=" << x << ", Y=" << y 
              << ", ID=" << sprite_id << std::endl;
    std::cout << "  Value (hex): 0x" << std::hex << base_value << std::dec << std::endl;
}

void Renderer::update_and_write_animated_sprite(uint32_t sprite_id_to_use) {
    // Write current sprite state to index 0
    write_sprite_to_frame_info(0, sprite_x, sprite_y, sprite_id_to_use);

    // Update X position for next frame
    if (sprite_direction == 1) {
        if (sprite_x == 2050) {
            sprite_direction = -1;
            sprite_x--;
        } else {
            sprite_x++;
        }
    } else { // sprite_direction == -1
        if (sprite_x == 120) {
            sprite_direction = 1;
            sprite_x++;
        } else {
            sprite_x--;
        }
    }
}