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

  // Senha administrativa local; nunca armazenar texto puro.
  bool hasLocalPassword();
  bool saveLocalPassword(const String& password);
  bool verifyLocalPassword(const String& password);
  bool clearLocalPassword(); // Recuperacao apenas por acao fisica.

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

  static constexpr const char*
    LOCAL_AUTH_SALT_KEY =
      "auth_salt";

  static constexpr const char*
    LOCAL_AUTH_HASH_KEY =
      "auth_hash";

  static constexpr uint32_t
    LOCAL_AUTH_ITERATIONS = 12000;

  static constexpr size_t
    LOCAL_AUTH_SALT_SIZE = 16;

  static constexpr size_t
    LOCAL_AUTH_HASH_SIZE = 32;

  static bool deriveLocalPassword(
    const String& password,
    const uint8_t salt[LOCAL_AUTH_SALT_SIZE],
    uint8_t hash[LOCAL_AUTH_HASH_SIZE]
  );
};

#endif
