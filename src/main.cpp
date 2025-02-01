#include <Arduino.h>
#include "boardcomputer.hpp"
#include "channel-handlers/pwmChannelHandler.hpp"
#include "channel-handlers/onOffChannelHandler.hpp"
#include "channel-handlers/blinkChannelHandler.hpp"
#include "const.hpp"

BoardComputer boardComputer(&Serial0);

PWMChannelHandler steeringPWMHandler(STEERING_PWM_PIN, STEERING_MIN, STEERING_MAX);
PWMChannelHandler throttlePWMHandler(THROTTLE_PWM_PIN);

OnOffChannelHandler headlightHandler(HEADLIGHT_PIN);
BlinkChannelHandler blinkerLeftHandler(BLINKER_LEFT_PIN, 300, 400);
BlinkChannelHandler blinkerRightHandler(BLINKER_RIGHT_PIN, 300, 400);
OnOffChannelHandler brakeLightHandler(BRAKE_LIGHT_PIN);

PWMChannelHandler winch1PWMHandler(WINCH_1_PWM_PIN);
PWMChannelHandler winch2PWMHandler(WINCH_2_PWM_PIN);

#define CHANNEL_SIZE (CHANNEL_MAX - CHANNEL_MIN)

#define CHANNEL_LOWER_QUARTER CHANNEL_MIN + (CHANNEL_SIZE / 4)
#define CHANNEL_UPPER_QUARTER CHANNEL_MAX - (CHANNEL_SIZE / 4)

#define STEERING_LEFT_VALUE CHANNEL_LOWER_QUARTER
#define STEERING_RIGHT_VALUE CHANNEL_UPPER_QUARTER

#define BREAK_LIGHT_THRESHOLD CHANNEL_MID

#define HEADLIGHT_ON_THRESHOLD CHANNEL_MID

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting board computer");

  steeringPWMHandler.setup(PWM_CENTER);
  boardComputer.onChannelChange(STEERING_CHANNEL, &steeringPWMHandler, CHANNEL_MID);

  throttlePWMHandler.setup();
  boardComputer.onChannelChange(THROTTLE_CHANNEL, &throttlePWMHandler, CHANNEL_MID);

  headlightHandler.isOnWhen([](uint16_t value)
                            { 
                              bool isOn = value > HEADLIGHT_ON_THRESHOLD;
                              Serial.printf("headlight is on: %d (value: %d, threshold: %d)\n", isOn, value, HEADLIGHT_ON_THRESHOLD);
                              return isOn;
                            });
  boardComputer.onChannelChange(HEADLIGHT_CHANNEL, &headlightHandler, CHANNEL_MIN);

  // blinker left is on when steering is left
  blinkerLeftHandler.isOnWhen([](uint16_t value)
                              { 
                                bool isOn = value < STEERING_LEFT_VALUE;
                                Serial.printf("blinker left is on: %d (value: %d, threshold: %d)\n", isOn, value, STEERING_LEFT_VALUE);
                                return isOn; 
                              });
  boardComputer.onChannelChange(STEERING_CHANNEL, &blinkerLeftHandler, CHANNEL_MIN);

  // blinker right is on when steering is right
  blinkerRightHandler.isOnWhen([](uint16_t value)
                               { 
                                 bool isOn = value > STEERING_RIGHT_VALUE;
                                 Serial.printf("blinker right is on: %d (value: %d, threshold: %d)\n", isOn, value, STEERING_RIGHT_VALUE);
                                 return isOn; 
                               });
  boardComputer.onChannelChange(STEERING_CHANNEL, &blinkerRightHandler, CHANNEL_MIN);

  // brake light is on when throttle is in the middle (channel mid +/- 10%)
  brakeLightHandler.isOnWhen([](uint16_t value)
                             { 
                               bool isOn = value <= BREAK_LIGHT_THRESHOLD;
                               Serial.printf("brake light is on: %d (value: %d, threshold: %d)\n", isOn, value, BREAK_LIGHT_THRESHOLD);
                               return isOn; 
                             });
  boardComputer.onChannelChange(THROTTLE_CHANNEL, &brakeLightHandler, CHANNEL_MIN);

  winch1PWMHandler.setup(CHANNEL_MID);
  boardComputer.onChannelChange(WINCH_1_CHANNEL, &winch1PWMHandler, CHANNEL_MID);

  winch2PWMHandler.setup(CHANNEL_MID);
  boardComputer.onChannelChange(WINCH_2_CHANNEL, &winch2PWMHandler, CHANNEL_MID);

  boardComputer.start();
}

void loop()
{
}