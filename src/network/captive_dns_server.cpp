#include "captive_dns_server.hpp"
#include "logger.hpp"

CaptiveDnsServer::CaptiveDnsServer() : isRunning(false)
{
}

void CaptiveDnsServer::start(const IPAddress &localIP)
{
    if (isRunning)
    {
        stop();
    }

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.setTTL(300);

    if (dnsServer.start(DNS_PORT, "*", localIP))
    {
        isRunning = true;
        LOG.infof("CaptiveDnsServer", "DNS server started on port %d", DNS_PORT);
    }
    else
    {
        LOG.error("CaptiveDnsServer", "Failed to start DNS server");
    }
}

void CaptiveDnsServer::stop()
{
    if (!isRunning)
        return;

    dnsServer.stop();
    isRunning = false;
    LOG.info("CaptiveDnsServer", "DNS server stopped");
}

void CaptiveDnsServer::processRequests()
{
    if (isRunning)
    {
        dnsServer.processNextRequest();
    }
}