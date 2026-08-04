#include "settingsmodule.h"

SettingsModule::SettingsModule() :
  initialized(false) {
}

bool SettingsModule::begin() {
  initialized =
    preferences.begin(
      STORAGE_NAMESPACE,
      false
    );

  return initialized;
}

void SettingsModule::end() {
  if (!initialized) {
    return;
  }

  preferences.end();
  initialized = false;
}

float SettingsModule::loadSetpoint(
  float defaultValue
) {
  if (!initialized) {
    return defaultValue;
  }

  return preferences.getFloat(
    SETPOINT_KEY,
    defaultValue
  );
}

float SettingsModule::loadHysteresis(
  float defaultValue
) {
  if (!initialized) {
    return defaultValue;
  }

  return preferences.getFloat(
    HYSTERESIS_KEY,
    defaultValue
  );
}

uint32_t SettingsModule::
loadCompressorProtectionSeconds(
  uint32_t defaultValue
) {
  if (!initialized) {
    return defaultValue;
  }

  return preferences.getUInt(
    COMPRESSOR_PROTECTION_KEY,
    defaultValue
  );
}

bool SettingsModule::saveSetpoint(
  float value
) {
  if (!initialized) {
    return false;
  }

  return preferences.putFloat(
    SETPOINT_KEY,
    value
  ) > 0;
}

bool SettingsModule::saveHysteresis(
  float value
) {
  if (!initialized) {
    return false;
  }

  return preferences.putFloat(
    HYSTERESIS_KEY,
    value
  ) > 0;
}

bool SettingsModule::
saveCompressorProtectionSeconds(
  uint32_t value
) {
  if (!initialized) {
    return false;
  }

  return preferences.putUInt(
    COMPRESSOR_PROTECTION_KEY,
    value
  ) > 0;
}

bool SettingsModule::saveControlSettings(
  float setpoint,
  float hysteresis
) {
  if (!initialized) {
    return false;
  }

  bool setpointSaved =
    saveSetpoint(
      setpoint
    );

  bool hysteresisSaved =
    saveHysteresis(
      hysteresis
    );

  return setpointSaved &&
    hysteresisSaved;
}

String SettingsModule::loadWifiSsid() {
  if (!initialized) {
    return "";
  }

  return preferences.getString(
    WIFI_SSID_KEY,
    ""
  );
}

String SettingsModule::loadWifiPassword() {
  if (!initialized) {
    return "";
  }

  return preferences.getString(
    WIFI_PASSWORD_KEY,
    ""
  );
}

bool SettingsModule::saveWifiCredentials(
  const String& ssid,
  const String& password
) {
  if (!initialized) {
    return false;
  }

  size_t ssidBytes =
    preferences.putString(
      WIFI_SSID_KEY,
      ssid
    );

  preferences.putString(
    WIFI_PASSWORD_KEY,
    password
  );

  return ssidBytes > 0;
}

bool SettingsModule::clearWifiCredentials() {
  if (!initialized) {
    return false;
  }

  preferences.remove(
    WIFI_SSID_KEY
  );

  preferences.remove(
    WIFI_PASSWORD_KEY
  );

  return true;
}
