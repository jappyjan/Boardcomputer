#include "ota_manager.hpp"

OTAManager::OTAManager() : isStarted(false) {}

void OTAManager::begin()
{
    if (isStarted)
        return;

    LOG.info("OTAManager", "Starting OTA service");

    setupCallbacks();
    ArduinoOTA.begin();

    isStarted = true;
    LOG.info("OTAManager", "OTA service started");
}

void OTAManager::stop()
{
    if (!isStarted)
        return;

    ArduinoOTA.end();
    isStarted = false;
    LOG.info("OTAManager", "OTA service stopped");
}

void OTAManager::handle()
{
    if (isStarted)
    {
        ArduinoOTA.handle();
    }
}

void OTAManager::setupCallbacks()
{
    ArduinoOTA.onStart([]()
                       {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        delay(100);
        LOG.infof("OTAManager", "Start updating %s", type.c_str()); });

    ArduinoOTA.onEnd([]()
                     { 
                         LOG.info("OTAManager", "Update complete");
                         delay(1000);
                         LOG.info("OTAManager", "Rebooting...");
                         Serial.flush();
                         ESP.restart(); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { LOG.infof("OTAManager", "Progress: %u%%", (progress / (total / 100))); });

    ArduinoOTA.onError([](ota_error_t error)
                       {
        const char* errorMsg;
        switch (error) {
            case OTA_AUTH_ERROR: errorMsg = "Auth Failed"; break;
            case OTA_BEGIN_ERROR: errorMsg = "Begin Failed"; break;
            case OTA_CONNECT_ERROR: errorMsg = "Connect Failed"; break;
            case OTA_RECEIVE_ERROR: errorMsg = "Receive Failed"; break;
            case OTA_END_ERROR: errorMsg = "End Failed"; break;
            default: errorMsg = "Unknown Error"; break;
        }
        LOG.errorf("OTAManager", "Error[%u]: %s", error, errorMsg);
        delay(100);
        ESP.restart(); });
}