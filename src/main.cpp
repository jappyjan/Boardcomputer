#include <Arduino.h>
#include "boardcomputer.hpp"
#include "channel-handlers/pwmChannelHandler.hpp"
#include "channel-handlers/onOffChannelHandler.hpp"
#include "const.hpp"

BoardComputer boardComputer;
PWMChannelHandler steeringPWMHandler(STEERING_PWM_PIN);

OnOffChannelHandler headlightHandler(HEADLIGHT_PIN);
OnOffChannelHandler blinkerLeftHandler(BLINKER_LEFT_PIN);
OnOffChannelHandler blinkerRightHandler(BLINKER_RIGHT_PIN);
OnOffChannelHandler brakeLightHandler(BRAKE_LIGHT_PIN);

#define CHANNEL_LOWER_QUARTER (CHANNEL_MAX - CHANNEL_MIN) / 4
#define CHANNEL_UPPER_QUARTER (CHANNEL_MAX - CHANNEL_MIN) / 4 * 3

#define STEERING_LEFT_VALUE CHANNEL_LOWER_QUARTER
#define STEERING_RIGHT_VALUE CHANNEL_UPPER_QUARTER

#define THROTTLE_ON_BREAK_VALUE_MIN CHANNEL_MID * 0.9
#define THROTTLE_ON_BREAK_VALUE_MAX CHANNEL_MID * 1.1

#define HEADLIGHT_ON_THRESHOLD CHANNEL_MID

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting board computer");

  steeringPWMHandler.setup();
  boardComputer.onChannelChange(STEERING_CHANNEL, &steeringPWMHandler);

  headlightHandler.isOnWhen([](uint16_t value)
                            { return value > HEADLIGHT_ON_THRESHOLD; });
  boardComputer.onChannelChange(HEADLIGHT_CHANNEL, &headlightHandler);

  // blinker left is on when steering is left
  blinkerLeftHandler.isOnWhen([](uint16_t value)
                              { return value > STEERING_LEFT_VALUE; });
  boardComputer.onChannelChange(STEERING_CHANNEL, &blinkerLeftHandler);

  // blinker right is on when steering is right
  blinkerRightHandler.isOnWhen([](uint16_t value)
                               { return value > STEERING_RIGHT_VALUE; });
  boardComputer.onChannelChange(STEERING_CHANNEL, &blinkerRightHandler);

  // brake light is on when throttle is in the middle (channel mid +/- 10%)
  brakeLightHandler.isOnWhen([](uint16_t value)
                             { return value > THROTTLE_ON_BREAK_VALUE_MIN && value < THROTTLE_ON_BREAK_VALUE_MAX; });
  boardComputer.onChannelChange(THROTTLE_CHANNEL, &brakeLightHandler);

  boardComputer.start();
}

void loop()
{
}