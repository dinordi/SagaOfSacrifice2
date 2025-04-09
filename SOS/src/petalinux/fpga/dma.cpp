#include "petalinux/fpga/dma.h"

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