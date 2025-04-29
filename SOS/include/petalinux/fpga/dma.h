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


unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value);

unsigned int read_dma(unsigned int *virtual_addr, int offset);

void dma_mm2s_status(unsigned int *virtual_addr);

int dma_mm2s_sync(unsigned int *virtual_addr);

void print_mem(void *virtual_address, int byte_count);

std::vector<unsigned char> load_sprite(const std::string &file_path);

