#include <iostream>
#include <fstream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define MM2S_CONTROL_REGISTER       0x00
#define MM2S_STATUS_REGISTER        0x04
#define MM2S_SRC_ADDRESS_REGISTER   0x18
#define MM2S_TRNSFR_LENGTH_REGISTER 0x28

#define IOC_IRQ_FLAG                1<<12
#define IDLE_FLAG                   1<<1

#define STATUS_HALTED               0x00000001
#define STATUS_IDLE                 0x00000002
#define STATUS_IOC_IRQ              0x00001000

#define HALT_DMA                    0x00000000
#define RUN_DMA                     0x00000001
#define RESET_DMA                   0x00000004
#define ENABLE_ALL_IRQ              0x00007000

unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value)
{
    virtual_addr[offset>>2] = value;
    return 0;
}

unsigned int read_dma(unsigned int *virtual_addr, int offset)
{
    return virtual_addr[offset>>2];
}

void dma_mm2s_status(unsigned int *virtual_addr)
{
    unsigned int status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    std::cout << "Memory-mapped to stream status (0x" << std::hex << status << "@0x" << MM2S_STATUS_REGISTER << "):";
    if (status & STATUS_HALTED) {
        std::cout << " Halted.\n";
    } else {
        std::cout << " Running.\n";
    }
    if (status & STATUS_IDLE) {
        std::cout << " Idle.\n";
    }
    if (status & STATUS_IOC_IRQ) {
        std::cout << " IOC interrupt occurred.\n";
    }
}

int dma_mm2s_sync(unsigned int *virtual_addr)
{
    unsigned int mm2s_status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    while(!(mm2s_status & IOC_IRQ_FLAG) || !(mm2s_status & IDLE_FLAG))
    {
        dma_mm2s_status(virtual_addr);
        mm2s_status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    }
    return 0;
}

void print_mem(void *virtual_address, int byte_count)
{
    char *data_ptr = static_cast<char*>(virtual_address);
    for(int i = 0; i < byte_count; i++) {
        printf("%02X", data_ptr[i]);
        if(i % 4 == 3) {
            printf(" ");
        }
    }
    printf("\n");
}

std::vector<unsigned char> load_sprite(const std::string &file_path)
{
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open sprite file.");
    }
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int main()
{
    std::cout << "Hello World! - Running DMA transfer test application.\n";

    std::cout << "Opening a character device file of the Zybo's DDR memory...\n";
    int ddr_memory = open("/dev/mem", O_RDWR | O_SYNC);

    std::cout << "Memory map the address of the DMA AXI IP via its AXI lite control interface register block.\n";
    unsigned int *dma_virtual_addr = static_cast<unsigned int*>(mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x40400000));

    std::cout << "Memory map the MM2S source address register block.\n";
    unsigned int *virtual_src_addr = static_cast<unsigned int*>(mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x0e000000));

    std::cout << "Loading sprite into memory...\n";
    std::vector<unsigned char> sprite = load_sprite("/home/root/SagaOfSacrifice2/SOS/assets/sprites/tiles.png");

    std::cout << "Passing a single horizontal line of the sprite to the DMA...\n";
    std::memcpy(virtual_src_addr, sprite.data(), sprite.size());

    std::cout << "Source memory block data: ";
    print_mem(virtual_src_addr, sprite.size());

    std::cout << "Reset the DMA.\n";
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RESET_DMA);
    dma_mm2s_status(dma_virtual_addr);

    std::cout << "Halt the DMA.\n";
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, HALT_DMA);
    dma_mm2s_status(dma_virtual_addr);

    std::cout << "Enable all interrupts.\n";
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    dma_mm2s_status(dma_virtual_addr);

    std::cout << "Writing source address of the data from MM2S in DDR...\n";
    write_dma(dma_virtual_addr, MM2S_SRC_ADDRESS_REGISTER, 0x0e000000);
    dma_mm2s_status(dma_virtual_addr);

    std::cout << "Run the MM2S channel.\n";
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RUN_DMA);
    dma_mm2s_status(dma_virtual_addr);

    std::cout << "Writing MM2S transfer length of " << sprite.size() << " bytes...\n";
    write_dma(dma_virtual_addr, MM2S_TRNSFR_LENGTH_REGISTER, sprite.size());
    dma_mm2s_status(dma_virtual_addr);

    std::cout << "Waiting for MM2S synchronization...\n";
    dma_mm2s_sync(dma_virtual_addr);

    dma_mm2s_status(dma_virtual_addr);

    std::cout << "\n";

    return 0;
}