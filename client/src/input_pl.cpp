/*
* Author: Joey van Noort
* 
*/
#include "input_pl.h"

#define test_bit(bit, array) ((array[(bit) / 8] >> ((bit) % 8)) & 1)


    // Constructor: probeert het opgegeven evdev pad te openen
    EvdevController::EvdevController(const std::string& targetName) :
        devicePath_(""),
        ev_fd_(-1),
        pollfd_{-1,0,0},
        targetDeviceName_(targetName)
    {
        // Initialiseer state vectors (groot genoeg voor alle mogelijke codes)
        // KEY_MAX en ABS_MAX komen uit linux/input-event-codes.h
        currentKeyStates_.resize(KEY_MAX + 1, false);
        previousKeyStates_.resize(KEY_MAX + 1, false);
        currentAbsValues_.resize(ABS_MAX + 1, 0);

        // findAndOpenDevice();    //Throws exception
        std::cout << devicePath_ << std::endl;
        // Configureer polling
        pollfd_.fd = ev_fd_;
        pollfd_.events = POLLIN; // We willen weten wanneer er data te lezen is
        pollfd_.revents = 0;
    }

    // Destructor: sluit de file descriptor
    EvdevController::~EvdevController() {
        if (ev_fd_ >= 0) {
            close(ev_fd_);
             std::cout << "Closed evdev controller: " << devicePath_ << std::endl;
        }
    }

    void EvdevController::readInput()
    {
        update();
        if (isKeyPressed(BTN_SOUTH)) { // X knop op PlayStation layout
            std::cout << "X (BTN_SOUTH) pressed!\n";
            set_up(true);
        }
        if(isKeyDown(BTN_EAST)) { // O knop
            std::cout << "O (BTN_EAST) is down.\n";
        }
        if(isKeyPressed(BTN_DPAD_LEFT)) { // DPAD left
            std::cout << "DPAD left pressed!\n";
            set_left(isKeyPressed(true));
        }
        if(isKeyPressed(BTN_DPAD_UP)) { // DPAD up
            std::cout << "DPAD up pressed!\n";
            set_up(isKeyPressed(true));
        }
        if(isKeyPressed(BTN_DPAD_RIGHT)) { // DPAD right
            std::cout << "DPAD right pressed!\n";
            set_right(isKeyPressed(true));
        }
        
    }

    void EvdevController::findAndOpenDevice()
    {
        const char* inputDir = "/dev/input";
        DIR* dir = opendir(inputDir);
        if(!dir)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot open /dev/input/");
        }

        int found_fd = -1;
        std::string found_path = "";

        struct dirent* entry;
        while((entry = readdir(dir)) != nullptr)
        {
            if(strncmp(entry->d_name, "event", 5) != 0){
                continue;
            }

            std::string currentPath = std::string(inputDir) + "/" + entry->d_name;
            int temp_fd = open(currentPath.c_str(), O_RDONLY | O_NONBLOCK);
            if (temp_fd < 0) {
                continue; // Negeer onleesbare/ontoegankelijke apparaten
            }

            // Check naam
            char name[256] = "Unknown";
            ioctl(temp_fd, EVIOCGNAME(sizeof(name)), name);
            if (strstr(name, targetDeviceName_.c_str()) == nullptr) {
                close(temp_fd);
                continue;
            }

            // Check basis capaciteiten
            unsigned char ev_bits[(EV_MAX + 7) / 8];
            if (ioctl(temp_fd, EVIOCGBIT(0, EV_MAX), ev_bits) < 0 ||
                !test_bit(EV_KEY, ev_bits) || !test_bit(EV_ABS, ev_bits)) {
                close(temp_fd);
                continue;
            }

            // Check specifieke gamepad capaciteiten
            unsigned char key_bits[(KEY_MAX + 7) / 8];
            unsigned char abs_bits[(ABS_MAX + 7) / 8];
            if (ioctl(temp_fd, EVIOCGBIT(EV_KEY, KEY_MAX), key_bits) < 0 ||
                ioctl(temp_fd, EVIOCGBIT(EV_ABS, ABS_MAX), abs_bits) < 0 ||
                !(test_bit(BTN_GAMEPAD, key_bits) || test_bit(BTN_SOUTH, key_bits)) || // Moet *een* gamepad knop hebben
                !test_bit(ABS_X, abs_bits) || !test_bit(ABS_Y, abs_bits)) { // Moet X/Y assen hebben
                close(temp_fd);
                continue;
            }

            // --- Apparaat Gevonden! ---
            // std::cout << "Debug: Found matching controller: " << name << " on " << currentPath << std::endl;
            found_fd = temp_fd;
            found_path = currentPath;
            break; // Stop met zoeken

        }

        closedir(dir);

        if (found_fd < 0) {
            throw std::runtime_error("Could not find a suitable evdev device matching '" + targetDeviceName_ + "' with gamepad capabilities in /dev/input. Check permissions and connection.");
        }

        // Wijs de gevonden waarden toe aan de member variabelen
        ev_fd_ = found_fd;
        devicePath_ = found_path;
        std::cout << "EvdevController: Using device '" << targetDeviceName_ << "' found at " << devicePath_ << std::endl;

    }

    // Update functie: lees nieuwe events en update de interne staat
    // Moet één keer per game loop frame worden aangeroepen
    void EvdevController::update() {
        // 1. Kopieer huidige staat naar vorige staat (voor isPressed/isReleased logica)
        previousKeyStates_ = currentKeyStates_;
        // Voor assen is een 'previous' staat meestal niet nodig, tenzij je veranderingen wilt detecteren

        // 2. Check of er events zijn zonder te blokkeren
        int poll_count = poll(&pollfd_, 1, 0); // Timeout 0 = non-blocking check

        if (poll_count < 0) {
            if (errno != EINTR) { // Negeer interrupties door signals
                throw std::system_error(errno, std::generic_category(), "poll() failed for " + devicePath_);
            }
            return; // Probeer volgende frame opnieuw
        }

        if (poll_count == 0 || !(pollfd_.revents & POLLIN)) {
            // Geen events beschikbaar
            return;
        }

        // 3. Lees en verwerk alle beschikbare events
        struct input_event ev;
        ssize_t bytesRead;
        while ((bytesRead = read(ev_fd_, &ev, sizeof(ev))) > 0) {
             if (bytesRead == sizeof(ev)) {
                processEvent(ev);
             } else {
                 // Gedeeltelijke read - kan gebeuren, maar is ongebruikelijk
                 std::cerr << "Warning: Partial read from " << devicePath_ << std::endl;
                 // Eventueel buffer legen of foutafhandeling
                 break; // Stop met lezen voor deze update cyclus
             }
        }

        // Handel read errors af (EAGAIN is normaal bij non-blocking als er geen data (meer) is)
        if (bytesRead < 0 && errno != EAGAIN) {
            throw std::system_error(errno, std::generic_category(), "Error reading from " + devicePath_);
        }
    }

    // --- Query Functies ---
    // Check of een knop/toets momenteel ingedrukt is
    // Gebruik codes zoals BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_TL, BTN_TR, KEY_SPACE etc.
    bool EvdevController::isKeyDown(int keyCode) const {
        if (keyCode < 0 || keyCode >= currentKeyStates_.size()) return false;
        return currentKeyStates_[keyCode];
    }

    // Check of een knop/toets precies deze frame ingedrukt werd
    bool EvdevController::isKeyPressed(int keyCode) const {
        if (keyCode < 0 || keyCode >= currentKeyStates_.size()) return false;
        return currentKeyStates_[keyCode] && !previousKeyStates_[keyCode];
    }

    // Check of een knop/toets precies deze frame losgelaten werd
    bool EvdevController::isKeyReleased(int keyCode) const {
         if (keyCode < 0 || keyCode >= currentKeyStates_.size()) return false;
        return !currentKeyStates_[keyCode] && previousKeyStates_[keyCode];
    }

    // Vraag de waarde van een absolute as op
    // Gebruik codes zoals ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y
    // Pas eventueel deadzone toe!
    int EvdevController::getAbsValue(int absCode, int deadzone) const {
        if (absCode < 0 || absCode >= currentAbsValues_.size()) return 0; // Of gooi exception

        int value = currentAbsValues_[absCode];

        // Eenvoudige symmetrische deadzone
        if (std::abs(value) < deadzone) {
            return 0;
        }
        return value;
    }

    // Hulpfunctie voor normalisatie van assen (typisch bereik -32768 tot 32767)
    float EvdevController::getAbsValueNormalized(int absCode, int deadzone) const {
        int value = getAbsValue(absCode, deadzone);
        if (value == 0) return 0.0f;

        // Normaliseer naar -1.0 tot 1.0 (benadering)
        // Let op: max waarde kan soms 32767 zijn.
        return static_cast<float>(value) / 32767.0f;
    }


    // Verwerkt een enkele input_event
    void EvdevController::processEvent(const struct input_event& ev) {
        switch (ev.type) {
            case EV_KEY: // Knoppen en toetsen
                if (ev.code >= 0 && ev.code < currentKeyStates_.size()) {
                    // ev.value: 0 = losgelaten, 1 = ingedrukt, 2 = auto-repeat
                    currentKeyStates_[ev.code] = (ev.value != 0); // Behandel 1 en 2 als ingedrukt
                    // Debug output (optioneel)
                    // if (ev.value == 1) std::cout << "Key/Button " << ev.code << " Pressed\n";
                    // if (ev.value == 0) std::cout << "Key/Button " << ev.code << " Released\n";
                    
                }
                break;

            case EV_ABS: // Analoge assen (sticks, triggers, D-pad soms)
                if (ev.code >= 0 && ev.code < currentAbsValues_.size()) {
                    currentAbsValues_[ev.code] = ev.value;
                     // Debug output (optioneel)
                     // std::cout << "Axis " << ev.code << " Value: " << ev.value << std::endl;
                }
                break;

            case EV_SYN: // Synchronisatie event
                // SYN_REPORT geeft aan dat een 'pakketje' van events compleet is.
                // Op dit punt is de staat consistent voor deze update.
                // We hoeven hier meestal niets speciaals te doen omdat we de staat
                // direct bijwerken bij EV_KEY/EV_ABS.
                break;

            // Negeer andere event types (EV_REL, EV_MSC, EV_FF etc.) voor nu
            default:
                break;
        }
    }