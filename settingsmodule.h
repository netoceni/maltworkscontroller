#ifndef SETTINGSMODULE_H
#define SETTINGSMODULE_H

#include <Arduino.h>
#include <Preferences.h>

class SettingsModule {
public:
  SettingsModule();

  bool begin();
  void end();

  float loadSetpoint(
    float defaultValue
  );

  float loadHysteresis(
    float defaultValue
  );

  uint32_t loadCompressorProtectionSeconds(
    uint32_t defaultValue
  );

  bool saveSetpoint(
    float value
  );

  bool saveHysteresis(
    float value
  );

  bool saveCompressorProtectionSeconds(
    uint32_t value
  );

  bool saveControlSettings(
    float setpoint,
    float hysteresis
  );

  String loadWifiSsid();
  String loadWifiPassword();

  bool saveWifiCredentials(
    const String& ssid,
    const String& password
  );

  bool clearWifiCredentials();

private:
  Preferences preferences;
  bool initialized;

  static constexpr const char*
    STORAGE_NAMESPACE =
      "maltworks";

  static constexpr const char*
    SETPOINT_KEY =
      "setpoint";

  static constexpr const char*
    HYSTERESIS_KEY =
      "hysteresis";

  static constexpr const char*
    COMPRESSOR_PROTECTION_KEY =
      "comp_delay";

  static constexpr const char*
    WIFI_SSID_KEY =
      "wifi_ssid";

  static constexpr const char*
    WIFI_PASSWORD_KEY =
      "wifi_pass";
};

#endif
