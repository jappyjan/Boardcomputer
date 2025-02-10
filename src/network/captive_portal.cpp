#include "captive_portal.hpp"
#include "logger.hpp"
#include <SPIFFS.h>

CaptivePortal::CaptivePortal(AsyncWebServer *server) : server(server)
{
}

void CaptivePortal::setupRoutes()
{
    server->on("/connecttest.txt", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/wpad.dat", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/generate_204", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/redirect", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/hotspot-detect.html", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/canonical.html", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/success.txt", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));
    server->on("/ncsi.txt", HTTP_GET, std::bind(&CaptivePortal::handleCaptivePortal, this, std::placeholders::_1));

    server->on("/favicon.ico", HTTP_GET, std::bind(&CaptivePortal::handleFavicon, this, std::placeholders::_1));
    server->on("/", HTTP_GET, std::bind(&CaptivePortal::handleRoot, this, std::placeholders::_1));

    server->onNotFound(std::bind(&CaptivePortal::handleNotFound, this, std::placeholders::_1));
}

void CaptivePortal::handleRoot(AsyncWebServerRequest *request)
{
    LOG.debugf("CaptivePortal", "Main page request from %s", request->client()->remoteIP().toString().c_str());
    request->send(SPIFFS, "/index.html", "text/html");
}

void CaptivePortal::handleCaptivePortal(AsyncWebServerRequest *request)
{
    String url = request->url();
    LOG.debugf("CaptivePortal", "Captive portal check from %s: %s",
               request->client()->remoteIP().toString().c_str(),
               url.c_str());

    if (url.equals("/connecttest.txt"))
    {
        request->redirect("http://logout.net"); // Special case for Windows 11
    }
    else if (url.equals("/wpad.dat"))
    {
        request->send(404); // Special case for Windows 10 WPAD
    }
    else if (url.equals("/success.txt"))
    {
        request->send(200); // Special case for success.txt
    }
    else
    {
        request->redirect("/"); // Default redirect to main page
    }
}

void CaptivePortal::handleFavicon(AsyncWebServerRequest *request)
{
    LOG.debugf("CaptivePortal", "Favicon request from %s", request->client()->remoteIP().toString().c_str());
    request->send(404);
}

void CaptivePortal::handleNotFound(AsyncWebServerRequest *request)
{
    LOG.debugf("CaptivePortal", "Unknown request: http://%s%s from %s",
               request->host().c_str(),
               request->url().c_str(),
               request->client()->remoteIP().toString().c_str());
    request->redirect("/");
}