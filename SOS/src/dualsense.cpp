#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <linux/input.h>

int main() {
    // Open the joystick device (usually /dev/input/js0)
    int js = open("/dev/input/js0", O_RDONLY);
    if (js == -1) {
        perror("Error opening joystick device");
        return 1;
    }

    // Variables for joystick event
    struct js_event js_event;

    // Read joystick events in a loop
    while (true) {
        // Read a joystick event
        if (read(js, &js_event, sizeof(struct js_event)) > 0) {
            // Process button events
            if (js_event.type == JS_EVENT_BUTTON) {
                std::cout << "Button " << (int)js_event.number
                          << (js_event.value ? " pressed" : " released") << std::endl;
            }
            // Process axis events
            else if (js_event.type == JS_EVENT_AXIS) {
                std::cout << "Axis " << (int)js_event.number
                          << " value: " << js_event.value << std::endl;
            }
        }
    }

    // Close the joystick device
    close(js);

    return 0;
}