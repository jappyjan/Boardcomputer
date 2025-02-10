#include "api_server.hpp"
#include "logger.hpp"

ApiServer::ApiServer(AsyncWebServer *server, ConfigManager *configManager, BoardComputer *boardComputer)
    : server(server), configManager(configManager), boardComputer(boardComputer)
{
}

void ApiServer::setupRoutes()
{
    server->on("/api/config", HTTP_GET, std::bind(&ApiServer::handleConfigGet, this, std::placeholders::_1));
    server->on("/api/pins", HTTP_GET, std::bind(&ApiServer::handlePinsGet, this, std::placeholders::_1));

    server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
               { request->send(200); }, NULL, std::bind(&ApiServer::handleConfigPost, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
}

void ApiServer::handleConfigGet(AsyncWebServerRequest *request)
{
    LOG.debugf("ApiServer", "Config GET request from %s", request->client()->remoteIP().toString().c_str());
    request->send(200, "application/json", this->configManager->getConfigAsJson());
}

void ApiServer::handlePinsGet(AsyncWebServerRequest *request)
{
    LOG.debugf("ApiServer", "Pins GET request from %s", request->client()->remoteIP().toString().c_str());
    String pinMap = this->boardComputer->getPinMap();
    request->send(200, "application/json", pinMap);
}

void ApiServer::handleConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    LOG.debugf("ApiServer", "Config POST request from %s (len: %d, index: %d, total: %d)",
               request->client()->remoteIP().toString().c_str(), len, index, total);

    String newConfig = String((char *)data);
    if (this->configManager->loadFromJson(newConfig.c_str()))
    {
        LOG.debug("ApiServer", "Configuration updated successfully");
        request->send(200, "text/plain", "Configuration updated successfully");
    }
    else
    {
        LOG.debug("ApiServer", "Invalid configuration received");
        request->send(400, "text/plain", "Invalid configuration");
    }
}