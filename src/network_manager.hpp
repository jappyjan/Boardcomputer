#pragma once

#include <ESPAsyncWebServer.h>
#include "network/wifi_manager.hpp"
#include "network/captive_dns_server.hpp"
#include "network/captive_portal.hpp"
#include "network/api_server.hpp"
#include "network/event_stream.hpp"
#include "ota_manager.hpp"

class NetworkManager
{
public:
    NetworkManager(ConfigManager *configManager, BoardComputer *boardComputer);
    ~NetworkManager()
    {
        if (server)
            delete server;
    }
    void start();
    void update();
    bool shouldStart();

private:
    static NetworkManager *instance; // Add static instance pointer

    ConfigManager *configManager;
    BoardComputer *boardComputer;
    AsyncWebServer *server;
    bool networkStackStarted;
    unsigned long lastReceiverSignal;
    unsigned long lastErrorTime;

    WifiManager wifiManager;
    CaptiveDnsServer dnsServer;
    CaptivePortal captivePortal;
    ApiServer apiServer;
    EventStream eventStream;
    OTAManager otaManager;

    static const unsigned long TIMEOUT_MS = WIFI_ENABLE_TIMEOUT;

    void startNetworkStack();
    void stopNetworkStack();
};