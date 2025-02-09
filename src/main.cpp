#include <Arduino.h>
#include "boardcomputer.hpp"
#include "config_manager.hpp"
#include "const.hpp"
#include "web_config_server.hpp"
#include "eeprom_manager.hpp"

BoardComputer boardComputer(&Serial0);
EEPROMManager eeprom;
ConfigManager configManager(&boardComputer, &eeprom);

WebConfigServer webServer(&configManager, &boardComputer);

void setup()
{
  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting board computer");

  if (!configManager.begin())
  {
    Serial.println("Failed to initialize config manager");
    while (true)
    {
      delay(1000);
    }
  }

  // Print size information
  Serial.printf("Config size: %d bytes\n", sizeof(Config));
  Serial.printf("DataHeader size: %d bytes\n", sizeof(EEPROMManager::DataHeader));

  // Load and apply configuration
  if (!configManager.loadFromEEPROM())
  {
    Serial.println("Failed to load config, continuing with defaults...");
    // Don't return, just continue with defaults
  }

  webServer.start();
  boardComputer.start();

  Serial.println("Setup complete");
}

void loop()
{
  // Add a small delay to prevent watchdog issues
  delay(10);
}