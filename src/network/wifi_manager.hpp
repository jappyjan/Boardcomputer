#pragma once

#include <WiFi.h>
#include "config_manager.hpp"

class WifiManager
{
public:
    WifiManager(ConfigManager *configManager);
    bool startAP();
    void stop();
    IPAddress getLocalIP() const { return localIP; }

private:
    ConfigManager *configManager;
    IPAddress localIP;
    IPAddress gateway;
    IPAddress subnet;
    bool isRunning;

    void setupIP();
};