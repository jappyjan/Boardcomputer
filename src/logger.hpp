#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>

enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger
{
public:
    static Logger &getInstance()
    {
        static Logger instance;
        return instance;
    }

    // Initialize logger with Serial and optional baud rate
    void begin(unsigned long baudRate = 115200)
    {
        Serial.begin(baudRate);
        while (!Serial)
            delay(10); // Wait for Serial to be ready
        info("Logger", "Logging system initialized");
    }

    // Main logging methods
    void debug(const char *tag, const char *message)
    {
        log(LogLevel::DEBUG, tag, message);
    }

    void info(const char *tag, const char *message)
    {
        log(LogLevel::INFO, tag, message);
    }

    void warning(const char *tag, const char *message)
    {
        log(LogLevel::WARNING, tag, message);
    }

    void error(const char *tag, const char *message)
    {
        log(LogLevel::ERROR, tag, message);
    }

    // Printf style logging methods
    template <typename... Args>
    void debugf(const char *tag, const char *format, Args... args)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, args...);
        debug(tag, buffer);
    }

    template <typename... Args>
    void infof(const char *tag, const char *format, Args... args)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, args...);
        info(tag, buffer);
    }

    template <typename... Args>
    void warningf(const char *tag, const char *format, Args... args)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, args...);
        warning(tag, buffer);
    }

    template <typename... Args>
    void errorf(const char *tag, const char *format, Args... args)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, args...);
        error(tag, buffer);
    }

    // Set minimum log level
    void setLogLevel(LogLevel level)
    {
        minimumLogLevel = level;
    }

    // Add custom log handler
    void addLogHandler(std::function<void(LogLevel, const char *, const char *)> handler)
    {
        logHandlers.push_back(handler);
    }

    // Add this method to the Logger class
    void removeLogHandlers()
    {
        logHandlers.clear();
    }

private:
    Logger() : minimumLogLevel(LogLevel::DEBUG) {}
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    LogLevel minimumLogLevel;
    std::vector<std::function<void(LogLevel, const char *, const char *)>> logHandlers;

    void log(LogLevel level, const char *tag, const char *message)
    {
        if (level < minimumLogLevel)
            return;

        // Format timestamp
        unsigned long now = millis();
        char timestamp[16];
        snprintf(timestamp, sizeof(timestamp), "[%lu]", now);

        // Get level string
        const char *levelStr;
        switch (level)
        {
        case LogLevel::DEBUG:
            levelStr = "DEBUG";
            break;
        case LogLevel::INFO:
            levelStr = "INFO";
            break;
        case LogLevel::WARNING:
            levelStr = "WARN";
            break;
        case LogLevel::ERROR:
            levelStr = "ERROR";
            break;
        default:
            levelStr = "UNKNOWN";
        }

        // Default Serial output
        Serial.printf("%s [%s] %s: %s\n", timestamp, levelStr, tag, message);

        // Call custom handlers
        for (const auto &handler : logHandlers)
        {
            handler(level, tag, message);
        }
    }
};

// Global logger instance accessor
#define LOG Logger::getInstance()