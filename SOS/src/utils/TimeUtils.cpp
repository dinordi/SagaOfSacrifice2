#include "utils/TimeUtils.h"
#include <chrono>

uint32_t get_ticks() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now().time_since_epoch();
    return duration_cast<milliseconds>(now).count();
}