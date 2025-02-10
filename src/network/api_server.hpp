#pragma once

#include <ESPAsyncWebServer.h>
#include "config_manager.hpp"
#include "boardcomputer.hpp"

class ApiServer
{
public:
    ApiServer(AsyncWebServer *server, ConfigManager *configManager, BoardComputer *boardComputer);
    void setupRoutes();
    void setServer(AsyncWebServer *newServer)
    {
        server = newServer;
        setupRoutes(); // Re-setup routes with new server
    }

private:
    AsyncWebServer *server;
    ConfigManager *configManager;
    BoardComputer *boardComputer;

    void handleConfigGet(AsyncWebServerRequest *request);
    void handlePinsGet(AsyncWebServerRequest *request);
    void handleConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
};