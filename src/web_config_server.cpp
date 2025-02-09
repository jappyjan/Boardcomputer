#include "web_config_server.hpp"
#include "config_manager.hpp"
#include <DNSServer.h>

const char *WebConfigServer::WIFI_SSID = "Boardcomputer";
const char *WebConfigServer::WIFI_PASSWORD = "boardcomputer";

WebConfigServer::WebConfigServer(ConfigManager *configManager, BoardComputer *boardComputer)
    : server(80), webServerStarted(false), lastReceiverSignal(0), lastErrorTime(0), dnsServer()
{
    this->configManager = configManager;
    this->boardComputer = boardComputer;
}

void WebConfigServer::start()
{
    lastReceiverSignal = millis();
    Serial.println("[WebServer] Starting");

    xTaskCreate(
        [](void *pvParameters)
        {
            Serial.println("[WebServer] Starting task");
            WebConfigServer *server = (WebConfigServer *)pvParameters;
            while (true)
            {
                server->update();
                vTaskDelay(100);
            }
        },
        "WebConfigServer", 4096, this, 5, NULL);
}

void WebConfigServer::update()
{
    unsigned long currentTime = millis();

    // Update lastReceiverSignal when receiving valid signals
    if (this->boardComputer->isReceiving())
    {
        lastReceiverSignal = currentTime;
    }

    // Update lastErrorTime when errors occur
    if (this->boardComputer->hasError())
    {
        if (lastErrorTime == 0)
        {
            lastErrorTime = currentTime;
        }
    }
    else
    {
        lastErrorTime = 0;
    }

    if (shouldStart())
    {
        startWebServer();
    }

    if (webServerStarted)
    {
        handleDNS(); // Handle DNS requests if server is running
    }
}

bool WebConfigServer::shouldStart()
{
    if (webServerStarted)
        return false;

    unsigned long currentTime = millis();

    // Check receiver timeout
    if (currentTime - lastReceiverSignal > TIMEOUT_MS)
    {
        Serial.printf("[WebServer] Starting due to receiver timeout (%lu ms since last signal)\n",
                      currentTime - lastReceiverSignal);
        return true;
    }

    // Check error state
    if (this->boardComputer->hasError() && currentTime - lastErrorTime > TIMEOUT_MS)
    {
        Serial.println("[WebServer] Starting due to error state timeout");
        return true;
    }

    return false;
}

void WebConfigServer::handleDNS()
{
    dnsServer.processNextRequest();
    // We can't easily log DNS queries directly since DNSServer doesn't expose this information
}

void WebConfigServer::setupRoutes()
{
    // Required for Windows 11 captive portal
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Windows 11 captive portal check from %s\n", request->client()->remoteIP().toString().c_str());
                request->redirect("http://logout.net"); });

    // Windows 10 captive portal
    server.on("/wpad.dat", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Windows 10 WPAD request from %s\n", request->client()->remoteIP().toString().c_str());
                request->send(404); });

    // Commonly used by modern systems
    server.on("/generate_204", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Android captive portal check from %s\n", request->client()->remoteIP().toString().c_str());
                request->redirect("/"); });

    server.on("/redirect", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Redirect request from %s\n", request->client()->remoteIP().toString().c_str());
                request->redirect("/"); });

    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] iOS/MacOS captive portal check from %s\n", request->client()->remoteIP().toString().c_str());
                request->redirect("/"); });

    server.on("/canonical.html", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Canonical page request from %s\n", request->client()->remoteIP().toString().c_str());
                request->redirect("/"); });

    server.on("/success.txt", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Success.txt check from %s\n", request->client()->remoteIP().toString().c_str());
                request->send(200); });

    server.on("/ncsi.txt", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Windows NCSI request from %s\n", request->client()->remoteIP().toString().c_str());
                request->redirect("/"); });

    // Return 404 for favicon
    server.on("/favicon.ico", [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Favicon request from %s\n", request->client()->remoteIP().toString().c_str());
                request->send(404); });

    // Original routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Main page request from %s\n", request->client()->remoteIP().toString().c_str());
                request->send(SPIFFS, "/index.html", "text/html"); });

    // API endpoints with logging
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Config GET request from %s\n", request->client()->remoteIP().toString().c_str());
                request->send(200, "application/json", this->configManager->getConfigAsJson()); });

    server.on("/api/pins", HTTP_GET, [this](AsyncWebServerRequest *request)
              { 
                Serial.printf("[WebServer] Pins GET request from %s\n", request->client()->remoteIP().toString().c_str());
                String pinMap = this->boardComputer->getPinMap();
                request->send(200, "application/json", pinMap); });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(200); }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
            Serial.printf("[WebServer] Config POST request from %s (len: %d, index: %d, total: %d)\n", 
                request->client()->remoteIP().toString().c_str(), len, index, total);
            
            String newConfig = String((char*)data);
            if (this->configManager->loadFromJson(newConfig.c_str())) {
                Serial.println("[WebServer] Configuration updated successfully");
                request->send(200, "text/plain", "Configuration updated successfully");
            } else {
                Serial.println("[WebServer] Invalid configuration received");
                request->send(400, "text/plain", "Invalid configuration");
            } });

    // Catch-all handler must be last
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        Serial.printf("[WebServer] Unknown request: http://%s%s from %s\n", 
            request->host().c_str(),
            request->url().c_str(),
            request->client()->remoteIP().toString().c_str());
        request->redirect("/"); });
}

void WebConfigServer::startWebServer()
{
    if (webServerStarted)
        return;

    Serial.println("[WebServer] Initializing web server...");

    if (!SPIFFS.begin(true))
    {
        Serial.println("[WebServer] ERROR: Failed to mount SPIFFS");
        return;
    }
    Serial.println("[WebServer] SPIFFS mounted successfully");

    WiFi.disconnect(true);
    delay(1000);
    Serial.println("[WebServer] Previous WiFi connections disconnected");

    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_AP);
    delay(1000);
    Serial.println("[WebServer] WiFi mode set to AP");

    IPAddress localIP(4, 3, 2, 1);
    IPAddress gateway(4, 3, 2, 1);
    IPAddress subnet(255, 255, 255, 0);

    if (!WiFi.softAPConfig(localIP, gateway, subnet))
    {
        Serial.println("[WebServer] ERROR: AP Config failed");
        return;
    }
    Serial.println("[WebServer] AP configured with IP: " + localIP.toString());

    Config config = this->configManager->getConfig();
    bool apStarted = WiFi.softAP(config.apSsid, config.apPassword, 6, 0, 4);

    if (apStarted)
    {
        Serial.println("[WebServer] Access Point Started Successfully");
        Serial.println("[WebServer] AP Details:");
        Serial.println("  SSID: " + String(config.apSsid));
        Serial.println("  Password: " + String(config.apPassword));
        Serial.println("  IP Address: " + WiFi.softAPIP().toString());
        Serial.println("  MAC Address: " + WiFi.softAPmacAddress());
        Serial.println("  Channel: 6");
        Serial.println("  Max Connections: 4");
    }
    else
    {
        Serial.println("[WebServer] ERROR: Failed to start Access Point");
        return;
    }

    dnsServer.start(DNS_PORT, "*", localIP);
    Serial.println("[WebServer] DNS server started on port " + String(DNS_PORT));

    setupRoutes();
    server.begin();

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.setTTL(300);
    dnsServer.start(DNS_PORT, "*", localIP);

    webServerStarted = true;
    Serial.println("[WebServer] Web server started and ready for connections");
}