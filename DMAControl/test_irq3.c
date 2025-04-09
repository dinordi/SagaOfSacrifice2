#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define INTERRUPT_DEVICE "/dev/uio0"

int main() {
    int uio_fd;
    uint32_t irq_count;

    // Open the UIO device
    uio_fd = open(INTERRUPT_DEVICE, O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
        return -1;
    }

    // Clear any pending interrupts at the start by writing to the UIO device
    uint32_t clear_value = 1; // Value to acknowledge/clear the interrupt
    if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
        perror("Failed to clear pending interrupt");
        close(uio_fd);
        return -1;
    }

    printf("Wacht op interrupt...\n");

    while (1) {
        // Wait for an interrupt
        if (read(uio_fd, &irq_count, sizeof(irq_count)) != sizeof(irq_count)) {
            perror("read");
            break;
        }

        printf("Interrupt ontvangen! IRQ count: %u\n", irq_count);

        // Clear the interrupt flag by writing to the UIO device
        if (write(uio_fd, &clear_value, sizeof(clear_value)) != sizeof(clear_value)) {
            perror("Failed to clear interrupt");
            break;
        }

        // Eventueel je FIFO hier uitlezen...
    }

    // Close the UIO device
    close(uio_fd);
    return 0;
}