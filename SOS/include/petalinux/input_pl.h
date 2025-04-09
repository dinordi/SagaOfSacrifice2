#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/input.h>
#include <linux/input-event-codes.h> // Essentieel voor BTN_*, ABS_*, KEY_* codes
#include <system_error>
#include <cstring>
#include <cmath> // Voor std::abs
#include "playerInput.h"
#include <dirent.h>

// Klasse om een controller via evdev uit te lezen
class EvdevController 
{
public:
    EvdevController(const std::string& targetName = "DualSense");
    ~EvdevController();

    void update();
    bool isValid() const;
    std::string getDevicePath() const;


    bool isKeyDown(int keyCode) const;
    bool isKeyUp(int keyCode) const;
    bool isKeyReleased(int keyCode) const;
    bool isKeyPressed(int keyCode) const;
    int getAbsValue(int absCode, int deadzone = 4000) const;
    float getAbsValueNormalized(int absCode, int deadzone = 4000) const;

private:
    std::string devicePath_;
    int ev_fd_; // File descriptor voor het evdev apparaat
    struct pollfd pollfd_; // Voor non-blocking checks
    std::string targetDeviceName_;

    // Interne staat
    std::vector<bool> currentKeyStates_;
    std::vector<bool> previousKeyStates_;
    std::vector<int> currentAbsValues_;

    void findAndOpenDevice();

    // Verwerkt een enkele input_event
    void processEvent(const struct input_event& ev);
};
