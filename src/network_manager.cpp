#include "network_manager.hpp"
#include "config_manager.hpp"
#include "logger.hpp"
#include <SPIFFS.h>

NetworkManager *NetworkManager::instance = nullptr;

NetworkManager::NetworkManager(ConfigManager *configManager, BoardComputer *boardComputer)
    : configManager(configManager),
      boardComputer(boardComputer),
      server(new AsyncWebServer(80)),
      networkStackStarted(false),
      lastReceiverSignal(0),
      lastErrorTime(0),
      wifiManager(configManager),
      captivePortal(server),
      apiServer(server, configManager, boardComputer),
      eventStream(server)
{
    instance = this;
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
}

void NetworkManager::start()
{
    lastReceiverSignal = millis();
    LOG.infof("NetworkManager", "Starting");

    xTaskCreate(
        [](void *pvParameters)
        {
            LOG.debug("NetworkManager", "Task started");
            NetworkManager *server = (NetworkManager *)pvParameters;
            while (true)
            {
                server->update();
                vTaskDelay(100);
            }
        },
        "NetworkManager", 8192, this, 5, NULL);
}

void NetworkManager::update()
{
    static unsigned long lastUpdateLog = 0;
    unsigned long currentTime = millis();

    // Log state every second
    if (currentTime - lastUpdateLog >= 1000)
    {
        lastUpdateLog = currentTime;
    }

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

    bool shouldBeRunning = shouldStart();

    if (shouldBeRunning && !networkStackStarted)
    {
        LOG.info("NetworkManager", "Starting network stack - condition triggered");
        startNetworkStack();
    }
    else if (!shouldBeRunning && networkStackStarted)
    {
        LOG.info("NetworkManager", "Stopping network stack - stable and healthy");
        stopNetworkStack();
    }

    if (networkStackStarted)
    {
        dnsServer.processRequests();
        otaManager.handle();
        eventStream.update();
    }
}

bool NetworkManager::shouldStart()
{
    // First check if we NEED to be running, regardless of current state
    bool isReceiving = this->boardComputer->isReceiving();
    bool hasError = this->boardComputer->hasError();

    // If we have errors or no signal, we need to run
    if (!isReceiving || hasError)
    {
        return true;
    }

    // If we get here, we're not running and don't need to run
    if (this->configManager->getConfig().keepWebServerRunning)
    {
        return true;
    }

    // If we're already running, check if we can stop
    if (networkStackStarted)
    {
        static const unsigned long HEALTHY_DURATION_REQUIRED = 5000;
        static unsigned long healthyStateStartTime = 0;
        unsigned long currentTime = millis();

        if (healthyStateStartTime == 0)
        {
            healthyStateStartTime = currentTime;
            LOG.debug("NetworkManager", "Starting healthy state timer - first healthy state detected");
            return true;
        }

        unsigned long timeInHealthyState = currentTime - healthyStateStartTime;
        LOG.debugf("NetworkManager", "Healthy state progress - Time: %lu/%lu ms",
                   timeInHealthyState, HEALTHY_DURATION_REQUIRED);

        if (timeInHealthyState >= HEALTHY_DURATION_REQUIRED)
        {
            LOG.debug("NetworkManager", "System has been stable and healthy for required duration");
            healthyStateStartTime = 0; // Reset for next time
            return false;
        }
        return true;
    }

    LOG.debug("NetworkManager", "No need to start network stack");
    return false;
}

void NetworkManager::startNetworkStack()
{
    if (networkStackStarted)
        return;

    // Reset network stack state
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    LOG.info("NetworkManager", "Initializing web server...");

    // Try to mount SPIFFS if not already mounted
    if (!SPIFFS.begin(true))
    {
        LOG.error("NetworkManager", "Failed to mount SPIFFS");
        return;
    }

    // Verify SPIFFS has the required files
    if (!SPIFFS.exists("/index.html"))
    {
        LOG.error("NetworkManager", "Critical file /index.html not found in SPIFFS");
        SPIFFS.end();
        return;
    }

    // Start WiFi Access Point
    if (!wifiManager.startAP())
    {
        LOG.error("NetworkManager", "Failed to start WiFi AP");
        SPIFFS.end();
        return;
    }
    LOG.info("NetworkManager", "WiFi AP started successfully");

    // Add delay to ensure WiFi is fully initialized
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Start DNS server
    dnsServer.start(wifiManager.getLocalIP());
    LOG.info("NetworkManager", "DNS server started");

    // Start OTA service after WiFi is set up
    otaManager.begin();
    LOG.info("NetworkManager", "OTA service started");

    // Setup web routes before starting server
    captivePortal.setupRoutes();
    apiServer.setupRoutes();
    LOG.info("NetworkManager", "Web routes configured");

    // Start EventStream
    LOG.info("NetworkManager", "Initializing event stream...");

    // Add logging handler
    LOG.addLogHandler([this](LogLevel level, const char *tag, const char *message)
                      {
        char buffer[512];
        const char *levelStr = "INFO";
        switch (level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::WARNING: levelStr = "WARN"; break;
            case LogLevel::ERROR: levelStr = "ERROR"; break;
            default: break;
        }
        snprintf(buffer, sizeof(buffer), "[%s] %s: %s", levelStr, tag, message);
        eventStream.sendEvent(EventType::LOGGING, buffer); });

    // Start telemetry updates
    xTaskCreate(
        [](void *param)
        {
            NetworkManager *nm = (NetworkManager *)param;
            while (true)
            {
                if (nm->boardComputer)
                {
                    DynamicJsonDocument doc(1024);
                    doc["isReceiving"] = nm->boardComputer->isReceiving();
                    doc["hasError"] = nm->boardComputer->hasError();

                    JsonArray channels = doc.createNestedArray("channels");
                    for (int i = 0; i < 16; i++)
                    {
                        channels.add(nm->boardComputer->getChannelValue(i));
                    }

                    nm->eventStream.sendJson(EventType::TELEMETRY, doc);
                }
                vTaskDelay(pdMS_TO_TICKS(100)); // Update every 100ms
            }
        },
        "TelemetryTask", 4096, this, 1, NULL);

    // Add delay before starting web server
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Start web server
    server->begin();
    LOG.info("NetworkManager", "Web server started");

    networkStackStarted = true;
    LOG.info("NetworkManager", "Network stack initialization complete");
}

void NetworkManager::stopNetworkStack()
{
    if (!networkStackStarted)
        return;

    // Stop all services in reverse order
    server->end();
    LOG.info("NetworkManager", "Web server stopped");

    eventStream.stop();
    otaManager.stop();
    dnsServer.stop();
    wifiManager.stop();

    // End SPIFFS to ensure clean unmount
    SPIFFS.end();
    LOG.info("NetworkManager", "SPIFFS unmounted");

    networkStackStarted = false;

    // Add delay to allow sockets to properly close
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Create a new server instance to ensure clean state
    delete server;
    server = new AsyncWebServer(80);

    // Re-initialize handlers with new server instance
    eventStream.setServer(server);
    captivePortal.setServer(server);
    apiServer.setServer(server);
}
