#pragma once
// =============================================================================
// StorageManager.h  –  NVS (Preferences) read/write wrappers
// =============================================================================

#include <Arduino.h>
#include <Preferences.h>
#include "ConfigManager.h"

class StorageManager {
public:
    // Singleton accessor.
    static StorageManager& instance() {
        static StorageManager inst;
        return inst;
    }

    // Persist the full DeviceConfig to NVS
    bool save(const DeviceConfig& cfg);

    // Load DeviceConfig from NVS; returns false if nothing was stored yet
    bool load(DeviceConfig& cfg);

    // Erase all stored keys (factory reset)
    bool erase();

private:
    StorageManager() {}
    Preferences _prefs;

    bool openRW();   // open the NVS namespace read-write
    bool openRO();   // open the NVS namespace read-only
    void close();    // close the NVS namespace
};
