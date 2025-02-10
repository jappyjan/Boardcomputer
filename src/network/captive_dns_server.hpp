#pragma once

#include <DNSServer.h>
#include <IPAddress.h>

class CaptiveDnsServer
{
public:
    CaptiveDnsServer();
    void start(const IPAddress &localIP);
    void stop();
    void processRequests();

private:
    DNSServer dnsServer;
    static const byte DNS_PORT = 53;
    bool isRunning;
};