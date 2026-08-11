#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_err.h>
#include <esp_system.h>
#include <inttypes.h>
#include <math.h>
#include <nvs.h>
#include <stdlib.h>

#include "cloudmodule.h"
#include "cloudroots.h"
#include "firmwareinfo.h"

CloudModule::CloudModule(
  NetworkModule& networkModule,
  ClockModule& clockModule,
  TemperatureModule& temperatureModule,
  RelayModule& relayModule,
  ControlModule& controlModule,
  SettingsModule& settingsModule,
  ProfileModule& profileModule,
  AlarmModule& alarmModule,
  EventLogModule& eventLogModule
) :
  network(networkModule),
  clock(clockModule),
  temperatures(temperatureModule),
  relays(relayModule),
  control(controlModule),
  settings(settingsModule),
  profile(profileModule),
  alarms(alarmModule),
  eventLog(eventLogModule),
  deviceId(""),
  deviceToken(""),
  telemetryUrl(""),
  bootId(""),
  lastError(""),
  pendingCommandId(""),
  pendingCommandType(""),
  pendingProfileName(""),
  pendingStagePlan(""),
  acknowledgementCommandId(""),
  acknowledgementStatus(""),
  acknowledgementMessage(""),
  initialized(false),
  enabled(false),
  requestInProgress(false),
  hasSuccessfulSync(false),
  pendingCommandAvailable(false),
  acknowledgementPending(false),
  pendingCommandSetpoint(0.0f),
  acknowledgementSetpoint(0.0f),
  pendingCommandExpiresAt(0),
  telemetryIntervalSeconds(
    DEFAULT_INTERVAL_SECONDS
  ),
  telemetrySequence(0),
  lastSuccessEpoch(0),
  consecutiveFailures(0),
  lastSuccessMillis(0),
  lastAttemptStartedMillis(0),
  nextAttemptMillis(0),
  lastHttpCode(0),
  status(Status::CLOUD_DISABLED),
  telemetryQueue(nullptr),
  stateMutex(nullptr),
  workerTask(nullptr),
  workerJob({}),
  minimumWorkerStackRemaining(
    WORKER_STACK_SIZE_BYTES
  ) {
}

bool CloudModule::begin() {
  deviceId = buildDeviceId();
  bootId = generateBootId();

  stateMutex = xSemaphoreCreateMutex();

  if (stateMutex == nullptr) {
    Serial.println(
      "Falha ao criar mutex do modulo cloud."
    );

    return false;
  }

  Preferences storage;

  bool storageAvailable =
    storage.begin(
      STORAGE_NAMESPACE,
      false
    );

  if (!storageAvailable) {
    lastError =
      "Memoria de configuracao cloud indisponivel.";

    status =
      Status::CONFIGURATION_ERROR;

    Serial.println(lastError);

    return false;
  }

  enabled =
    storage.getBool(
      "enabled",
      false
    );

  telemetryUrl =
    normalizeTelemetryUrl(
      storage.getString(
        "url",
        ""
      )
    );

  telemetryIntervalSeconds =
    storage.getUInt(
      "interval",
      DEFAULT_INTERVAL_SECONDS
    );

  if (
    telemetryIntervalSeconds <
      MINIMUM_INTERVAL_SECONDS ||
    telemetryIntervalSeconds >
      MAXIMUM_INTERVAL_SECONDS
  ) {
    telemetryIntervalSeconds =
      DEFAULT_INTERVAL_SECONDS;
  }

  deviceToken =
    storage.getString(
      "token",
      ""
    );

  if (!isValidToken(deviceToken)) {
    deviceToken =
      generateDeviceToken();

    if (
      storage.putString(
        "token",
        deviceToken
      ) != deviceToken.length()
    ) {
      storage.end();

      lastError =
        "Falha ao persistir o token do dispositivo.";

      status =
        Status::CONFIGURATION_ERROR;

      Serial.println(lastError);

      return false;
    }
  }

  /*
    O handle da NVS existe somente durante cada
    transacao. Isso evita que um handle mantido
    desde o boot fique invalido antes de um
    salvamento feito pela interface web.
  */
  storage.end();

  /*
    A partir da 5.0.3, a configuracao operacional fica
    em um namespace separado e usa a API NVS nativa.
    O namespace mwcloud permanece com o token, portanto
    a identidade do dispositivo e preservada.
  */
  nvs_handle_t configurationHandle;
  esp_err_t configurationOpenResult =
    nvs_open(
      CONFIGURATION_NAMESPACE,
      NVS_READWRITE,
      &configurationHandle
    );

  if (configurationOpenResult == ESP_OK) {
    char savedUrl[MAXIMUM_URL_LENGTH + 1] = {0};
    size_t savedUrlLength = sizeof(savedUrl);
    uint32_t savedInterval =
      DEFAULT_INTERVAL_SECONDS;
    uint8_t savedEnabled = 0;

    if (
      nvs_get_str(
        configurationHandle,
        "url",
        savedUrl,
        &savedUrlLength
      ) == ESP_OK
    ) {
      telemetryUrl =
        normalizeTelemetryUrl(
          String(savedUrl)
        );
    }

    if (
      nvs_get_u32(
        configurationHandle,
        "interval",
        &savedInterval
      ) == ESP_OK &&
      savedInterval >= MINIMUM_INTERVAL_SECONDS &&
      savedInterval <= MAXIMUM_INTERVAL_SECONDS
    ) {
      telemetryIntervalSeconds = savedInterval;
    }

    if (
      nvs_get_u8(
        configurationHandle,
        "enabled",
        &savedEnabled
      ) == ESP_OK
    ) {
      enabled = savedEnabled != 0;
    }

    if (telemetryUrl.length() == 0) {
      telemetryUrl =
        DEFAULT_TELEMETRY_URL;

      enabled = true;

      nvs_set_str(
        configurationHandle,
        "url",
        telemetryUrl.c_str()
      );

      nvs_set_u32(
        configurationHandle,
        "interval",
        telemetryIntervalSeconds
      );

      nvs_set_u8(
        configurationHandle,
        "enabled",
        1
      );

      nvs_commit(
        configurationHandle
      );

      Serial.println(
        "Configuracao cloud oficial aplicada no primeiro uso."
      );
    }

    nvs_close(configurationHandle);
  }

  if (!createWorker()) {
    lastError =
      "Falha ao iniciar o transporte cloud.";

    status =
      Status::CONFIGURATION_ERROR;

    Serial.println(lastError);

    return false;
  }

  initialized = true;

  if (!enabled) {
    status = Status::CLOUD_DISABLED;
  } else if (!configurationReadyUnsafe()) {
    status =
      Status::CONFIGURATION_ERROR;
  } else {
    status =
      Status::WAITING_NETWORK;
  }

  Serial.print(
    "Device ID: "
  );

  Serial.println(deviceId);

  Serial.println(
    enabled
      ? "Sincronizacao cloud habilitada."
      : "Sincronizacao cloud desabilitada."
  );

  return true;
}

void CloudModule::update() {
  if (!initialized) {
    return;
  }

  applyPendingCommand();

  lockState();

  bool cloudEnabled = enabled;
  bool configured =
    configurationReadyUnsafe();
  bool busy = requestInProgress;
  bool hadSuccessfulSync =
    hasSuccessfulSync;
  unsigned long successfulAt =
    lastSuccessMillis;
  unsigned long scheduledAt =
    nextAttemptMillis;

  unlockState();

  if (!cloudEnabled) {
    setStatus(Status::CLOUD_DISABLED);
    return;
  }

  if (!configured) {
    setStatus(
      Status::CONFIGURATION_ERROR
    );
    return;
  }

  if (!network.isStationConnected()) {
    setStatus(
      Status::WAITING_NETWORK
    );
    return;
  }

  if (!clock.isSynchronized()) {
    setStatus(
      Status::WAITING_CLOCK
    );
    return;
  }

  if (busy) {
    return;
  }

  unsigned long currentTime =
    millis();

  if (
    static_cast<long>(
      currentTime - scheduledAt
    ) < 0
  ) {
    unsigned long onlineWindow =
      max(
        180UL * 1000UL,
        telemetryIntervalSeconds *
          3UL * 1000UL
      );

    if (
      hadSuccessfulSync &&
      currentTime - successfulAt <=
        onlineWindow
    ) {
      setStatus(Status::ONLINE);
    } else {
      setStatus(Status::READY);
    }

    return;
  }

  if (!enqueueTelemetry()) {
    completeRequest(
      false,
      Status::NETWORK_ERROR,
      0,
      "Falha ao enfileirar telemetria.",
      0
    );
  }
}

bool CloudModule::saveConfiguration(
  bool cloudEnabled,
  const String& candidateUrl,
  uint32_t intervalSeconds
) {
  if (!initialized) {
    return failConfigurationSave(
      "Modulo cloud nao inicializado."
    );
  }

  String normalizedUrl =
    normalizeTelemetryUrl(
      candidateUrl
    );

  if (
    intervalSeconds <
      MINIMUM_INTERVAL_SECONDS ||
    intervalSeconds >
      MAXIMUM_INTERVAL_SECONDS
  ) {
    return failConfigurationSave(
      "O intervalo deve estar entre 5 e 3600 segundos."
    );
  }

  if (
    cloudEnabled &&
    normalizedUrl.length() == 0
  ) {
    return failConfigurationSave(
      "Use uma URL HTTPS valida."
    );
  }

  nvs_handle_t storage;
  esp_err_t openResult =
    nvs_open(
      CONFIGURATION_NAMESPACE,
      NVS_READWRITE,
      &storage
    );

  if (openResult != ESP_OK) {
    return failConfigurationSave(
      String(
        "Nao foi possivel abrir a memoria cloud: "
      ) + esp_err_to_name(openResult) + "."
    );
  }

  esp_err_t urlResult =
    nvs_set_str(
      storage,
      "url",
      normalizedUrl.c_str()
    );

  esp_err_t intervalResult =
    nvs_set_u32(
      storage,
      "interval",
      intervalSeconds
    );

  esp_err_t enabledResult =
    nvs_set_u8(
      storage,
      "enabled",
      cloudEnabled ? 1 : 0
    );

  esp_err_t commitResult =
    nvs_commit(storage);

  if (
    urlResult != ESP_OK ||
    intervalResult != ESP_OK ||
    enabledResult != ESP_OK ||
    commitResult != ESP_OK
  ) {
    String storageError =
      "Falha NVS cloud. URL=";
    storageError += esp_err_to_name(urlResult);
    storageError += ", intervalo=";
    storageError += esp_err_to_name(intervalResult);
    storageError += ", habilitacao=";
    storageError += esp_err_to_name(enabledResult);
    storageError += ", commit=";
    storageError += esp_err_to_name(commitResult);
    storageError += ".";

    nvs_close(storage);

    return failConfigurationSave(
      storageError
    );
  }

  nvs_close(storage);

  lockState();

  enabled = cloudEnabled;
  telemetryUrl = normalizedUrl;
  telemetryIntervalSeconds =
    intervalSeconds;

  nextAttemptMillis = 0;
  consecutiveFailures = 0;
  lastError = "";
  lastHttpCode = 0;

  if (!enabled) {
    status = Status::CLOUD_DISABLED;
  } else if (!configurationReadyUnsafe()) {
    status =
      Status::CONFIGURATION_ERROR;
  } else {
    status = Status::READY;
  }

  unlockState();

  return true;
}

bool CloudModule::failConfigurationSave(
  const String& error
) {
  lockState();
  lastError = error;
  unlockState();

  Serial.print(
    "Configuracao cloud: "
  );

  Serial.println(error);

  return false;
}

bool CloudModule::regenerateDeviceToken() {
  if (!initialized) {
    return false;
  }

  lockState();

  if (requestInProgress) {
    unlockState();
    return false;
  }

  unlockState();

  String newToken =
    generateDeviceToken();

  Preferences storage;

  if (
    !storage.begin(
      STORAGE_NAMESPACE,
      false
    )
  ) {
    return false;
  }

  bool tokenSaved =
    storage.putString(
      "token",
      newToken
    ) == newToken.length() &&
    storage.getString(
      "token",
      ""
    ) == newToken;

  storage.end();

  if (!tokenSaved) {
    return false;
  }

  lockState();

  deviceToken = newToken;
  hasSuccessfulSync = false;
  lastSuccessEpoch = 0;
  lastSuccessMillis = 0;
  consecutiveFailures = 0;
  nextAttemptMillis = 0;
  lastHttpCode = 0;
  lastError = "";
  status = enabled
    ? Status::READY
    : Status::CLOUD_DISABLED;

  unlockState();

  return true;
}

bool CloudModule::requestImmediateSync() {
  if (!initialized) {
    return false;
  }

  lockState();

  bool available =
    enabled &&
    configurationReadyUnsafe() &&
    !requestInProgress;

  if (available) {
    nextAttemptMillis = 0;
  }

  unlockState();

  return available;
}

String CloudModule::getDeviceId() const {
  return deviceId;
}

String CloudModule::getDeviceTokenHint() const {
  lockState();

  String compact =
    deviceToken.length() >= 16
      ? deviceToken.substring(
          deviceToken.length() - 16
        )
      : "";

  String hint = compact.length() == 16
    ? compact.substring(0, 4) + "-" +
      compact.substring(4, 8) + "-" +
      compact.substring(8, 12) + "-" +
      compact.substring(12, 16)
    : "---- ---- ---- ----";

  unlockState();

  return hint;
}

String CloudModule::getTelemetryUrl() const {
  lockState();
  String value = telemetryUrl;
  unlockState();
  return value;
}

String CloudModule::getLastError() const {
  lockState();
  String value = lastError;
  unlockState();
  return value;
}

uint32_t CloudModule::
getTelemetryIntervalSeconds() const {
  lockState();
  uint32_t value =
    telemetryIntervalSeconds;
  unlockState();
  return value;
}

uint32_t CloudModule::
getLastSuccessEpoch() const {
  lockState();
  uint32_t value = lastSuccessEpoch;
  unlockState();
  return value;
}

uint32_t CloudModule::
getSecondsUntilNextAttempt() const {
  lockState();

  unsigned long scheduledAt =
    nextAttemptMillis;
  bool busy = requestInProgress;

  unlockState();

  if (busy) {
    return 0;
  }

  unsigned long currentTime = millis();

  if (
    static_cast<long>(
      currentTime - scheduledAt
    ) >= 0
  ) {
    return 0;
  }

  return (
    scheduledAt - currentTime + 999UL
  ) / 1000UL;
}

int CloudModule::getLastHttpCode() const {
  lockState();
  int value = lastHttpCode;
  unlockState();
  return value;
}

bool CloudModule::isEnabled() const {
  lockState();
  bool value = enabled;
  unlockState();
  return value;
}

bool CloudModule::isConfigured() const {
  lockState();
  bool value =
    configurationReadyUnsafe();
  unlockState();
  return value;
}

bool CloudModule::isOnline() const {
  lockState();

  bool successful =
    hasSuccessfulSync;
  unsigned long successfulAt =
    lastSuccessMillis;
  uint32_t interval =
    telemetryIntervalSeconds;
  bool cloudEnabled = enabled;

  unlockState();

  if (
    !cloudEnabled ||
    !successful ||
    !network.isStationConnected()
  ) {
    return false;
  }

  unsigned long onlineWindow =
    max(
      180UL * 1000UL,
      interval * 3UL * 1000UL
    );

  return millis() - successfulAt <=
    onlineWindow;
}

bool CloudModule::
isRequestInProgress() const {
  lockState();
  bool value = requestInProgress;
  unlockState();
  return value;
}

CloudModule::Status
CloudModule::getStatus() const {
  lockState();
  Status value = status;
  unlockState();
  return value;
}

const char* CloudModule::getStatusText() const {
  switch (getStatus()) {
    case Status::CLOUD_DISABLED:
      return "DESABILITADA";

    case Status::CONFIGURATION_ERROR:
      return "CONFIGURACAO INCOMPLETA";

    case Status::WAITING_NETWORK:
      return "AGUARDANDO WI-FI";

    case Status::WAITING_CLOCK:
      return "AGUARDANDO RELOGIO";

    case Status::READY:
      return "PRONTA PARA SINCRONIZAR";

    case Status::SENDING:
      return "ENVIANDO TELEMETRIA";

    case Status::ONLINE:
      return "ONLINE";

    case Status::AUTHENTICATION_ERROR:
      return "FALHA DE AUTENTICACAO";

    case Status::TLS_ERROR:
      return "FALHA TLS";

    case Status::NETWORK_ERROR:
      return "FALHA DE REDE";

    case Status::SERVER_ERROR:
      return "FALHA DO SERVIDOR";

    default:
      return "DESCONHECIDO";
  }
}

void CloudModule::workerTaskEntry(
  void* parameter
) {
  CloudModule* instance =
    static_cast<CloudModule*>(
      parameter
    );

  instance->workerLoop();
}

void CloudModule::workerLoop() {
  while (true) {
    if (
      xQueueReceive(
        telemetryQueue,
        &workerJob,
        portMAX_DELAY
      ) == pdTRUE
    ) {
      sendTelemetry(workerJob);

      UBaseType_t remaining =
        uxTaskGetStackHighWaterMark(
          nullptr
        );

      if (
        remaining <
          minimumWorkerStackRemaining
      ) {
        minimumWorkerStackRemaining =
          remaining;

        Serial.print(
          "Pilha livre minima da tarefa cloud: "
        );

        Serial.print(
          minimumWorkerStackRemaining
        );

        Serial.println(
          " bytes."
        );
      }
    }
  }
}

void CloudModule::sendTelemetry(
  const TelemetryJob& job
) {
  if (WiFi.status() != WL_CONNECTED) {
    completeRequest(
      false,
      Status::NETWORK_ERROR,
      0,
      "Wi-Fi desconectado durante o envio.",
      job.sentEpoch
    );

    return;
  }

  WiFiClientSecure secureClient;
  HTTPClient http;

  secureClient.setCACert(
    MALTWORKS_CLOUD_ROOT_CA
  );

  http.setConnectTimeout(4000);
  http.setTimeout(5000);

  if (!http.begin(secureClient, job.url)) {
    completeRequest(
      false,
      Status::TLS_ERROR,
      0,
      "Nao foi possivel iniciar a conexao HTTPS.",
      job.sentEpoch
    );

    return;
  }

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "Authorization",
    String("Bearer ") + job.token
  );

  http.addHeader(
    "X-Maltworks-Device-ID",
    deviceId
  );

  http.addHeader(
    "X-Maltworks-Firmware",
    FirmwareInfo::VERSION
  );

  http.addHeader(
    "User-Agent",
    String("MaltworksController/") +
      FirmwareInfo::VERSION
  );

  int httpCode =
    http.POST(
      reinterpret_cast<uint8_t*>(
        const_cast<char*>(job.payload)
      ),
      strlen(job.payload)
    );

  if (
    httpCode >= 200 &&
    httpCode < 300
  ) {
    String response =
      http.getString();

    http.end();

    handleCommandResponse(response);

    completeRequest(
      true,
      Status::ONLINE,
      httpCode,
      "",
      job.sentEpoch,
      String(
        job.acknowledgedCommandId
      )
    );

    return;
  }

  Status resultStatus =
    Status::SERVER_ERROR;

  String error;

  if (
    httpCode == 401 ||
    httpCode == 403
  ) {
    resultStatus =
      Status::AUTHENTICATION_ERROR;

    error =
      "Token recusado pela API.";
  } else if (httpCode < 0) {
    error =
      HTTPClient::errorToString(
        httpCode
      );

    String normalizedError = error;
    normalizedError.toUpperCase();

    resultStatus =
      normalizedError.indexOf("SSL") >= 0 ||
      normalizedError.indexOf("TLS") >= 0
        ? Status::TLS_ERROR
        : Status::NETWORK_ERROR;
  } else {
    error =
      "API respondeu HTTP " +
      String(httpCode) +
      ".";
  }

  http.end();

  completeRequest(
    false,
    resultStatus,
    httpCode,
    error,
    job.sentEpoch
  );
}

void CloudModule::completeRequest(
  bool success,
  Status resultStatus,
  int httpCode,
  const String& error,
  uint32_t sentEpoch,
  const String& acknowledgedCommandId
) {
  lockState();

  requestInProgress = false;
  lastHttpCode = httpCode;

  if (!enabled) {
    status = Status::CLOUD_DISABLED;
    unlockState();
    return;
  }

  if (success) {
    unsigned long completedAt = millis();
    unsigned long scheduledAt =
      lastAttemptStartedMillis +
      telemetryIntervalSeconds * 1000UL;

    hasSuccessfulSync = true;
    lastSuccessMillis = completedAt;
    lastSuccessEpoch = sentEpoch;
    consecutiveFailures = 0;
    lastError = "";
    status = Status::ONLINE;

    if (
      acknowledgedCommandId.length() > 0 &&
      acknowledgementPending &&
      acknowledgementCommandId ==
        acknowledgedCommandId
    ) {
      acknowledgementPending = false;
      acknowledgementCommandId = "";
      acknowledgementStatus = "";
      acknowledgementMessage = "";
      acknowledgementSetpoint = 0.0f;
    }
    /*
      O intervalo e contado desde o inicio da tentativa.
      Assim, a duracao do HTTPS nao e somada ao periodo
      configurado. Se a requisicao exceder o intervalo,
      a proxima telemetria fica liberada imediatamente.
    */
    nextAttemptMillis =
      static_cast<long>(
        completedAt - scheduledAt
      ) >= 0
        ? completedAt
        : scheduledAt;
  } else {
    consecutiveFailures++;
    lastError = error;
    status = resultStatus;
    nextAttemptMillis =
      millis() +
      retryDelayMilliseconds(
        consecutiveFailures
      );
  }

  unlockState();
}

bool CloudModule::createWorker() {
  telemetryQueue =
    xQueueCreate(
      1,
      sizeof(TelemetryJob)
    );

  if (telemetryQueue == nullptr) {
    return false;
  }

  BaseType_t taskCreated =
    xTaskCreate(
      workerTaskEntry,
      "mw-cloud",
      WORKER_STACK_SIZE_BYTES,
      this,
      1,
      &workerTask
    );

  return taskCreated == pdPASS;
}

bool CloudModule::enqueueTelemetry() {
  String payload =
    buildTelemetryPayload();

  if (
    payload.length() == 0 ||
    payload.length() >
      MAXIMUM_PAYLOAD_LENGTH
  ) {
    return false;
  }

  TelemetryJob job = {};

  lockState();

  String url = telemetryUrl;
  String token = deviceToken;
  String acknowledgedCommandId =
    acknowledgementPending
      ? acknowledgementCommandId
      : "";

  unlockState();

  strlcpy(
    job.url,
    url.c_str(),
    sizeof(job.url)
  );

  strlcpy(
    job.token,
    token.c_str(),
    sizeof(job.token)
  );

  strlcpy(
    job.payload,
    payload.c_str(),
    sizeof(job.payload)
  );

  strlcpy(
    job.acknowledgedCommandId,
    acknowledgedCommandId.c_str(),
    sizeof(job.acknowledgedCommandId)
  );

  job.sentEpoch =
    static_cast<uint32_t>(
      clock.getEpoch()
    );

  lockState();

  if (requestInProgress) {
    unlockState();
    return false;
  }

  requestInProgress = true;
  lastAttemptStartedMillis = millis();
  status = Status::SENDING;
  lastError = "";

  unlockState();

  if (
    xQueueSend(
      telemetryQueue,
      &job,
      0
    ) != pdTRUE
  ) {
    lockState();
    requestInProgress = false;
    unlockState();

    return false;
  }

  return true;
}

void CloudModule::applyPendingCommand() {
  lockState();

  if (
    !pendingCommandAvailable ||
    acknowledgementPending ||
    !enabled
  ) {
    unlockState();
    return;
  }

  String commandId =
    pendingCommandId;
  String commandType =
    pendingCommandType;
  String profileName =
    pendingProfileName;
  String stagePlan =
    pendingStagePlan;
  float requestedSetpoint =
    pendingCommandSetpoint;
  ConfigurationCommand requestedConfiguration =
    pendingConfiguration;
  uint32_t expiresAt =
    pendingCommandExpiresAt;

  pendingCommandAvailable = false;
  pendingCommandId = "";
  pendingCommandType = "";
  pendingProfileName = "";
  pendingStagePlan = "";
  pendingCommandSetpoint = 0.0f;
  pendingConfiguration =
    ConfigurationCommand();
  pendingCommandExpiresAt = 0;

  unlockState();

  String resultStatus = "rejected";
  String resultMessage;

  uint32_t currentEpoch =
    static_cast<uint32_t>(
      clock.getEpoch()
    );

  if (
    currentEpoch == 0 ||
    currentEpoch >= expiresAt
  ) {
    resultMessage =
      "Comando expirado antes da aplicacao.";
  } else if (
    commandType == "set_setpoint"
  ) {
    if (profile.isActive()) {
      resultMessage =
        "Perfil ativo impede alteracao manual.";
    } else if (
      requestedSetpoint < -10.0f ||
      requestedSetpoint > 40.0f ||
      !isfinite(requestedSetpoint)
    ) {
      resultMessage =
        "Setpoint remoto fora dos limites.";
    } else if (
      !settings.saveSetpoint(
        requestedSetpoint
      )
    ) {
      resultMessage =
        "Falha ao persistir o setpoint remoto.";
    } else {
      control.setSetpoint(
        requestedSetpoint
      );

      resultStatus = "applied";
      resultMessage =
        "Setpoint remoto aplicado.";

      eventLog.add(
        EventLogModule::Category::CONTROL,
        "Setpoint remoto aplicado: " +
        String(requestedSetpoint, 1) +
        " C"
      );
    }
  } else if (
    commandType == "set_configuration"
  ) {
    if (profile.isActive()) {
      resultMessage =
        "Perfil ativo impede alterar configuracoes.";
    } else if (
      !isValidConfigurationCommand(
        requestedConfiguration
      )
    ) {
      resultMessage =
        "Configuracao cloud fora dos limites.";
    } else {
      float oldHysteresis =
        control.getHysteresis();
      uint32_t oldCompressorProtection =
        control
          .getCompressorProtectionSeconds();
      float oldRefrigeratorOffset =
        temperatures
          .getRefrigeratorOffset();
      float oldThermalWellOffset =
        temperatures
          .getThermalWellOffset();
      AlarmModule::Configuration
        oldAlarmConfiguration =
          alarms.getConfiguration();

      AlarmModule::Configuration
        newAlarmConfiguration;
      newAlarmConfiguration.sensorAlarmEnabled =
        requestedConfiguration.sensorAlarmEnabled;
      newAlarmConfiguration.highTemperatureEnabled =
        requestedConfiguration.highTemperatureEnabled;
      newAlarmConfiguration.lowTemperatureEnabled =
        requestedConfiguration.lowTemperatureEnabled;
      newAlarmConfiguration.responseAlarmEnabled =
        requestedConfiguration.responseAlarmEnabled;
      newAlarmConfiguration.highTemperatureLimit =
        requestedConfiguration.highTemperatureLimit;
      newAlarmConfiguration.lowTemperatureLimit =
        requestedConfiguration.lowTemperatureLimit;
      newAlarmConfiguration.minimumExpectedChange =
        requestedConfiguration.minimumExpectedChange;
      newAlarmConfiguration.responseTimeoutSeconds =
        requestedConfiguration.responseTimeoutSeconds;

      bool saved =
        settings.saveHysteresis(
          requestedConfiguration.hysteresis
        );

      if (saved) {
        saved =
          settings
            .saveCompressorProtectionSeconds(
              requestedConfiguration
                .compressorProtectionSeconds
            );
      }

      if (saved) {
        saved =
          temperatures.saveCalibration(
            requestedConfiguration
              .refrigeratorOffset,
            requestedConfiguration
              .thermalWellOffset
          );
      }

      if (saved) {
        saved =
          alarms.saveConfiguration(
            newAlarmConfiguration
          );
      }

      if (!saved) {
        settings.saveHysteresis(
          oldHysteresis
        );
        settings
          .saveCompressorProtectionSeconds(
            oldCompressorProtection
          );
        temperatures.saveCalibration(
          oldRefrigeratorOffset,
          oldThermalWellOffset
        );
        alarms.saveConfiguration(
          oldAlarmConfiguration
        );

        resultMessage =
          "Falha ao persistir configuracao cloud.";
      } else {
        control.setHysteresis(
          requestedConfiguration.hysteresis
        );
        control
          .setCompressorProtectionSeconds(
            requestedConfiguration
              .compressorProtectionSeconds
          );

        resultStatus = "applied";
        resultMessage =
          "Configuracao cloud aplicada.";

        eventLog.add(
          EventLogModule::Category::CONTROL,
          "Configuracao atualizada pela nuvem"
        );
      }
    }
  } else if (
    commandType == "start_profile"
  ) {
    ProfileModule::Stage stages[
      ProfileModule::MAX_STAGES
    ];
    uint8_t stageCount = 0;

    if (
      profile.isActive() &&
      profile.getProfileName() ==
        profileName
    ) {
      resultStatus = "applied";
      resultMessage =
        "Perfil cloud ja estava em execucao.";
    } else if (profile.isActive()) {
      resultMessage =
        "Ja existe um perfil ativo.";
    } else if (!clock.isSynchronized()) {
      resultMessage =
        "Relogio indisponivel para iniciar o perfil.";
    } else if (
      !parseStagePlan(
        stagePlan,
        stages,
        stageCount
      )
    ) {
      resultMessage =
        "Etapas da receita cloud invalidas.";
    } else if (
      !profile.saveProfile(
        profileName,
        stages,
        stageCount
      )
    ) {
      resultMessage =
        "Falha ao gravar a receita cloud.";
    } else if (!profile.start()) {
      resultMessage =
        "Falha ao iniciar a receita cloud.";
    } else {
      resultStatus = "applied";
      resultMessage =
        "Perfil cloud iniciado.";

      eventLog.add(
        EventLogModule::Category::RECIPE,
        "Perfil cloud iniciado: " +
        profileName
      );
    }
  } else if (
    commandType == "pause_profile"
  ) {
    if (profile.isPaused()) {
      resultStatus = "applied";
      resultMessage =
        "Perfil cloud ja estava pausado.";
    } else if (!profile.isActive()) {
      resultMessage =
        "Perfil nao esta em execucao.";
    } else if (!profile.pause()) {
      resultMessage =
        "Falha ao pausar o perfil.";
    } else {
      resultStatus = "applied";
      resultMessage =
        "Perfil cloud pausado.";

      eventLog.add(
        EventLogModule::Category::RECIPE,
        "Perfil pausado pela nuvem"
      );
    }
  } else if (
    commandType == "resume_profile"
  ) {
    if (
      profile.isActive() &&
      !profile.isPaused()
    ) {
      resultStatus = "applied";
      resultMessage =
        "Perfil cloud ja estava em execucao.";
    } else if (!profile.isPaused()) {
      resultMessage =
        "Perfil nao esta pausado.";
    } else if (!profile.resume()) {
      resultMessage =
        "Falha ao retomar o perfil.";
    } else {
      resultStatus = "applied";
      resultMessage =
        "Perfil cloud retomado.";

      eventLog.add(
        EventLogModule::Category::RECIPE,
        "Perfil retomado pela nuvem"
      );
    }
  } else if (
    commandType == "stop_profile"
  ) {
    if (!profile.isActive()) {
      float manualSetpoint =
        settings.loadSetpoint(
          control.getSetpoint()
        );

      if (
        manualSetpoint >= -10.0f &&
        manualSetpoint <= 40.0f &&
        isfinite(manualSetpoint)
      ) {
        control.setSetpoint(
          manualSetpoint
        );
      }

      resultStatus = "applied";
      resultMessage =
        "Perfil cloud ja estava parado.";
    } else if (!profile.cancel()) {
      resultMessage =
        "Falha ao interromper o perfil.";
    } else {
      float manualSetpoint =
        settings.loadSetpoint(
          control.getSetpoint()
        );

      if (
        manualSetpoint >= -10.0f &&
        manualSetpoint <= 40.0f &&
        isfinite(manualSetpoint)
      ) {
        control.setSetpoint(
          manualSetpoint
        );
      }

      resultStatus = "applied";
      resultMessage =
        "Perfil cloud interrompido.";

      eventLog.add(
        EventLogModule::Category::RECIPE,
        "Perfil interrompido pela nuvem"
      );
    }
  } else if (
    commandType == "acknowledge_alarms"
  ) {
    alarms.acknowledgeAll();

    resultStatus = "applied";
    resultMessage =
      "Alarmes reconhecidos pela nuvem.";

    eventLog.add(
      EventLogModule::Category::ALARM,
      "Alarmes reconhecidos pela nuvem"
    );
  } else {
    resultMessage =
      "Tipo de comando cloud desconhecido.";
  }

  if (resultStatus == "rejected") {
    eventLog.add(
      EventLogModule::Category::CLOUD,
      "Comando cloud rejeitado: " +
      resultMessage
    );

    Serial.print(
      "Comando cloud rejeitado: "
    );
    Serial.println(resultMessage);
  }

  lockState();

  acknowledgementCommandId =
    commandId;
  acknowledgementStatus =
    resultStatus;
  acknowledgementMessage =
    resultMessage;
  acknowledgementSetpoint =
    control.getSetpoint();
  acknowledgementPending = true;
  nextAttemptMillis = millis();

  unlockState();
}

void CloudModule::handleCommandResponse(
  const String& response
) {
  int commandStart =
    response.indexOf(
      "\"command\":{"
    );

  if (commandStart < 0) {
    return;
  }

  String commandJson =
    response.substring(commandStart);

  String commandId;
  String commandType;
  String profileName;
  String stagePlan;
  float requestedSetpoint = 0.0f;
  ConfigurationCommand
    requestedConfiguration;
  uint32_t expiresAt = 0;
  uint32_t declaredStageCount = 0;
  ProfileModule::Stage
    validationStages[
      ProfileModule::MAX_STAGES
    ];
  uint8_t parsedStageCount = 0;

  if (
    !extractJsonString(
      commandJson,
      "id",
      commandId
    ) ||
    !extractJsonString(
      commandJson,
      "type",
      commandType
    ) ||
    !extractJsonUnsigned(
      commandJson,
      "expiresAt",
      expiresAt
    ) ||
    !isValidCommandId(commandId)
  ) {
    Serial.println(
      "Resposta cloud contem comando invalido."
    );
    return;
  }

  bool validCommand = false;

  if (commandType == "set_setpoint") {
    validCommand =
      extractJsonFloat(
        commandJson,
        "setpoint",
        requestedSetpoint
      ) &&
      requestedSetpoint >= -10.0f &&
      requestedSetpoint <= 40.0f &&
      isfinite(requestedSetpoint);
  } else if (
    commandType == "start_profile"
  ) {
    validCommand =
      extractJsonString(
        commandJson,
        "profileName",
        profileName
      ) &&
      extractJsonString(
        commandJson,
        "stagePlan",
        stagePlan
      ) &&
      extractJsonUnsigned(
        commandJson,
        "stageCount",
        declaredStageCount
      ) &&
      profileName.length() > 0 &&
      profileName.length() <= 31 &&
      stagePlan.length() > 0 &&
      stagePlan.length() <=
        MAXIMUM_STAGE_PLAN_LENGTH &&
      declaredStageCount > 0 &&
      declaredStageCount <=
        ProfileModule::MAX_STAGES &&
      parseStagePlan(
        stagePlan,
        validationStages,
        parsedStageCount
      ) &&
      parsedStageCount ==
        declaredStageCount;
  } else if (
    commandType == "set_configuration"
  ) {
    validCommand =
      extractJsonFloat(
        commandJson,
        "hysteresis",
        requestedConfiguration.hysteresis
      ) &&
      extractJsonUnsigned(
        commandJson,
        "compressorProtectionSeconds",
        requestedConfiguration
          .compressorProtectionSeconds
      ) &&
      extractJsonFloat(
        commandJson,
        "refrigeratorOffset",
        requestedConfiguration
          .refrigeratorOffset
      ) &&
      extractJsonFloat(
        commandJson,
        "thermalWellOffset",
        requestedConfiguration
          .thermalWellOffset
      ) &&
      extractJsonBoolean(
        commandJson,
        "sensorAlarmEnabled",
        requestedConfiguration
          .sensorAlarmEnabled
      ) &&
      extractJsonBoolean(
        commandJson,
        "highTemperatureEnabled",
        requestedConfiguration
          .highTemperatureEnabled
      ) &&
      extractJsonBoolean(
        commandJson,
        "lowTemperatureEnabled",
        requestedConfiguration
          .lowTemperatureEnabled
      ) &&
      extractJsonBoolean(
        commandJson,
        "responseAlarmEnabled",
        requestedConfiguration
          .responseAlarmEnabled
      ) &&
      extractJsonFloat(
        commandJson,
        "highTemperatureLimit",
        requestedConfiguration
          .highTemperatureLimit
      ) &&
      extractJsonFloat(
        commandJson,
        "lowTemperatureLimit",
        requestedConfiguration
          .lowTemperatureLimit
      ) &&
      extractJsonFloat(
        commandJson,
        "minimumExpectedChange",
        requestedConfiguration
          .minimumExpectedChange
      ) &&
      extractJsonUnsigned(
        commandJson,
        "responseTimeoutSeconds",
        requestedConfiguration
          .responseTimeoutSeconds
      ) &&
      isValidConfigurationCommand(
        requestedConfiguration
      );
  } else {
    validCommand =
      commandType == "pause_profile" ||
      commandType == "resume_profile" ||
      commandType == "stop_profile" ||
      commandType == "acknowledge_alarms";
  }

  if (!validCommand) {
    Serial.println(
      "Resposta cloud contem comando invalido."
    );
    return;
  }

  lockState();

  bool alreadyAcknowledged =
    acknowledgementPending &&
    acknowledgementCommandId ==
      commandId;

  if (!alreadyAcknowledged) {
    pendingCommandId = commandId;
    pendingCommandType = commandType;
    pendingProfileName = profileName;
    pendingStagePlan = stagePlan;
    pendingCommandSetpoint =
      requestedSetpoint;
    pendingConfiguration =
      requestedConfiguration;
    pendingCommandExpiresAt = expiresAt;
    pendingCommandAvailable = true;
  }

  unlockState();
}

String CloudModule::buildDeviceId() const {
  uint64_t chipId =
    ESP.getEfuseMac();

  char buffer[16];

  snprintf(
    buffer,
    sizeof(buffer),
    "MW-%04" PRIX16 "%08" PRIX32,
    static_cast<uint16_t>(
      chipId >> 32
    ),
    static_cast<uint32_t>(chipId)
  );

  return String(buffer);
}

String CloudModule::generateDeviceToken() const {
  uint8_t randomBytes[32];

  esp_fill_random(
    randomBytes,
    sizeof(randomBytes)
  );

  static const char HEX_DIGITS[] =
    "0123456789abcdef";

  char token[
    TOKEN_HEX_LENGTH + 1
  ];

  for (
    size_t index = 0;
    index < sizeof(randomBytes);
    index++
  ) {
    token[index * 2] =
      HEX_DIGITS[randomBytes[index] >> 4];

    token[index * 2 + 1] =
      HEX_DIGITS[randomBytes[index] & 0x0F];
  }

  token[TOKEN_HEX_LENGTH] = '\0';

  return String(token);
}

String CloudModule::generateBootId() const {
  char buffer[9];

  snprintf(
    buffer,
    sizeof(buffer),
    "%08X",
    static_cast<unsigned int>(
      esp_random()
    )
  );

  return String(buffer);
}

String CloudModule::buildTelemetryPayload() {
  bool refrigeratorConnected =
    temperatures
      .isRefrigeratorSensorConnected();

  bool thermalWellConnected =
    temperatures
      .isThermalWellSensorConnected();

  AlarmModule::Configuration
    alarmConfiguration =
      alarms.getConfiguration();

  telemetrySequence++;

  lockState();

  bool includeCommandResult =
    acknowledgementPending;
  String resultCommandId =
    acknowledgementCommandId;
  String resultStatus =
    acknowledgementStatus;
  String resultMessage =
    acknowledgementMessage;
  float resultSetpoint =
    acknowledgementSetpoint;

  unlockState();

  String json;
  json.reserve(2000);

  json += "{";
  json += "\"schemaVersion\":1";

  json += ",\"deviceId\":\"";
  json += jsonEscape(deviceId);
  json += "\"";

  json += ",\"bootId\":\"";
  json += bootId;
  json += "\"";

  json += ",\"sequence\":";
  json += String(telemetrySequence);

  json += ",\"sentAt\":";
  json += String(
    static_cast<uint32_t>(
      clock.getEpoch()
    )
  );

  json += ",\"uptimeSeconds\":";
  json += String(millis() / 1000UL);

  json += ",\"firmware\":{";
  json += "\"product\":\"";
  json += FirmwareInfo::PRODUCT;
  json += "\",\"version\":\"";
  json += FirmwareInfo::VERSION;
  json += "\",\"phase\":\"";
  json += FirmwareInfo::BUILD_PHASE;
  json += "\"}";

  json += ",\"network\":{";
  json += "\"rssi\":";
  json += String(
    network.getStationRssi()
  );
  json += "}";

  json += ",\"temperatures\":{";

  json += "\"refrigerator\":{";
  json += "\"connected\":";
  json += refrigeratorConnected
    ? "true"
    : "false";
  json += ",\"value\":";

  if (refrigeratorConnected) {
    json += String(
      temperatures
        .getRefrigeratorTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"raw\":";

  if (refrigeratorConnected) {
    json += String(
      temperatures
        .getRefrigeratorRawTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"offset\":";
  json += String(
    temperatures
      .getRefrigeratorOffset(),
    2
  );
  json += "}";

  json += ",\"thermalWell\":{";
  json += "\"connected\":";
  json += thermalWellConnected
    ? "true"
    : "false";
  json += ",\"value\":";

  if (thermalWellConnected) {
    json += String(
      temperatures
        .getThermalWellTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"raw\":";

  if (thermalWellConnected) {
    json += String(
      temperatures
        .getThermalWellRawTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"offset\":";
  json += String(
    temperatures
      .getThermalWellOffset(),
    2
  );
  json += "}";
  json += "}";

  json += ",\"control\":{";
  json += "\"setpoint\":";
  json += String(
    control.getSetpoint(),
    1
  );
  json += ",\"hysteresis\":";
  json += String(
    control.getHysteresis(),
    1
  );
  json += ",\"state\":\"";
  json += jsonEscape(
    String(control.getStateText())
  );
  json += "\",\"cooling\":";
  json += relays.isCoolingOn()
    ? "true"
    : "false";
  json += ",\"heating\":";
  json += relays.isHeatingOn()
    ? "true"
    : "false";
  json += ",\"compressorProtectionSeconds\":";
  json += String(
    control
      .getCoolingDelayRemainingSeconds()
  );
  json += ",\"compressorProtectionDurationSeconds\":";
  json += String(
    control
      .getCompressorProtectionSeconds()
  );
  json += "}";

  json += ",\"profile\":{";
  json += "\"active\":";
  json += profile.isActive()
    ? "true"
    : "false";
  json += ",\"paused\":";
  json += profile.isPaused()
    ? "true"
    : "false";
  json += ",\"name\":\"";
  json += jsonEscape(
    profile.getProfileName()
  );
  json += "\",\"state\":\"";
  json += jsonEscape(
    String(
      profile.getRunStateText()
    )
  );
  json += "\",\"stage\":";
  json += String(
    profile.getCurrentStageIndex()
  );
  json += ",\"stageCount\":";
  json += String(
    profile.getStageCount()
  );
  json += ",\"remainingSeconds\":";
  json += String(
    profile.getTotalRemainingSeconds()
  );
  json += "}";

  json += ",\"alarms\":{";
  json += "\"active\":";
  json += alarms.hasActiveAlarm()
    ? "true"
    : "false";
  json += ",\"unacknowledged\":";
  json += alarms.hasUnacknowledgedAlarm()
    ? "true"
    : "false";
  json += ",\"count\":";
  json += String(
    alarms.getActiveAlarmCount()
  );
  json += ",\"summary\":\"";
  json += jsonEscape(
    alarms.getSummaryText()
  );
  json += "\"";
  json += ",\"configuration\":{";
  json += "\"sensorAlarmEnabled\":";
  json += alarmConfiguration.sensorAlarmEnabled
    ? "true"
    : "false";
  json += ",\"highTemperatureEnabled\":";
  json += alarmConfiguration.highTemperatureEnabled
    ? "true"
    : "false";
  json += ",\"lowTemperatureEnabled\":";
  json += alarmConfiguration.lowTemperatureEnabled
    ? "true"
    : "false";
  json += ",\"responseAlarmEnabled\":";
  json += alarmConfiguration.responseAlarmEnabled
    ? "true"
    : "false";
  json += ",\"highTemperatureLimit\":";
  json += String(
    alarmConfiguration.highTemperatureLimit,
    1
  );
  json += ",\"lowTemperatureLimit\":";
  json += String(
    alarmConfiguration.lowTemperatureLimit,
    1
  );
  json += ",\"minimumExpectedChange\":";
  json += String(
    alarmConfiguration.minimumExpectedChange,
    1
  );
  json += ",\"responseTimeoutSeconds\":";
  json += String(
    alarmConfiguration.responseTimeoutSeconds
  );
  json += "}";
  json += "}";

  if (includeCommandResult) {
    json += ",\"commandResult\":{";
    json += "\"id\":\"";
    json += jsonEscape(resultCommandId);
    json += "\",\"status\":\"";
    json += jsonEscape(resultStatus);
    json += "\",\"appliedSetpoint\":";
    json += String(resultSetpoint, 1);
    json += ",\"message\":\"";
    json += jsonEscape(resultMessage);
    json += "\"}";
  }

  json += "}";

  return json;
}

String CloudModule::normalizeTelemetryUrl(
  const String& candidate
) const {
  String normalized = candidate;
  normalized.trim();

  while (
    normalized.length() > 0 &&
    normalized.endsWith("/")
  ) {
    normalized.remove(
      normalized.length() - 1
    );
  }

  if (
    normalized.length() == 0
  ) {
    return "";
  }

  if (
    normalized.length() >
      MAXIMUM_URL_LENGTH ||
    !normalized.startsWith(
      "https://"
    ) ||
    normalized.indexOf(' ') >= 0
  ) {
    return "";
  }

  int hostStart = 8;
  int firstPathSeparator =
    normalized.indexOf(
      '/',
      hostStart
    );

  int hostEnd =
    firstPathSeparator >= 0
      ? firstPathSeparator
      : normalized.length();

  if (
    hostEnd <= hostStart ||
    hostEnd - hostStart > 253
  ) {
    return "";
  }

  return normalized;
}

String CloudModule::jsonEscape(
  const String& text
) const {
  String escaped;
  escaped.reserve(
    text.length() + 8
  );

  for (
    size_t index = 0;
    index < text.length();
    index++
  ) {
    char character = text[index];

    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;

      case '"':
        escaped += "\\\"";
        break;

      case '\n':
        escaped += "\\n";
        break;

      case '\r':
        escaped += "\\r";
        break;

      case '\t':
        escaped += "\\t";
        break;

      default:
        if (
          static_cast<uint8_t>(
            character
          ) >= 0x20
        ) {
          escaped += character;
        }
        break;
    }
  }

  return escaped;
}

bool CloudModule::isValidToken(
  const String& token
) const {
  if (
    token.length() !=
      TOKEN_HEX_LENGTH
  ) {
    return false;
  }

  for (
    size_t index = 0;
    index < token.length();
    index++
  ) {
    char character = token[index];

    bool hexadecimal =
      (
        character >= '0' &&
        character <= '9'
      ) ||
      (
        character >= 'a' &&
        character <= 'f'
      ) ||
      (
        character >= 'A' &&
        character <= 'F'
      );

    if (!hexadecimal) {
      return false;
    }
  }

  return true;
}

bool CloudModule::isValidCommandId(
  const String& commandId
) const {
  if (
    commandId.length() !=
      COMMAND_ID_LENGTH ||
    !commandId.startsWith("cmd_")
  ) {
    return false;
  }

  for (
    size_t index = 4;
    index < commandId.length();
    index++
  ) {
    char character = commandId[index];

    bool valid =
      (
        character >= '0' &&
        character <= '9'
      ) ||
      (
        character >= 'a' &&
        character <= 'f'
      );

    if (!valid) {
      return false;
    }
  }

  return true;
}

bool CloudModule::extractJsonString(
  const String& json,
  const String& key,
  String& value
) const {
  String marker =
    "\"" + key + "\":\"";

  int start = json.indexOf(marker);
  if (start < 0) {
    return false;
  }

  start += marker.length();
  int end = json.indexOf('"', start);
  if (end < start) {
    return false;
  }

  value = json.substring(start, end);
  return true;
}

bool CloudModule::extractJsonFloat(
  const String& json,
  const String& key,
  float& value
) const {
  String marker =
    "\"" + key + "\":";

  int start = json.indexOf(marker);
  if (start < 0) {
    return false;
  }

  start += marker.length();

  while (
    start < json.length() &&
    json[start] == ' '
  ) {
    start++;
  }

  const char* numberStart =
    json.c_str() + start;
  char* numberEnd = nullptr;
  float parsed =
    strtof(numberStart, &numberEnd);

  if (
    numberEnd == numberStart ||
    !isfinite(parsed)
  ) {
    return false;
  }

  value = parsed;
  return true;
}

bool CloudModule::extractJsonUnsigned(
  const String& json,
  const String& key,
  uint32_t& value
) const {
  String marker =
    "\"" + key + "\":";

  int start = json.indexOf(marker);
  if (start < 0) {
    return false;
  }

  start += marker.length();

  while (
    start < json.length() &&
    json[start] == ' '
  ) {
    start++;
  }

  const char* numberStart =
    json.c_str() + start;
  char* numberEnd = nullptr;
  unsigned long parsed =
    strtoul(numberStart, &numberEnd, 10);

  if (numberEnd == numberStart) {
    return false;
  }

  value = static_cast<uint32_t>(parsed);
  return true;
}

bool CloudModule::extractJsonBoolean(
  const String& json,
  const String& key,
  bool& value
) const {
  String marker =
    "\"" + key + "\":";

  int start = json.indexOf(marker);
  if (start < 0) {
    return false;
  }

  start += marker.length();

  while (
    start < json.length() &&
    json[start] == ' '
  ) {
    start++;
  }

  if (json.startsWith("true", start)) {
    value = true;
    return true;
  }

  if (json.startsWith("false", start)) {
    value = false;
    return true;
  }

  return false;
}

bool CloudModule::
isValidConfigurationCommand(
  const ConfigurationCommand& candidate
) const {
  return
    isfinite(candidate.hysteresis) &&
    candidate.hysteresis >= 0.1f &&
    candidate.hysteresis <= 5.0f &&
    candidate.compressorProtectionSeconds >=
      60UL &&
    candidate.compressorProtectionSeconds <=
      900UL &&
    isfinite(candidate.refrigeratorOffset) &&
    candidate.refrigeratorOffset >= -10.0f &&
    candidate.refrigeratorOffset <= 10.0f &&
    isfinite(candidate.thermalWellOffset) &&
    candidate.thermalWellOffset >= -10.0f &&
    candidate.thermalWellOffset <= 10.0f &&
    isfinite(candidate.highTemperatureLimit) &&
    isfinite(candidate.lowTemperatureLimit) &&
    candidate.highTemperatureLimit >
      candidate.lowTemperatureLimit &&
    candidate.highTemperatureLimit <= 60.0f &&
    candidate.lowTemperatureLimit >= -30.0f &&
    isfinite(candidate.minimumExpectedChange) &&
    candidate.minimumExpectedChange >= 0.1f &&
    candidate.minimumExpectedChange <= 10.0f &&
    candidate.responseTimeoutSeconds >= 60UL &&
    candidate.responseTimeoutSeconds <=
      24UL * 60UL * 60UL &&
    candidate.responseTimeoutSeconds % 60UL == 0;
}

bool CloudModule::parseStagePlan(
  const String& stagePlan,
  ProfileModule::Stage* stages,
  uint8_t& stageCount
) const {
  stageCount = 0;

  if (
    stages == nullptr ||
    stagePlan.length() == 0 ||
    stagePlan.length() >
      MAXIMUM_STAGE_PLAN_LENGTH
  ) {
    return false;
  }

  int segmentStart = 0;

  while (
    segmentStart <
    stagePlan.length()
  ) {
    if (
      stageCount >=
      ProfileModule::MAX_STAGES
    ) {
      return false;
    }

    int segmentEnd =
      stagePlan.indexOf(
        ';',
        segmentStart
      );

    if (segmentEnd < 0) {
      segmentEnd =
        stagePlan.length();
    }

    String segment =
      stagePlan.substring(
        segmentStart,
        segmentEnd
      );

    int separator =
      segment.indexOf(',');

    if (
      separator <= 0 ||
      separator >=
        segment.length() - 1 ||
      segment.indexOf(
        ',',
        separator + 1
      ) >= 0
    ) {
      return false;
    }

    String temperatureText =
      segment.substring(
        0,
        separator
      );

    String durationText =
      segment.substring(
        separator + 1
      );

    if (
      durationText.startsWith("-")
    ) {
      return false;
    }

    char* temperatureEnd = nullptr;
    float temperature =
      strtof(
        temperatureText.c_str(),
        &temperatureEnd
      );

    char* durationEnd = nullptr;
    unsigned long duration =
      strtoul(
        durationText.c_str(),
        &durationEnd,
        10
      );

    if (
      temperatureEnd ==
        temperatureText.c_str() ||
      *temperatureEnd != '\0' ||
      durationEnd ==
        durationText.c_str() ||
      *durationEnd != '\0' ||
      !isfinite(temperature) ||
      temperature < -10.0f ||
      temperature > 40.0f ||
      duration < 60UL ||
      duration >
        90UL * 24UL * 60UL * 60UL
    ) {
      return false;
    }

    stages[stageCount]
      .targetTemperature =
        temperature;

    stages[stageCount]
      .durationSeconds =
        static_cast<uint32_t>(
          duration
        );

    stageCount++;

    if (
      segmentEnd >=
      stagePlan.length()
    ) {
      break;
    }

    segmentStart =
      segmentEnd + 1;

    if (
      segmentStart >=
      stagePlan.length()
    ) {
      return false;
    }
  }

  return stageCount > 0;
}

bool CloudModule::
configurationReadyUnsafe() const {
  return telemetryUrl.length() > 0 &&
    isValidToken(deviceToken);
}

unsigned long
CloudModule::retryDelayMilliseconds(
  uint32_t failureCount
) const {
  uint32_t exponent =
    failureCount > 6
      ? 5
      : failureCount - 1;

  uint32_t seconds =
    15UL << exponent;

  return min(
    seconds,
    300UL
  ) * 1000UL;
}

void CloudModule::lockState() const {
  if (stateMutex != nullptr) {
    xSemaphoreTake(
      stateMutex,
      portMAX_DELAY
    );
  }
}

void CloudModule::unlockState() const {
  if (stateMutex != nullptr) {
    xSemaphoreGive(stateMutex);
  }
}

void CloudModule::setStatus(
  Status newStatus
) {
  lockState();

  if (!requestInProgress) {
    status = newStatus;
  }

  unlockState();
}
