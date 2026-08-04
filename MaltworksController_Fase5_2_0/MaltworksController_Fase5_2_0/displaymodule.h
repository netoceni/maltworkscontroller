#ifndef DISPLAYMODULE_H
#define DISPLAYMODULE_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayModule {
public:
  DisplayModule();

  bool begin(
    uint8_t address = 0x3C
  );

  void showStartup();

  void showStatus(
    float refrigeratorTemperature,
    float thermalWellTemperature,
    float setpoint,
    float hysteresis,
    const char* stateText,
    bool waitingForCompressor,
    unsigned long coolingDelaySeconds
  );

  void clear();

private:
  static constexpr uint8_t SCREEN_WIDTH = 128;
  static constexpr uint8_t SCREEN_HEIGHT = 64;
  static constexpr int8_t OLED_RESET_PIN = -1;

  Adafruit_SSD1306 display;
  bool initialized;

  void printTemperature(
    float temperature
  );
};

#endif
