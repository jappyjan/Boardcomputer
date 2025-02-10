#include "wifi_manager.hpp"
#include "logger.hpp"

WifiManager::WifiManager(ConfigManager *configManager)
    : configManager(configManager), isRunning(false)
{
    localIP.fromString("4.3.2.1");
    gateway.fromString("4.3.2.1");
    subnet.fromString("255.255.255.0");
}

void WifiManager::setupIP()
{
    if (!WiFi.softAPConfig(localIP, gateway, subnet))
    {
        LOG.error("WifiManager", "AP Config failed");
        return;
    }
    LOG.debugf("WifiManager", "AP configured with IP: %s", localIP.toString().c_str());
}

bool WifiManager::startAP()
{
    if (isRunning)
        return true;

    WiFi.disconnect(true);
    delay(1000);
    LOG.info("WifiManager", "Previous WiFi connections disconnected");

    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_AP);
    delay(1000);
    LOG.info("WifiManager", "WiFi mode set to AP");

    setupIP();

    Config config = configManager->getConfig();
    bool apStarted = WiFi.softAP(config.apSsid, config.apPassword, 6, 0, 4);

    if (apStarted)
    {
        LOG.info("WifiManager", "Access Point Started Successfully");
        LOG.debugf("WifiManager", "AP Details: SSID=%s, IP=%s, MAC=%s, Channel=6, MaxConn=4",
                   config.apSsid, WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str());
        isRunning = true;
        return true;
    }
    else
    {
        LOG.error("WifiManager", "Failed to start Access Point");
        return false;
    }
}

void WifiManager::stop()
{
    if (!isRunning)
        return;

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    isRunning = false;
    LOG.info("WifiManager", "WiFi AP stopped");
}