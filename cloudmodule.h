#ifndef CLOUDMODULE_H
#define CLOUDMODULE_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "alarmmodule.h"
#include "clockmodule.h"
#include "controlmodule.h"
#include "eventlogmodule.h"
#include "networkmodule.h"
#include "profilemodule.h"
#include "relaymodule.h"
#include "settingsmodule.h"
#include "temperaturemodule.h"

class CloudModule {
public:
  enum class Status : uint8_t {
    CLOUD_DISABLED,
    CONFIGURATION_ERROR,
    WAITING_NETWORK,
    WAITING_CLOCK,
    READY,
    SENDING,
    ONLINE,
    AUTHENTICATION_ERROR,
    TLS_ERROR,
    NETWORK_ERROR,
    SERVER_ERROR
  };

  CloudModule(
    NetworkModule& networkModule,
    ClockModule& clockModule,
    TemperatureModule& temperatureModule,
    RelayModule& relayModule,
    ControlModule& controlModule,
    SettingsModule& settingsModule,
    ProfileModule& profileModule,
    AlarmModule& alarmModule,
    EventLogModule& eventLogModule
  );

  bool begin();
  void update();

  bool saveConfiguration(
    bool enabled,
    const String& telemetryUrl,
    uint32_t intervalSeconds
  );

  bool regenerateDeviceToken();
  bool requestImmediateSync();

  String getDeviceId() const;
  String getDeviceTokenHint() const;
  String getTelemetryUrl() const;
  String getLastError() const;

  uint32_t getTelemetryIntervalSeconds() const;
  uint32_t getLastSuccessEpoch() const;
  uint32_t getSecondsUntilNextAttempt() const;

  int getLastHttpCode() const;

  bool isEnabled() const;
  bool isConfigured() const;
  bool isOnline() const;
  bool isRequestInProgress() const;

  Status getStatus() const;
  const char* getStatusText() const;

private:
  static constexpr const char*
    STORAGE_NAMESPACE = "mwcloud";

  static constexpr const char*
    CONFIGURATION_NAMESPACE = "mwcloudcfg";

  static constexpr const char*
    DEFAULT_TELEMETRY_URL =
      "https://api.maltworks.com.br/v1/telemetry";

  static constexpr uint32_t
    DEFAULT_INTERVAL_SECONDS = 5;

  static constexpr uint32_t
    MINIMUM_INTERVAL_SECONDS = 5;

  static constexpr uint32_t
    MAXIMUM_INTERVAL_SECONDS = 3600;

  static constexpr size_t
    MAXIMUM_URL_LENGTH = 190;

  static constexpr size_t
    TOKEN_HEX_LENGTH = 64;

  static constexpr size_t
    MAXIMUM_PAYLOAD_LENGTH = 3400;

  static constexpr size_t
    COMMAND_ID_LENGTH = 36;

  static constexpr size_t
    MAXIMUM_STAGE_PLAN_LENGTH = 220;

  static constexpr uint32_t
    WORKER_STACK_SIZE_BYTES =
      16UL * 1024UL;

  struct TelemetryJob {
    char url[MAXIMUM_URL_LENGTH + 1];
    char token[TOKEN_HEX_LENGTH + 1];
    char payload[MAXIMUM_PAYLOAD_LENGTH + 1];
    char acknowledgedCommandId[COMMAND_ID_LENGTH + 1];
    uint32_t sentEpoch;
  };

  struct ConfigurationCommand {
    float hysteresis = 0.5f;
    float refrigeratorOffset = 0.0f;
    float thermalWellOffset = 0.0f;
    float highTemperatureLimit = 35.0f;
    float lowTemperatureLimit = -5.0f;
    float minimumExpectedChange = 0.5f;
    uint32_t compressorProtectionSeconds = 60UL;
    uint32_t responseTimeoutSeconds = 5400UL;
    bool sensorAlarmEnabled = true;
    bool highTemperatureEnabled = true;
    bool lowTemperatureEnabled = true;
    bool responseAlarmEnabled = true;
  };

  NetworkModule& network;
  ClockModule& clock;
  TemperatureModule& temperatures;
  RelayModule& relays;
  ControlModule& control;
  SettingsModule& settings;
  ProfileModule& profile;
  AlarmModule& alarms;
  EventLogModule& eventLog;

  String deviceId;
  String deviceToken;
  String telemetryUrl;
  String bootId;
  String lastError;
  String pendingCommandId;
  String pendingCommandType;
  String pendingProfileName;
  String pendingStagePlan;
  String acknowledgementCommandId;
  String acknowledgementStatus;
  String acknowledgementMessage;

  bool initialized;
  bool enabled;
  bool requestInProgress;
  bool hasSuccessfulSync;
  bool pendingCommandAvailable;
  bool acknowledgementPending;

  float pendingCommandSetpoint;
  float acknowledgementSetpoint;

  ConfigurationCommand
    pendingConfiguration;

  uint32_t pendingCommandExpiresAt;

  uint32_t telemetryIntervalSeconds;
  uint32_t telemetrySequence;
  uint32_t lastSuccessEpoch;
  uint32_t consecutiveFailures;

  unsigned long lastSuccessMillis;
  unsigned long lastAttemptStartedMillis;
  unsigned long nextAttemptMillis;

  int lastHttpCode;
  Status status;

  QueueHandle_t telemetryQueue;
  mutable SemaphoreHandle_t stateMutex;
  TaskHandle_t workerTask;
  TelemetryJob workerJob;
  UBaseType_t minimumWorkerStackRemaining;

  static void workerTaskEntry(
    void* parameter
  );

  void workerLoop();
  void sendTelemetry(
    const TelemetryJob& job
  );

  void completeRequest(
    bool success,
    Status resultStatus,
    int httpCode,
    const String& error,
    uint32_t sentEpoch,
    const String& acknowledgedCommandId = ""
  );

  bool createWorker();
  bool enqueueTelemetry();
  void applyPendingCommand();
  void handleCommandResponse(
    const String& response
  );

  String buildDeviceId() const;
  String generateDeviceToken() const;
  String generateBootId() const;
  String buildTelemetryPayload();

  String normalizeTelemetryUrl(
    const String& candidate
  ) const;

  String jsonEscape(
    const String& text
  ) const;

  bool isValidToken(
    const String& token
  ) const;

  bool isValidCommandId(
    const String& commandId
  ) const;

  bool extractJsonString(
    const String& json,
    const String& key,
    String& value
  ) const;

  bool extractJsonFloat(
    const String& json,
    const String& key,
    float& value
  ) const;

  bool extractJsonUnsigned(
    const String& json,
    const String& key,
    uint32_t& value
  ) const;

  bool extractJsonBoolean(
    const String& json,
    const String& key,
    bool& value
  ) const;

  bool isValidConfigurationCommand(
    const ConfigurationCommand& candidate
  ) const;

  bool parseStagePlan(
    const String& stagePlan,
    ProfileModule::Stage* stages,
    uint8_t& stageCount
  ) const;

  bool configurationReadyUnsafe() const;

  bool failConfigurationSave(
    const String& error
  );

  unsigned long retryDelayMilliseconds(
    uint32_t failureCount
  ) const;

  void lockState() const;
  void unlockState() const;
  void setStatus(Status newStatus);
};

#endif
