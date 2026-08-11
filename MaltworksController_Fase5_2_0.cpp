#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "displaymodule.h"
#include "temperaturemodule.h"
#include "relaymodule.h"
#include "controlmodule.h"
#include "webmodule.h"
#include "settingsmodule.h"
#include "historymodule.h"
#include "networkmodule.h"
#include "clockmodule.h"
#include "firmwareinfo.h"
#include "profilemodule.h"
#include "recipelibrarymodule.h"
#include "alarmmodule.h"
#include "eventlogmodule.h"
#include "cloudmodule.h"

constexpr uint8_t OLED_SDA_PIN = 21;
constexpr uint8_t OLED_SCL_PIN = 22;
constexpr uint8_t TEMPERATURE_SENSOR_PIN = 15;
constexpr uint8_t COOLING_RELAY_PIN = 27;
constexpr uint8_t HEATING_RELAY_PIN = 26;

constexpr bool RELAY_ACTIVE_LOW = true;

constexpr float DEFAULT_SETPOINT = 20.0f;
constexpr float DEFAULT_HYSTERESIS = 0.5f;
constexpr uint32_t
  DEFAULT_COMPRESSOR_PROTECTION_SECONDS =
    60UL;

constexpr unsigned long
  SYSTEM_UPDATE_INTERVAL_MS =
    1000UL;

constexpr unsigned long
  HISTORY_INTERVAL_MS =
    15UL * 1000UL;

constexpr float
  SETTINGS_CHANGE_TOLERANCE =
    0.001f;

const char* ACCESS_POINT_NAME =
  "MaltworksController";

const char* ACCESS_POINT_PASSWORD =
  "maltworks";

DisplayModule displayModule;

TemperatureModule temperatureModule(
  TEMPERATURE_SENSOR_PIN
);

RelayModule relayModule(
  COOLING_RELAY_PIN,
  HEATING_RELAY_PIN,
  RELAY_ACTIVE_LOW
);

ControlModule controlModule(
  relayModule
);

HistoryModule historyModule;
SettingsModule settingsModule;
NetworkModule networkModule;
ClockModule clockModule(
  networkModule
);

ProfileModule profileModule(
  clockModule,
  controlModule
);

RecipeLibraryModule recipeLibraryModule;

AlarmModule alarmModule(
  relayModule,
  controlModule
);

EventLogModule eventLogModule(
  clockModule
);

CloudModule cloudModule(
  networkModule,
  clockModule,
  temperatureModule,
  relayModule,
  controlModule,
  settingsModule,
  profileModule,
  alarmModule,
  eventLogModule
);

WebModule webModule(
  temperatureModule,
  relayModule,
  controlModule,
  historyModule,
  networkModule,
  settingsModule,
  clockModule,
  profileModule,
  recipeLibraryModule,
  alarmModule,
  eventLogModule,
  cloudModule
);

unsigned long lastSystemUpdate = 0;
unsigned long lastHistoryUpdate = 0;

float lastSavedSetpoint =
  DEFAULT_SETPOINT;

float lastSavedHysteresis =
  DEFAULT_HYSTERESIS;

bool settingsAvailable =
  false;

bool previousCoolingState = false;
bool previousHeatingState = false;
bool previousWifiConnected = false;
bool previousCloudOnline = false;

CloudModule::Status
  previousCloudStatus =
    CloudModule::Status::CLOUD_DISABLED;

ProfileModule::RunState
  previousProfileState =
    ProfileModule::RunState::STOPPED;

uint8_t previousProfileStage = 0;

bool previousAlarmStates[4] = {
  false,
  false,
  false,
  false
};

bool floatValuesAreDifferent(
  float firstValue,
  float secondValue
) {
  return fabs(
    firstValue -
    secondValue
  ) > SETTINGS_CHANGE_TOLERANCE;
}

void loadSavedSettings() {
  settingsAvailable =
    settingsModule.begin();

  if (!settingsAvailable) {
    controlModule.setSetpoint(
      DEFAULT_SETPOINT
    );

    controlModule.setHysteresis(
      DEFAULT_HYSTERESIS
    );

    controlModule
      .setCompressorProtectionSeconds(
        DEFAULT_COMPRESSOR_PROTECTION_SECONDS
      );

    lastSavedSetpoint =
      DEFAULT_SETPOINT;

    lastSavedHysteresis =
      DEFAULT_HYSTERESIS;

    Serial.println(
      "Memoria de configuracoes indisponivel."
    );

    return;
  }

  float loadedSetpoint =
    settingsModule.loadSetpoint(
      DEFAULT_SETPOINT
    );

  float loadedHysteresis =
    settingsModule.loadHysteresis(
      DEFAULT_HYSTERESIS
    );

  uint32_t loadedCompressorProtection =
    settingsModule
      .loadCompressorProtectionSeconds(
        DEFAULT_COMPRESSOR_PROTECTION_SECONDS
      );

  if (
    loadedSetpoint < -10.0f ||
    loadedSetpoint > 40.0f
  ) {
    loadedSetpoint =
      DEFAULT_SETPOINT;
  }

  if (
    loadedHysteresis < 0.1f ||
    loadedHysteresis > 5.0f
  ) {
    loadedHysteresis =
      DEFAULT_HYSTERESIS;
  }

  if (
    loadedCompressorProtection < 60UL ||
    loadedCompressorProtection > 900UL
  ) {
    loadedCompressorProtection =
      DEFAULT_COMPRESSOR_PROTECTION_SECONDS;
  }

  controlModule.setSetpoint(
    loadedSetpoint
  );

  controlModule.setHysteresis(
    loadedHysteresis
  );

  controlModule
    .setCompressorProtectionSeconds(
      loadedCompressorProtection
    );

  lastSavedSetpoint =
    loadedSetpoint;

  lastSavedHysteresis =
    loadedHysteresis;

  Serial.println(
    "Configuracoes carregadas."
  );
}

void saveSettingsIfChanged() {
  if (!settingsAvailable) {
    return;
  }

  /*
    O setpoint aplicado por uma receita e temporario.
    A configuracao manual permanece preservada durante
    a execucao do perfil.
  */
  if (profileModule.isActive()) {
    return;
  }

  float currentSetpoint =
    controlModule.getSetpoint();

  float currentHysteresis =
    controlModule.getHysteresis();

  bool setpointChanged =
    floatValuesAreDifferent(
      currentSetpoint,
      lastSavedSetpoint
    );

  bool hysteresisChanged =
    floatValuesAreDifferent(
      currentHysteresis,
      lastSavedHysteresis
    );

  if (
    !setpointChanged &&
    !hysteresisChanged
  ) {
    return;
  }

  bool saved =
    settingsModule.saveControlSettings(
      currentSetpoint,
      currentHysteresis
    );

  if (saved) {
    lastSavedSetpoint =
      currentSetpoint;

    lastSavedHysteresis =
      currentHysteresis;

    Serial.println(
      "Configuracoes salvas."
    );
  } else {
    Serial.println(
      "Falha ao salvar configuracoes."
    );
  }
}

void detectAndLogEvents() {
  bool wifiConnected =
    networkModule.isStationConnected();

  if (
    wifiConnected !=
    previousWifiConnected
  ) {
    eventLogModule.add(
      EventLogModule::Category::NETWORK,
      wifiConnected
        ? (
            "Wi-Fi domestico conectado: " +
            networkModule.getStationSsid()
          )
        : String(
            "Wi-Fi domestico desconectado"
          )
    );

    previousWifiConnected =
      wifiConnected;
  }

  bool cloudOnline =
    cloudModule.isOnline();

  if (
    cloudOnline !=
      previousCloudOnline &&
    cloudModule.isEnabled()
  ) {
    eventLogModule.add(
      EventLogModule::Category::CLOUD,
      cloudOnline
        ? "Maltworks Cloud online"
        : "Maltworks Cloud offline"
    );

    previousCloudOnline =
      cloudOnline;
  }

  CloudModule::Status cloudStatus =
    cloudModule.getStatus();

  if (
    cloudStatus !=
      previousCloudStatus &&
    cloudStatus ==
      CloudModule::Status::AUTHENTICATION_ERROR
  ) {
    eventLogModule.add(
      EventLogModule::Category::CLOUD,
      "Token cloud recusado pela API"
    );
  }

  previousCloudStatus = cloudStatus;

  bool cooling =
    relayModule.isCoolingOn();

  if (
    cooling !=
    previousCoolingState
  ) {
    eventLogModule.add(
      EventLogModule::Category::CONTROL,
      cooling
        ? "Resfriamento ligado"
        : "Resfriamento desligado"
    );

    previousCoolingState =
      cooling;
  }

  bool heating =
    relayModule.isHeatingOn();

  if (
    heating !=
    previousHeatingState
  ) {
    eventLogModule.add(
      EventLogModule::Category::CONTROL,
      heating
        ? "Aquecimento ligado"
        : "Aquecimento desligado"
    );

    previousHeatingState =
      heating;
  }

  ProfileModule::RunState profileState =
    profileModule.getRunState();

  if (
    profileState !=
    previousProfileState
  ) {
    if (
      profileState ==
      ProfileModule::RunState::COMPLETED
    ) {
      eventLogModule.add(
        EventLogModule::Category::RECIPE,
        "Perfil concluido: " +
        profileModule.getProfileName()
      );
    } else if (
      profileState ==
      ProfileModule::RunState::WAITING_CLOCK
    ) {
      eventLogModule.add(
        EventLogModule::Category::RECIPE,
        "Perfil aguardando sincronizacao do relogio"
      );
    }

    previousProfileState =
      profileState;
  }

  uint8_t currentStage =
    profileModule.getCurrentStageIndex();

  if (
    profileModule.isActive() &&
    currentStage !=
      previousProfileStage
  ) {
    eventLogModule.add(
      EventLogModule::Category::RECIPE,
      "Etapa " +
      String(
        currentStage + 1
      ) +
      " iniciada em " +
      String(
        profileModule
          .getCurrentTargetTemperature(),
        1
      ) +
      " C"
    );

    previousProfileStage =
      currentStage;
  }

  for (
    uint8_t index = 0;
    index < 4;
    index++
  ) {
    AlarmModule::AlarmId id =
      static_cast<AlarmModule::AlarmId>(
        index
      );

    bool active =
      alarmModule.getAlarmState(
        id
      ).active;

    if (
      active !=
      previousAlarmStates[index]
    ) {
      eventLogModule.add(
        EventLogModule::Category::ALARM,
        String(
          active
            ? "ALARME ATIVO: "
            : "Alarme normalizado: "
        ) +
        alarmModule.getAlarmName(id)
      );

      previousAlarmStates[index] =
        active;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("MALTWORKS CONTROLLER");
  Serial.print("FASE ");
  Serial.println(FirmwareInfo::VERSION);
  Serial.println("==============================");

  Wire.begin(
    OLED_SDA_PIN,
    OLED_SCL_PIN
  );

  relayModule.begin();
  controlModule.begin();
  historyModule.begin();

  loadSavedSettings();

  bool displayOk =
    displayModule.begin();

  if (displayOk) {
    displayModule.showStartup();

    Serial.println(
      "Display OLED inicializado."
    );
  } else {
    Serial.println(
      "Falha no display OLED."
    );
  }

  bool sensorOk =
    temperatureModule.begin();

  if (sensorOk) {
    Serial.println(
      "Sensor DS18B20 encontrado."
    );
  } else {
    Serial.println(
      "Sensor DS18B20 nao encontrado."
    );
  }

  historyModule.addPoint(
    temperatureModule.getRefrigeratorTemperature(),
    controlModule.getSetpoint(),
    temperatureModule.isRefrigeratorSensorConnected()
  );

  String savedWifiSsid = "";
  String savedWifiPassword = "";

  if (settingsAvailable) {
    savedWifiSsid =
      settingsModule.loadWifiSsid();

    savedWifiPassword =
      settingsModule.loadWifiPassword();
  }

  bool networkOk =
    networkModule.begin(
      ACCESS_POINT_NAME,
      ACCESS_POINT_PASSWORD,
      savedWifiSsid,
      savedWifiPassword
    );

  clockModule.begin();
  profileModule.begin();
  recipeLibraryModule.begin();
  alarmModule.begin();
  eventLogModule.begin();
  cloudModule.begin();

  eventLogModule.add(
    EventLogModule::Category::SYSTEM,
    "Controlador inicializado - firmware " +
    String(FirmwareInfo::VERSION)
  );

  previousWifiConnected =
    networkModule.isStationConnected();

  previousCloudOnline =
    cloudModule.isOnline();

  previousCloudStatus =
    cloudModule.getStatus();

  previousCoolingState =
    relayModule.isCoolingOn();

  previousHeatingState =
    relayModule.isHeatingOn();

  previousProfileState =
    profileModule.getRunState();

  previousProfileStage =
    profileModule.getCurrentStageIndex();

  for (
    uint8_t index = 0;
    index < 4;
    index++
  ) {
    previousAlarmStates[index] =
      alarmModule.getAlarmState(
        static_cast<AlarmModule::AlarmId>(
          index
        )
      ).active;
  }

  bool webOk =
    webModule.begin();

  if (
    networkOk &&
    webOk
  ) {
    Serial.println(
      "Interface web inicializada."
    );

    Serial.print(
      "Rede de contingencia: "
    );

    Serial.println(
      ACCESS_POINT_NAME
    );

    Serial.print(
      "Senha: "
    );

    Serial.println(
      ACCESS_POINT_PASSWORD
    );

    Serial.print(
      "Endereco de contingencia: http://"
    );

    Serial.println(
      networkModule.getAccessPointIp()
    );
  } else {
    Serial.println(
      "Falha ao inicializar rede ou interface web."
    );
  }

  Serial.println("==============================");

  delay(1500);

  lastSystemUpdate = 0;
  lastHistoryUpdate = millis();
}

void loop() {
  networkModule.update();
  clockModule.update();
  profileModule.update();
  webModule.update();

  if (
    webModule.isFirmwareUpdateInProgress()
  ) {
    relayModule.allOff();
    delay(2);
    return;
  }

  unsigned long currentTime =
    millis();

  if (
    currentTime - lastSystemUpdate <
    SYSTEM_UPDATE_INTERVAL_MS
  ) {
    return;
  }

  lastSystemUpdate =
    currentTime;

  saveSettingsIfChanged();

  temperatureModule.update();

  float refrigeratorTemperature =
    temperatureModule.getRefrigeratorTemperature();

  bool sensorConnected =
    temperatureModule.isRefrigeratorSensorConnected();

  controlModule.update(
    refrigeratorTemperature,
    sensorConnected
  );

  alarmModule.update(
    refrigeratorTemperature,
    sensorConnected
  );

  cloudModule.update();

  detectAndLogEvents();

  if (
    currentTime - lastHistoryUpdate >=
    HISTORY_INTERVAL_MS
  ) {
    lastHistoryUpdate =
      currentTime;

    historyModule.addPoint(
      refrigeratorTemperature,
      controlModule.getSetpoint(),
      sensorConnected
    );

    Serial.print(
      "Ponto adicionado ao historico. Total: "
    );

    Serial.println(
      historyModule.getPointCount()
    );
  }

  float thermalWellTemperature =
    temperatureModule
      .getThermalWellTemperature();

  unsigned long coolingDelaySeconds =
    controlModule.getCoolingDelayRemainingSeconds();

  bool waitingForCompressor =
    controlModule.getState() ==
    ControlModule::State::WAITING_COOLING;

  displayModule.showStatus(
    refrigeratorTemperature,
    thermalWellTemperature,
    controlModule.getSetpoint(),
    controlModule.getHysteresis(),
    controlModule.getStateText(),
    waitingForCompressor,
    coolingDelaySeconds
  );

  Serial.print("Geladeira: ");

  if (sensorConnected) {
    Serial.print(
      refrigeratorTemperature,
      2
    );

    Serial.println(" C");
  } else {
    Serial.println("ERRO");
  }

  Serial.print("Estado: ");
  Serial.println(
    controlModule.getStateText()
  );

  Serial.print("Resfriamento: ");
  Serial.println(
    relayModule.isCoolingOn()
      ? "LIGADO"
      : "DESLIGADO"
  );

  Serial.print("Aquecimento: ");
  Serial.println(
    relayModule.isHeatingOn()
      ? "LIGADO"
      : "DESLIGADO"
  );

  if (waitingForCompressor) {
    Serial.print(
      "Compressor liberado em: "
    );

    Serial.print(
      coolingDelaySeconds
    );

    Serial.println(" s");
  }

  Serial.println("------------------------------");
}
