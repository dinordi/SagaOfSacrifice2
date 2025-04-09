#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/interrupt.h>

#define INTERRUPT_DEVICE "/dev/uio0"  // De UIO device node

// De interrupt handler functie
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    // Interrupt verwerking
    printf("Interrupt geactiveerd! IRQ: %d\n", irq);
    // Lees data uit de FIFO (bijvoorbeeld)
    // fifo_read();
    return IRQ_HANDLED;
}

int main()
{
    int uio_fd;
    unsigned int irq;
    
    // Open de UIO device
    uio_fd = open(INTERRUPT_DEVICE, O_RDWR);
    if (uio_fd < 0) {
        perror("Failed to open UIO device");
        return -1;
    }
    
    // Verkrijg de IRQ nummer van het UIO apparaat
    ioctl(uio_fd, UIO_GET_IRQ, &irq);
    
    // Registreer de interrupt handler
    if (request_irq(irq, my_irq_handler, IRQF_SHARED, "my_interrupt", NULL)) {
        perror("Failed to request IRQ");
        return -1;
    }
    
    // Wacht op de interrupt
    while (1) {
        sleep(1); // Je zou hier een select of poll kunnen gebruiken voor niet-blocking
    }

    return 0;
}
