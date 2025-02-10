#pragma once

#include <ESPAsyncWebServer.h>

class CaptivePortal
{
public:
    CaptivePortal(AsyncWebServer *server);
    void setupRoutes();
    void setServer(AsyncWebServer *newServer)
    {
        server = newServer;
        setupRoutes(); // Re-setup routes with new server
    }

private:
    AsyncWebServer *server;

    void handleCaptivePortal(AsyncWebServerRequest *request);
    void handleRoot(AsyncWebServerRequest *request);
    void handleFavicon(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
};