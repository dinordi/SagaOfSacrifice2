#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define BRAM_BASE_ADDR 0x40000000  // Fysiek adres van BRAM
#define BRAM_SIZE      0x10000     // Grootte van de BRAM (pas aan indien nodig)

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Kan /dev/mem niet openen");
        return -1;
    }

    // Map het fysieke BRAM-adres naar gebruikersruimte
    void *bram_ptr = mmap(NULL, BRAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BRAM_BASE_ADDR);
    if (bram_ptr == MAP_FAILED) {
        perror("mmap mislukt");
        close(fd);
        return -1;
    }

    // Schrijf een waarde naar de BRAM
    volatile unsigned int *bram = (unsigned int *) bram_ptr;
    bram[0] = 0xDEADBEEF;
    bram[1] = 0x12345678;

    printf("Data naar BRAM geschreven!\n");

    // Lees de waarden terug en print ze
    printf("Gelezen van BRAM[0]: 0x%X\n", bram[0]);
    printf("Gelezen van BRAM[1]: 0x%X\n", bram[1]);

    // Opruimen
    munmap(bram_ptr, BRAM_SIZE);
    close(fd);

    return 0;
}
