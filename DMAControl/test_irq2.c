#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define INTERRUPT_DEVICE "/dev/uio0"

int main() {
    int uio_fd;
    uint32_t irq_count;

    uio_fd = open(INTERRUPT_DEVICE, O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
        return -1;
    }

    printf("Wacht op interrupt...\n");

    while (1) {
        // Blokkeert tot er een interrupt optreedt
        if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
            perror("read");
            break;
        }

        printf("Interrupt ontvangen! IRQ count: %u\n", irq_count);

        // Eventueel je FIFO hier uitlezen...
    }

    close(uio_fd);
    return 0;
}
