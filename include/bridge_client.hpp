/*
 * bridge_client.hpp -- C++ RAII wrapper for Bridge client requests.
 *
 * Usage:
 *   {
 *       BridgeMonitor mon("my_section");
 *       // ... code to monitor ...
 *   }  // monitoring stops when `mon` goes out of scope
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef BRIDGE_CLIENT_HPP
#define BRIDGE_CLIENT_HPP

#include "bridge_client.h"

class BridgeMonitor {
public:
    explicit BridgeMonitor(const char* label) {
        BRIDGE_MONITOR_START(label);
    }
    ~BridgeMonitor() {
        BRIDGE_MONITOR_STOP();
    }

    BridgeMonitor(const BridgeMonitor&) = delete;
    BridgeMonitor& operator=(const BridgeMonitor&) = delete;
};

#endif /* BRIDGE_CLIENT_HPP */
