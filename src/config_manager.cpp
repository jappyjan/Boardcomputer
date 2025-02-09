#include "config_manager.hpp"
#include "eeprom_manager.hpp"

ConfigManager::ConfigManager(BoardComputer *computer, EEPROMManager *eeprom)
    : computer(computer), eeprom(eeprom), config(), eepromInitialized(false)
{
    eepromInitialized = false;
}

bool ConfigManager::begin()
{
    Serial.printf("Initializing EEPROM for Config size: %d bytes\n", sizeof(Config));
    eepromInitialized = eeprom->begin(sizeof(Config));
    if (!eepromInitialized)
    {
        Serial.println("ERROR: Failed to initialize EEPROM");
        return false;
    }

    // Try to read config, if it fails (checksum error), clear EEPROM
    Config testConfig;
    if (!eeprom->read(testConfig))
    {
        Serial.println("Invalid data in EEPROM, clearing...");
        eeprom->clear();
    }

    return eepromInitialized;
}

bool ConfigManager::load(const Config &config)
{
    this->configure(config);

    if (!eepromInitialized)
    {
        Serial.println("ERROR: Cannot write to EEPROM - not initialized");
        return false;
    }

    return eeprom->write(config);
}

bool ConfigManager::loadFromEEPROM()
{
    if (!eepromInitialized)
    {
        Serial.println("ERROR: Cannot read from EEPROM - not initialized");
        return false;
    }

    Config config;
    if (!eeprom->read(config))
    {
        Serial.println("Failed to load config, using defaults");
        return false;
    }
    return configure(config);
}

bool ConfigManager::loadFromJson(const char *jsonConfig)
{
    Config config = parseJson(jsonConfig);
    return load(config);
}

Config ConfigManager::getConfig()
{
    return config;
}

String ConfigManager::getConfigAsJson()
{
    StaticJsonDocument<2048> doc;
    JsonArray handlers = doc.createNestedArray("handlers");

    for (size_t i = 0; i < config.numHandlers; i++)
    {
        const auto &handler = config.handlers[i];
        JsonObject handlerObj = handlers.createNestedObject();
        handlerObj["type"] = handler.type;
        handlerObj["pin"] = handler.pin;
        handlerObj["channel"] = handler.channel;
        handlerObj["failsafe"] = handler.failsafe;
        handlerObj["threshold"] = handler.threshold;
        handlerObj["operator"] = handler.op;
        handlerObj["inverted"] = handler.inverted;
        handlerObj["min"] = handler.min;
        handlerObj["max"] = handler.max;
        handlerObj["onTime"] = handler.onTime;
        handlerObj["offTime"] = handler.offTime;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

bool ConfigManager::configure(const Config &config)
{
    Serial.println("Starting configuration...");

    // Basic validation
    if (config.numHandlers == 0)
    {
        Serial.println("Warning: No handlers configured");
    }

    // Store the new configuration
    this->config = config;

    for (size_t i = 0; i < config.numHandlers; i++)
    {
        const auto &handler = config.handlers[i];
        Serial.printf("\nConfiguring %s handler for pin '%s' on channel %d\n",
                      handler.type, handler.pin, handler.channel);

        if (strcmp(handler.type, "pwm") == 0)
        {
            configurePWMHandler(handler);
        }
        else if (strcmp(handler.type, "onoff") == 0)
        {
            configureOnOffHandler(handler);
        }
        else if (strcmp(handler.type, "blink") == 0)
        {
            configureBlinkHandler(handler);
        }
        else
        {
            Serial.printf("WARNING: Unknown handler type '%s'\n", handler.type);
        }
    }

    Serial.println("\nConfiguration complete!");
    return true;
}

void ConfigManager::configurePWMHandler(const HandlerConfig &handlerConfig)
{
    if (!handlerConfig.failsafe)
    {
        Serial.println("ERROR: PWM handler requires 'failsafe' value");
        return;
    }

    auto pinInfo = PIN_MAP.find(handlerConfig.pin);
    if (pinInfo == PIN_MAP.end() || !pinInfo->second.isPWM)
    {
        Serial.printf("ERROR: Invalid PWM pin: %s\n", handlerConfig.pin);
        return;
    }

    uint8_t channel = handlerConfig.channel;
    int failsafeValue = handlerConfig.failsafe;
    int min = handlerConfig.min;
    int max = handlerConfig.max;
    bool inverted = handlerConfig.inverted;

    // Validate failsafe value is within range
    if (failsafeValue < CHANNEL_MIN || failsafeValue > CHANNEL_MAX)
    {
        Serial.printf("ERROR: Failsafe value %d is out of range (%d-%d)\n",
                      failsafeValue, CHANNEL_MIN, CHANNEL_MAX);
        return;
    }

    Serial.printf("  PWM Configuration:\n");
    Serial.printf("    - Pin: %s (GPIO%d)\n", handlerConfig.pin, pinInfo->second.pin);
    Serial.printf("    - Failsafe: %d\n", failsafeValue);
    Serial.printf("    - Range: %d to %d\n", min, max);
    Serial.printf("    - Inverted: %s\n", inverted ? "yes" : "no");

    auto *handler = new PWMChannelHandler(pinInfo->second.pin, min, max);
    handler->setup(failsafeValue); // Use failsafe as initial value
    handler->setInverted(inverted);
    this->computer->onChannelChange(channel, handler, failsafeValue);
}

void ConfigManager::configureOnOffHandler(const HandlerConfig &handlerConfig)
{
    if (!handlerConfig.failsafe)
    {
        Serial.println("ERROR: OnOff handler requires 'failsafe' value");
        return;
    }

    auto pinInfo = PIN_MAP.find(handlerConfig.pin);
    if (pinInfo == PIN_MAP.end())
    {
        Serial.printf("ERROR: Invalid pin: %s\n", handlerConfig.pin);
        return;
    }

    uint8_t channel = handlerConfig.channel;
    int failsafeValue = handlerConfig.failsafe;
    auto compareFunc = createThresholdFunction(handlerConfig);

    // Validate failsafe value is within range
    if (failsafeValue < CHANNEL_MIN || failsafeValue > CHANNEL_MAX)
    {
        Serial.printf("ERROR: Failsafe value %d is out of range (%d-%d)\n",
                      failsafeValue, CHANNEL_MIN, CHANNEL_MAX);
        return;
    }

    Serial.printf("  OnOff Configuration:\n");
    Serial.printf("    - Pin: %s (GPIO%d)\n", handlerConfig.pin, pinInfo->second.pin);
    Serial.printf("    - Failsafe: %d\n", failsafeValue);
    Serial.printf("    - Threshold: %d\n", handlerConfig.threshold);
    Serial.printf("    - Operator: %s\n", handlerConfig.op);

    auto *handler = new OnOffChannelHandler(pinInfo->second.pin);
    handler->isOnWhen(compareFunc);
    this->computer->onChannelChange(channel, handler, failsafeValue);
}

void ConfigManager::configureBlinkHandler(const HandlerConfig &handlerConfig)
{
    if (!handlerConfig.failsafe)
    {
        Serial.println("ERROR: Blink handler requires 'failsafe' value");
        return;
    }

    auto pinInfo = PIN_MAP.find(handlerConfig.pin);
    if (pinInfo == PIN_MAP.end())
    {
        Serial.printf("ERROR: Invalid pin: %s\n", handlerConfig.pin);
        return;
    }

    uint8_t channel = handlerConfig.channel;
    int failsafeValue = handlerConfig.failsafe;
    int onTime = handlerConfig.onTime;
    int offTime = handlerConfig.offTime;
    auto compareFunc = createThresholdFunction(handlerConfig);

    // Validate failsafe value is within range
    if (failsafeValue < CHANNEL_MIN || failsafeValue > CHANNEL_MAX)
    {
        Serial.printf("ERROR: Failsafe value %d is out of range (%d-%d)\n",
                      failsafeValue, CHANNEL_MIN, CHANNEL_MAX);
        return;
    }

    Serial.printf("  Blink Configuration:\n");
    Serial.printf("    - Pin: %s (GPIO%d)\n", handlerConfig.pin, pinInfo->second.pin);
    Serial.printf("    - Failsafe: %d\n", failsafeValue);
    Serial.printf("    - Timing: %dms on, %dms off\n", onTime, offTime);
    Serial.printf("    - Threshold: %d\n", handlerConfig.threshold);
    Serial.printf("    - Operator: %s\n", handlerConfig.op);

    auto *handler = new BlinkChannelHandler(pinInfo->second.pin, onTime, offTime);
    handler->isOnWhen(compareFunc);
    this->computer->onChannelChange(channel, handler, failsafeValue);
}

auto ConfigManager::createThresholdFunction(const HandlerConfig &handlerConfig) -> std::function<bool(uint16_t)>
{
    int threshold = handlerConfig.threshold;
    const char *op = handlerConfig.op;

    std::function<bool(uint16_t)> compareFunc;

    if (strcmp(op, "lessThan") == 0)
    {
        compareFunc = [threshold](uint16_t value)
        { return value < threshold; };
    }
    else if (strcmp(op, "greaterThan") == 0)
    {
        compareFunc = [threshold](uint16_t value)
        { return value > threshold; };
    }
    else if (strcmp(op, "equals") == 0)
    {
        compareFunc = [threshold](uint16_t value)
        { return value == threshold; };
    }
    else
    {
        Serial.printf("WARNING: Unknown operator '%s', defaulting to greaterThan\n", op);
        compareFunc = [threshold](uint16_t value)
        { return value > threshold; };
    }

    return compareFunc;
}

Config ConfigManager::parseJson(const char *jsonConfig)
{
    Config config;
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonConfig);

    if (error)
    {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return config;
    }

    Serial.println("JSON parsed successfully");
    JsonArray handlers = doc["handlers"];
    size_t numHandlers = std::min(handlers.size(), static_cast<size_t>(Config::MAX_HANDLERS));
    Serial.printf("Found %d handlers to configure\n", numHandlers);

    config.numHandlers = numHandlers;
    for (size_t i = 0; i < numHandlers; i++)
    {
        JsonObject handler = handlers[i];
        HandlerConfig &handlerConfig = config.handlers[i];

        handlerConfig.setType(handler["type"] | "");
        handlerConfig.setPin(handler["pin"] | "");
        handlerConfig.channel = handler["channel"] | 0;
        handlerConfig.failsafe = handler["failsafe"] | 0;
        handlerConfig.threshold = handler["threshold"] | CHANNEL_MID;
        handlerConfig.setOp(handler["operator"] | "greaterThan");
        handlerConfig.inverted = handler["inverted"] | false;
        handlerConfig.min = handler["min"] | 0;
        handlerConfig.max = handler["max"] | 255;
        handlerConfig.onTime = handler["onTime"] | 300;
        handlerConfig.offTime = handler["offTime"] | 400;
    }

    return config;
}
