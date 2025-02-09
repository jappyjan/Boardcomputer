#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <CRC32.h>
#include "config_versions.hpp"

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
        Serial.printf("Initializing EEPROM with header size %d + data size %d = %d total bytes\n",
                      HEADER_SIZE, dataSize, totalSize);

        // ESP32/8266 EEPROM size must be multiple of 4
        if (totalSize % 4 != 0)
        {
            totalSize += 4 - (totalSize % 4);
            Serial.printf("Adjusted EEPROM size to %d bytes to match 4-byte alignment\n", totalSize);
        }

        bool success = EEPROM.begin(totalSize);
        if (!success)
        {
            Serial.println("EEPROM.begin() failed!");
#ifdef ESP32
            Serial.println("On ESP32, check if partition table includes EEPROM partition");
#endif
#ifdef ESP8266
            Serial.println("On ESP8266, check if flash size is configured correctly");
#endif
        }
        else
        {
            Serial.printf("EEPROM initialized with size %d bytes\n", EEPROM.length());
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

        // Debug print
        Serial.print("Checksum calculation - First 32 bytes: ");
        for (int i = 0; i < min(32, (int)sizeof(T)); i++)
        {
            Serial.printf("%02X ", buffer[i]);
        }
        Serial.println();

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
            Serial.printf("ERROR: Data too large. Total size needed: %d, EEPROM size: %d\n",
                          sizeof(T) + HEADER_SIZE, EEPROM.length());
            return false;
        }

        DataHeader header;
        header.magic = MAGIC_NUMBER;
        header.version = CURRENT_VERSION;
        header.dataSize = sizeof(T);
        header.timestamp = millis();
        header.checksum = calculateChecksum(data);

        Serial.println("Writing to EEPROM:");
        Serial.printf("  Magic: 0x%08X\n", header.magic);
        Serial.printf("  Version: %d\n", header.version);
        Serial.printf("  Data Size: %d bytes\n", header.dataSize);
        Serial.printf("  Checksum: 0x%08X\n", header.checksum);
        Serial.printf("  Timestamp: %lu\n", header.timestamp);
        Serial.printf("  EEPROM total size: %d bytes\n", EEPROM.length());

        // Write header
        EEPROM.put(0, header);

        // Write data directly
        EEPROM.put(HEADER_SIZE, data);

        // Verify the write by reading back
        T readback;
        EEPROM.get(HEADER_SIZE, readback);
        uint32_t readbackChecksum = calculateChecksum(readback);
        Serial.printf("  Write verification checksum: 0x%08X\n", readbackChecksum);

        if (readbackChecksum != header.checksum)
        {
            Serial.println("ERROR: Write verification failed - checksums don't match");
            return false;
        }

        bool success = EEPROM.commit();
        Serial.printf("EEPROM commit %s\n", success ? "successful" : "failed");
        return success;
    }

    template <typename T>
    bool read(T &data)
    {
        DataHeader header;
        EEPROM.get(0, header);

        Serial.println("Reading from EEPROM:");
        Serial.printf("  Magic: 0x%08X\n", header.magic);
        Serial.printf("  Version: %d\n", header.version);
        Serial.printf("  Data Size: %d bytes\n", header.dataSize);
        Serial.printf("  Checksum: 0x%08X\n", header.checksum);
        Serial.printf("  Timestamp: %lu\n", header.timestamp);
        Serial.printf("  Expected data size: %d bytes\n", sizeof(T));

        // Validate magic number
        if (header.magic != MAGIC_NUMBER)
        {
            Serial.printf("Invalid magic number in EEPROM. Expected: 0x%08X, Got: 0x%08X\n",
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
                Serial.println("Migration failed");
                return false;
            }
        }

        // Calculate checksum
        uint32_t calculatedChecksum = calculateChecksum(readData);
        Serial.printf("Checksum comparison:\n");
        Serial.printf("  Stored:     0x%08X\n", header.checksum);
        Serial.printf("  Calculated: 0x%08X\n", calculatedChecksum);

        if (calculatedChecksum == header.checksum)
        {
            memcpy(&data, &readData, sizeof(T));
            Serial.println("EEPROM read successful");
            return true;
        }

        Serial.println("Checksum verification failed");
        return false;
    }

    bool clear()
    {
        Serial.println("Clearing EEPROM...");
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

        Serial.printf("Migrating data from version %d to %d\n", fromVersion, CURRENT_VERSION);

        while (fromVersion < CURRENT_VERSION)
        {
            // Example migration from V1 to V2
            if (fromVersion == 1)
            {
                // Read old format
                ConfigVersions::ConfigV1 oldConfig;
                EEPROM.get(HEADER_SIZE, oldConfig);

                // Convert to new format
                ConfigVersions::ConfigV2 newConfig;

                // Copy common fields
                newConfig.numHandlers = oldConfig.numHandlers;
                memcpy(newConfig.handlers, oldConfig.handlers, sizeof(oldConfig.handlers));

                // Set new fields to defaults
                strncpy(newConfig.apSsid, "Boardcomputer", sizeof(newConfig.apSsid) - 1);
                strncpy(newConfig.apPassword, "Boardcomputer", sizeof(newConfig.apPassword) - 1);

                // Copy back to output
                memcpy(&data, &newConfig, sizeof(newConfig));

                fromVersion = 2;
            }
        }

        Serial.printf("No migration path from version %d to %d\n", fromVersion, CURRENT_VERSION);
        return false;
    }
};
