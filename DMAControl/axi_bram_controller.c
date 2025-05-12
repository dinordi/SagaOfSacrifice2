#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
 
#define BRAM_BASE 0x80000000
#define BRAM_SIZE 0x2000  // 8 KB

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }
 
    volatile uint32_t *bram = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BRAM_BASE);
    if (bram == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }
    
    
    // Image 1: Top left corner (0,0) - 400x400 pixels, ID 1
    // Combine all fields into a single 64-bit value
    // Format: X(11) + Y(11) + Width(11) + Height(11) + ID(11) bits
    uint64_t image1_value = ((uint64_t)160 << 44) |        // X=0 (bits 54-44)
                             ((uint64_t)0 << 33) |        // Y=0 (bits 43-33)
                             ((uint64_t)400 << 22) |      // Width=400 (bits 32-22)
                             ((uint64_t)400 << 11) |      // Height=400 (bits 21-11)
                             ((uint64_t)1);               // ID=1 (bits 10-0)
    bram[0] = (uint32_t)(image1_value & 0xFFFFFFFF);      // Lower 32 bits
    bram[1] = (uint32_t)(image1_value >> 32);            // Upper 32 bits
    
    
    
    // Termination value (bits 54-33 all set to 1)
    // For termination: bits 54-33 (22 bits) must all be 1
    uint64_t termination_value = ((uint64_t)0x3FFFFF << 33); // Set bits 54-33 to all 1's
    bram[3] = (uint32_t)(termination_value & 0xFFFFFFFF);    // Lower 32 bits
    bram[4] = (uint32_t)(termination_value >> 32);          // Upper 32 bits
    
    
 
    munmap((void *)bram, BRAM_SIZE);
    close(fd);
    printf("Successfully wrote 400x400 image and termination to BRAM\n");
    return 0;
}