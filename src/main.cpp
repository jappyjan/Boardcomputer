#include <Arduino.h>
#include "bordcomputer.hpp"
#include "config_manager.hpp"
#include "const.hpp"
#include "network_manager.hpp"
#include "eeprom_manager.hpp"
#include "logger.hpp"

BoardComputer boardComputer(&Serial0);
EEPROMManager eeprom;
ConfigManager configManager(&boardComputer, &eeprom);

NetworkManager network(&configManager, &boardComputer);

void setup()
{
  // Initialize logger first
  LOG.begin(115200);

  delay(3000);

  LOG.info("Main", "Starting board computer");

  if (!configManager.begin())
  {
    LOG.error("Main", "Failed to initialize config manager");
    while (true)
    {
      delay(1000);
    }
  }

  // Print size information
  LOG.infof("Main", "Config size: %d bytes", sizeof(Config));
  LOG.infof("Main", "DataHeader size: %d bytes", sizeof(EEPROMManager::DataHeader));

  // Load and apply configuration
  if (!configManager.loadFromEEPROM())
  {
    LOG.warning("Main", "Failed to load config, continuing with defaults...");
    // Don't return, just continue with defaults
  }

  network.start();
  boardComputer.start();

  LOG.info("Main", "Setup complete");
}

void loop()
{
  // Add a small delay to prevent watchdog issues
  delay(10);
}