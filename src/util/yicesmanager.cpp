#include "yicesmanager.hpp"

#include <yices.h>

std::atomic_uint YicesManager::running = 0;

void YicesManager::inc() {
    ++running;
}

void YicesManager::dec() {
    --running;
}

void YicesManager::init() {
    yices_init();
}

void YicesManager::exit() {
    if (running == 0) {
        yices_exit();
    }
}
