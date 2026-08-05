#include <math.h>

#include "displaymodule.h"
#include "firmwareinfo.h"

DisplayModule::DisplayModule() :
  display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET_PIN
  ),
  initialized(false) {
}

bool DisplayModule::begin(
  uint8_t address
) {
  initialized =
    display.begin(
      SSD1306_SWITCHCAPVCC,
      address
    );

  if (!initialized) {
    return false;
  }

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.display();

  return true;
}

void DisplayModule::showStartup() {
  if (!initialized) {
    return;
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(4, 8);
  display.println("MALTWORKS");

  display.setTextSize(1);
  display.setCursor(18, 38);
  display.println(
    String("Controller ") +
    FirmwareInfo::VERSION
  );

  display.setCursor(29, 52);
  display.println(
    "Iniciando..."
  );

  display.display();
}

void DisplayModule::showStatus(
  float refrigeratorTemperature,
  float thermalWellTemperature,
  float setpoint,
  float hysteresis,
  const char* stateText,
  bool waitingForCompressor,
  unsigned long coolingDelaySeconds
) {
  if (!initialized) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("GEL:");

  display.setTextSize(2);
  display.setCursor(30, 0);

  printTemperature(
    refrigeratorTemperature
  );

  display.setTextSize(1);
  display.setCursor(0, 22);
  display.print("POCO:");

  display.setCursor(42, 22);

  printTemperature(
    thermalWellTemperature
  );

  display.setTextSize(1);
  display.setCursor(0, 36);

  display.print("SP:");
  display.print(
    setpoint,
    1
  );

  display.print(" H:");
  display.print(
    hysteresis,
    1
  );

  display.drawLine(
    0,
    47,
    127,
    47,
    SSD1306_WHITE
  );

  display.setCursor(0, 53);

  if (waitingForCompressor) {
    display.print("PROTECAO: ");
    display.print(
      coolingDelaySeconds
    );
    display.print("s");
  } else {
    display.print(
      stateText
    );
  }

  display.display();
}

void DisplayModule::printTemperature(
  float temperature
) {
  if (isnan(temperature)) {
    display.print("--.-");
    return;
  }

  display.print(
    temperature,
    1
  );

  display.print("C");
}

void DisplayModule::clear() {
  if (!initialized) {
    return;
  }

  display.clearDisplay();
  display.display();
}
