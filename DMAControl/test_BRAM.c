#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/interrupt.h>

#define BRAM_BASE_ADDR 0x40000000  // Fysiek adres van BRAM
#define BRAM_SIZE      0x10000     // Grootte van de BRAM (pas aan indien nodig)
#define BRAM_INTERRUPT 63

volatile unsigned int *bram = NULL;


handle_BRAM_interrupt(int sig) {
    // Dit is de interrupt handler voor de BRAM
    printf("Interrupt ontvangen van BRAM!\n");
    
}

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
    bram = (unsigned int *) bram_ptr;

    struct sigaction sa;
    sa.sa_handler = handle_BRAM_interrupt;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGIO, &sa, NULL) == -1) {
        perror("sigaction");
        munmap(bram_ptr, BRAM_SIZE);
        close(fd);
        return -1;
    }

    if(fcntl(fd, F_SETOWN, getpid()) == -1) {
        perror("fcntl F_SETOWN");
        munmap(bram_ptr, BRAM_SIZE);
        close(fd);
        return -1;
    }

    if(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | FASYNC) == -1) {
        perror("fcntl F_SETFL");
        munmap(bram_ptr, BRAM_SIZE);
        close(fd);
        return -1;
    }
    bram[0] = 0xDEADBEEF;
    bram[1] = 0x12345678;

    printf("Data naar BRAM geschreven!\n");

    // Lees de waarden terug en print ze
    printf("Gelezen van BRAM[0]: 0x%X\n", bram[0]);
    printf("Gelezen van BRAM[1]: 0x%X\n", bram[1]);

    printf("Wacht op interrupt...\n");
    while(1) {
        // Wacht op een interrupt
        pause();
    }


    // Opruimen
    munmap(bram_ptr, BRAM_SIZE);
    close(fd);

    return 0;
}
