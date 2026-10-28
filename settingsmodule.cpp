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


#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <string.h>

bool SettingsModule::deriveLocalPassword(
  const String& password,
  const uint8_t salt[LOCAL_AUTH_SALT_SIZE],
  uint8_t hash[LOCAL_AUTH_HASH_SIZE]
) {
  // ESP-IDF 5.5 / Mbed TLS 3: a variante antiga baseada no
  // mbedtls_md_context_t nao esta mais disponivel.
  return mbedtls_pkcs5_pbkdf2_hmac_ext(
    MBEDTLS_MD_SHA256,
    reinterpret_cast<const unsigned char*>(password.c_str()),
    password.length(),
    salt,
    LOCAL_AUTH_SALT_SIZE,
    LOCAL_AUTH_ITERATIONS,
    LOCAL_AUTH_HASH_SIZE,
    hash
  ) == 0;
}

bool SettingsModule::hasLocalPassword() {
  return initialized &&
    preferences.getBytesLength(LOCAL_AUTH_SALT_KEY) == LOCAL_AUTH_SALT_SIZE &&
    preferences.getBytesLength(LOCAL_AUTH_HASH_KEY) == LOCAL_AUTH_HASH_SIZE;
}

bool SettingsModule::saveLocalPassword(const String& password) {
  if (!initialized || hasLocalPassword() ||
      password.length() < 12 || password.length() > 64) return false;

  uint8_t salt[LOCAL_AUTH_SALT_SIZE];
  uint8_t hash[LOCAL_AUTH_HASH_SIZE];
  esp_fill_random(salt, sizeof(salt));
  if (!deriveLocalPassword(password, salt, hash)) return false;

  // Gravacao de salt antes de hash: estado parcial nunca libera autenticacao.
  if (preferences.putBytes(LOCAL_AUTH_SALT_KEY, salt, sizeof(salt)) != sizeof(salt)) return false;
  if (preferences.putBytes(LOCAL_AUTH_HASH_KEY, hash, sizeof(hash)) != sizeof(hash)) {
    preferences.remove(LOCAL_AUTH_SALT_KEY);
    return false;
  }
  return true;
}

bool SettingsModule::verifyLocalPassword(const String& password) {
  if (!hasLocalPassword()) return false;
  uint8_t salt[LOCAL_AUTH_SALT_SIZE];
  uint8_t expected[LOCAL_AUTH_HASH_SIZE];
  uint8_t actual[LOCAL_AUTH_HASH_SIZE];
  if (preferences.getBytes(LOCAL_AUTH_SALT_KEY, salt, sizeof(salt)) != sizeof(salt) ||
      preferences.getBytes(LOCAL_AUTH_HASH_KEY, expected, sizeof(expected)) != sizeof(expected) ||
      !deriveLocalPassword(password, salt, actual)) return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < sizeof(actual); i++) difference |= actual[i] ^ expected[i];
  return difference == 0;
}

bool SettingsModule::clearLocalPassword() {
  if (!initialized) return false;
  preferences.remove(LOCAL_AUTH_HASH_KEY);
  preferences.remove(LOCAL_AUTH_SALT_KEY);
  return !hasLocalPassword();
}
