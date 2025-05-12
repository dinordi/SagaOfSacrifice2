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
 
    void *bram_void_ptr = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BRAM_BASE);
    if (bram_void_ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }
    // Cast to a 64-bit pointer type
    volatile uint64_t *bram = (volatile uint64_t *)bram_void_ptr;
    
    
    // Image 1: X=400, Y=400, Width=400, Height=400, ID=160
    // Combine all fields into a single 64-bit value
    // Format: X(11) + Y(11) + Width(11) + Height(11) + ID(11) bits
    uint64_t image1_value = ((uint64_t)400 << 44) |        // X position
                             ((uint64_t)400 << 33) |        // Y position
                             ((uint64_t)400 << 22) |      // Width
                             ((uint64_t)400 << 11) |      // Height
                             ((uint64_t)160);               // ID
    // Write the 64-bit value directly
    bram[0] = image1_value;

    // Image 2: X=100, Y=100, Width=200, Height=200, ID=2
    uint64_t image2_value = ((uint64_t)800 << 44) |        // X position
                             ((uint64_t)400 << 33) |        // Y position
                             ((uint64_t)200 << 22) |      // Width
                             ((uint64_t)200 << 11) |      // Height
                             ((uint64_t)2);                // ID
    // Write the second 64-bit value directly
    bram[1] = image2_value;
    
    // Termination value (bits 54-33 all set to 1)
    // For termination: bits 54-33 (X and Y fields) must all be 1
    uint64_t termination_value = ((uint64_t)0x3FFFFF << 33); // Set bits 54-33 to all 1's (22 bits for X and Y)
    // Write the 64-bit termination value directly after the last image descriptor
    bram[2] = termination_value;
 
    munmap(bram_void_ptr, BRAM_SIZE); // Use the original void* for munmap
    close(fd);
    printf("Successfully wrote 400x400 image, 200x200 image, and termination to BRAM\n");
    return 0;
}