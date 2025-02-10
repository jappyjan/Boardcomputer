#pragma once
#include <ESPAsyncWebServer.h>
#include "logger.hpp"
#include <ArduinoJson.h>

enum class EventType
{
    LOGGING,
    TELEMETRY
};

class EventStream
{
public:
    EventStream(AsyncWebServer *server) : events("/events")
    {
        setServer(server);
    }

    void setServer(AsyncWebServer *server)
    {
        server->addHandler(&events);

        events.onConnect([this](AsyncEventSourceClient *client)
                         {
            LOG.infof("EventStream", "Client connected from %s", client->client()->remoteIP().toString().c_str());
            
            // Send welcome message
            DynamicJsonDocument doc(256);
            doc["type"] = "connected";
            doc["clientIp"] = client->client()->remoteIP().toString();
            
            String output;
            serializeJson(doc, output);
            client->send(output.c_str()); });
    }

    void stop()
    {
        events.close();
    }

    void update()
    {
        // This method intentionally left empty as SSE doesn't need periodic updates
        // The events are sent when sendEvent() or sendJson() is called
    }

    void sendEvent(EventType type, const char *message)
    {
        DynamicJsonDocument doc(512);
        doc["type"] = static_cast<int>(type);
        doc["data"] = message;

        String output;
        serializeJson(doc, output);
        events.send(output.c_str(), "message", millis());
    }

    void sendJson(EventType type, const DynamicJsonDocument &data)
    {
        DynamicJsonDocument doc(1024);
        doc["type"] = static_cast<int>(type);
        doc["data"] = data;

        String output;
        serializeJson(doc, output);
        events.send(output.c_str(), "message", millis());
    }

private:
    AsyncEventSource events;
};