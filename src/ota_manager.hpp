#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include "logger.hpp"

class OTAManager
{
public:
    OTAManager();

    void begin();
    void handle();
    void stop();

private:
    bool isStarted;

    void setupCallbacks();
};