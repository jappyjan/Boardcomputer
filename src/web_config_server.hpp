#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <esp_wifi.h>

#include "config_manager.hpp"

class WebConfigServer
{
public:
    WebConfigServer(ConfigManager *configManager, BoardComputer *boardComputer);
    void start();
    void update();
    bool shouldStart();
    void handleDNS();

private:
    static const char *WIFI_SSID;
    static const char *WIFI_PASSWORD;
    static const unsigned long TIMEOUT_MS = WIFI_ENABLE_TIMEOUT;

    ConfigManager *configManager;
    BoardComputer *boardComputer;
    AsyncWebServer server;
    bool webServerStarted;
    unsigned long lastReceiverSignal;
    unsigned long lastErrorTime;
    DNSServer dnsServer;
    const byte DNS_PORT = 53;

    void startWebServer();
    void setupRoutes();
};