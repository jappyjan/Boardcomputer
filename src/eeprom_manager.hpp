#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <CRC32.h>
#include "config_versions.hpp"
#include "logger.hpp"

class EEPROMManager
{
public:
    // Header structure for stored data
    struct DataHeader
    {
        uint32_t magic;     // Magic number to identify valid data
        uint16_t version;   // Schema version for migrations
        uint16_t dataSize;  // Size of the data block
        uint32_t checksum;  // CRC32 checksum of the data
        uint32_t timestamp; // Last update timestamp
    };
    static const uint32_t MAGIC_NUMBER = 0xB0C0FFEE; // Magic number to identify our data
    static const uint16_t CURRENT_VERSION = 1;       // Current schema version
    static const size_t HEADER_SIZE = sizeof(DataHeader);

    bool begin(size_t dataSize)
    {
        size_t totalSize = HEADER_SIZE + dataSize;
        LOG.debugf("EEPROMManager", "Initializing EEPROM with header size %d + data size %d = %d total bytes",
                   HEADER_SIZE, dataSize, totalSize);

        // ESP32/8266 EEPROM size must be multiple of 4
        if (totalSize % 4 != 0)
        {
            totalSize += 4 - (totalSize % 4);
            LOG.debugf("EEPROMManager", "Adjusted EEPROM size to %d bytes to match 4-byte alignment", totalSize);
        }

        bool success = EEPROM.begin(totalSize);
        if (!success)
        {
            LOG.error("EEPROMManager", "EEPROM.begin() failed!");
#ifdef ESP32
            LOG.error("EEPROMManager", "On ESP32, check if partition table includes EEPROM partition");
#endif
#ifdef ESP8266
            LOG.error("EEPROMManager", "On ESP8266, check if flash size is configured correctly");
#endif
        }
        else
        {
            LOG.infof("EEPROMManager", "EEPROM initialized with size %d bytes", EEPROM.length());
        }
        return success;
    }

    template <typename T>
    uint32_t calculateChecksum(const T &data)
    {
        // Create clean buffer
        uint8_t buffer[sizeof(T)];
        memset(buffer, 0, sizeof(T));
        memcpy(buffer, &data, sizeof(T));

        CRC32 crc;
        crc.update(buffer, sizeof(T));
        return crc.finalize();
    }

    template <typename T>
    bool write(const T &data)
    {
        // Add size verification
        if (sizeof(T) + HEADER_SIZE > EEPROM.length())
        {
            LOG.errorf("EEPROMManager", "Data too large. Total size needed: %d, EEPROM size: %d",
                       sizeof(T) + HEADER_SIZE, EEPROM.length());
            return false;
        }

        DataHeader header;
        header.magic = MAGIC_NUMBER;
        header.version = CURRENT_VERSION;
        header.dataSize = sizeof(T);
        header.timestamp = millis();
        header.checksum = calculateChecksum(data);

        LOG.debugf("EEPROMManager", "Writing to EEPROM: Magic=0x%08X, Ver=%d, Size=%d, CRC=0x%08X",
                   header.magic, header.version, header.dataSize, header.checksum);

        // Write header
        EEPROM.put(0, header);

        // Write data directly
        EEPROM.put(HEADER_SIZE, data);

        // Verify the write by reading back
        T readback;
        EEPROM.get(HEADER_SIZE, readback);
        uint32_t readbackChecksum = calculateChecksum(readback);
        LOG.debugf("EEPROMManager", "  Write verification checksum: 0x%08X", readbackChecksum);

        if (readbackChecksum != header.checksum)
        {
            LOG.error("EEPROMManager", "Write verification failed - checksums don't match");
            return false;
        }

        bool success = EEPROM.commit();
        LOG.debugf("EEPROMManager", "EEPROM write successful");
        return success;
    }

    template <typename T>
    bool read(T &data)
    {
        DataHeader header;
        EEPROM.get(0, header);

        LOG.debug("EEPROMManager", "Reading from EEPROM:");
        LOG.debugf("EEPROMManager", "  Magic: 0x%08X", header.magic);
        LOG.debugf("EEPROMManager", "  Version: %d", header.version);
        LOG.debugf("EEPROMManager", "  Data Size: %d bytes", header.dataSize);
        LOG.debugf("EEPROMManager", "  Checksum: 0x%08X", header.checksum);
        LOG.debugf("EEPROMManager", "  Timestamp: %lu", header.timestamp);
        LOG.debugf("EEPROMManager", "  Expected data size: %d bytes", sizeof(T));

        // Validate magic number
        if (header.magic != MAGIC_NUMBER)
        {
            LOG.errorf("EEPROMManager", "Invalid magic number in EEPROM. Expected: 0x%08X, Got: 0x%08X",
                       MAGIC_NUMBER, header.magic);
            return false;
        }

        // Read data
        T readData;
        EEPROM.get(HEADER_SIZE, readData);

        // Handle version migration if needed
        if (header.version != CURRENT_VERSION)
        {
            if (!migrateData(readData, header.version))
            {
                LOG.errorf("EEPROMManager", "Migration failed");
                return false;
            }
        }

        // Calculate checksum
        uint32_t calculatedChecksum = calculateChecksum(readData);

        if (calculatedChecksum == header.checksum)
        {
            memcpy(&data, &readData, sizeof(T));
            LOG.infof("EEPROMManager", "EEPROM read successful");
            return true;
        }

        LOG.errorf("EEPROMManager", "Checksum verification failed");
        return false;
    }

    bool clear()
    {
        LOG.infof("EEPROMManager", "Clearing EEPROM...");
        for (size_t i = 0; i < EEPROM.length(); i++)
        {
            EEPROM.write(i, 0);
        }
        return EEPROM.commit();
    }

private:
    /**
     * Migrate data from older versions
     * Returns true if migration was successful
     */
    template <typename T>
    bool migrateData(T &data, uint16_t fromVersion)
    {
        if (fromVersion == CURRENT_VERSION)
        {
            return true; // No migration needed
        }

        LOG.infof("EEPROMManager", "Migrating data from version %d to %d", fromVersion, CURRENT_VERSION);

        while (fromVersion < CURRENT_VERSION)
        {
            LOG.errorf("EEPROMManager", "No migration path from version %d to %d", fromVersion, CURRENT_VERSION);
            while (true)
            {
                delay(1000);
            }
        }

        LOG.infof("EEPROMManager", "No migration path from version %d to %d", fromVersion, CURRENT_VERSION);
        return false;
    }
};
