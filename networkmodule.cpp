#include "networkmodule.h"

NetworkModule::NetworkModule() :
  accessPointName(""),
  stationSsid(""),
  stationPassword(""),
  accessPointStarted(false),
  captivePortalStarted(false),
  connectionPreviouslyReported(false),
  configurationCompletionPending(false),
  configurationShutdownScheduled(false),
  lastConnectionAttempt(0),
  stationDisconnectedSince(0),
  configurationShutdownScheduledAt(0) {
}

bool NetworkModule::begin(
  const char* accessPointName,
  const String& savedStationSsid,
  const String& savedStationPassword
) {
  this->accessPointName =
    accessPointName;

  stationSsid =
    savedStationSsid;

  stationPassword =
    savedStationPassword;

  stationSsid.trim();

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  if (hasStationCredentials()) {
    WiFi.mode(WIFI_STA);

    stationDisconnectedSince =
      millis();

    startStationConnection();

    Serial.println(
      "Rede de configuracao permanecera desligada se o Wi-Fi conectar."
    );
  } else {
    Serial.println(
      "Wi-Fi domestico ainda nao configurado."
    );

    return startConfigurationAccessPoint();
  }

  return true;
}

void NetworkModule::update() {
  if (captivePortalStarted) {
    dnsServer.processNextRequest();
  }

  if (!hasStationCredentials()) {
    if (!accessPointStarted) {
      startConfigurationAccessPoint();
    }

    return;
  }

  bool connected =
    isStationConnected();

  if (
    connected &&
    !connectionPreviouslyReported
  ) {
    connectionPreviouslyReported =
      true;

    Serial.println();
    Serial.println(
      "Wi-Fi domestico conectado."
    );

    Serial.print(
      "Rede: "
    );

    Serial.println(
      WiFi.SSID()
    );

    Serial.print(
      "IP local: "
    );

    Serial.println(
      WiFi.localIP()
    );

    stationDisconnectedSince = 0;

    if (
      accessPointStarted &&
      !configurationCompletionPending
    ) {
      stopConfigurationAccessPoint();
    } else if (
      accessPointStarted &&
      configurationCompletionPending
    ) {
      Serial.println(
        "Configuracao concluida. Aguardando confirmacao do usuario para abrir o Maltworks Cloud."
      );
    }
  }

  if (
    connected &&
    accessPointStarted &&
    configurationShutdownScheduled &&
    millis() - configurationShutdownScheduledAt >=
      CONFIGURATION_SHUTDOWN_DELAY_MS
  ) {
    stopConfigurationAccessPoint();
  }

  if (
    !connected &&
    connectionPreviouslyReported
  ) {
    connectionPreviouslyReported =
      false;

    Serial.println();
    Serial.println(
      "Wi-Fi domestico desconectado."
    );

    stationDisconnectedSince =
      millis();
  }

  if (
    !connected &&
    stationDisconnectedSince == 0
  ) {
    stationDisconnectedSince =
      millis();
  }

  if (
    !connected &&
    !accessPointStarted &&
    millis() - stationDisconnectedSince >=
      CONFIGURATION_FALLBACK_DELAY_MS
  ) {
    Serial.println(
      "Wi-Fi indisponivel. Reabrindo a configuracao local."
    );

    startConfigurationAccessPoint();
  }

  if (
    connected ||
    millis() - lastConnectionAttempt <
      RECONNECT_INTERVAL_MS
  ) {
    return;
  }

  startStationConnection();
}

bool NetworkModule::connectStation(
  const String& ssid,
  const String& password
) {
  String normalizedSsid =
    ssid;

  normalizedSsid.trim();

  if (
    normalizedSsid.length() == 0 ||
    normalizedSsid.length() > 32
  ) {
    return false;
  }

  stationSsid =
    normalizedSsid;

  stationPassword =
    password;

  connectionPreviouslyReported =
    false;

  if (accessPointStarted) {
    configurationCompletionPending =
      true;

    configurationShutdownScheduled =
      false;

    configurationShutdownScheduledAt =
      0;
  }

  stationDisconnectedSince =
    millis();

  startStationConnection();

  return true;
}

void NetworkModule::disconnectStation() {
  stationSsid = "";
  stationPassword = "";

  WiFi.disconnect(
    false,
    true
  );

  connectionPreviouslyReported =
    false;

  configurationCompletionPending =
    false;

  configurationShutdownScheduled =
    false;

  configurationShutdownScheduledAt =
    0;

  stationDisconnectedSince = 0;

  startConfigurationAccessPoint();

  Serial.println(
    "Credenciais do Wi-Fi domestico removidas."
  );
}

bool NetworkModule::completeConfiguration() {
  if (
    !accessPointStarted ||
    !configurationCompletionPending ||
    !isStationConnected()
  ) {
    return false;
  }

  configurationShutdownScheduled =
    true;

  configurationShutdownScheduledAt =
    millis();

  Serial.println(
    "Transferencia para o Maltworks Cloud solicitada."
  );

  return true;
}

bool NetworkModule::
isAccessPointStarted() const {
  return accessPointStarted;
}

bool NetworkModule::
isConfigurationCompletionPending() const {
  return configurationCompletionPending;
}

bool NetworkModule::
hasStationCredentials() const {
  return stationSsid.length() > 0;
}

bool NetworkModule::
isStationConnected() const {
  return WiFi.status() ==
    WL_CONNECTED;
}

String NetworkModule::
getStationStatusText() const {
  if (!hasStationCredentials()) {
    return "NAO CONFIGURADO";
  }

  switch (WiFi.status()) {
    case WL_CONNECTED:
      return "CONECTADO";

    case WL_NO_SSID_AVAIL:
      return "REDE NAO ENCONTRADA";

    case WL_CONNECT_FAILED:
      return "FALHA DE AUTENTICACAO";

    case WL_CONNECTION_LOST:
      return "CONEXAO PERDIDA";

    case WL_DISCONNECTED:
      return "CONECTANDO";

    case WL_IDLE_STATUS:
      return "CONECTANDO";

    default:
      return "CONECTANDO";
  }
}

String NetworkModule::
getStationSsid() const {
  return stationSsid;
}

IPAddress NetworkModule::
getAccessPointIp() const {
  return WiFi.softAPIP();
}

IPAddress NetworkModule::
getStationIp() const {
  if (!isStationConnected()) {
    return IPAddress(
      0,
      0,
      0,
      0
    );
  }

  return WiFi.localIP();
}

int32_t NetworkModule::
getStationRssi() const {
  if (!isStationConnected()) {
    return 0;
  }

  return WiFi.RSSI();
}

uint8_t NetworkModule::
getConnectedAccessPointClients() const {
  if (!accessPointStarted) {
    return 0;
  }

  return WiFi.softAPgetStationNum();
}

bool NetworkModule::
startConfigurationAccessPoint() {
  if (accessPointStarted) {
    return true;
  }

  WiFi.mode(WIFI_AP_STA);

  accessPointStarted =
    WiFi.softAP(
      accessPointName.c_str()
    );

  if (!accessPointStarted) {
    Serial.println(
      "Falha ao criar a rede de configuracao."
    );

    return false;
  }

  Serial.println(
    "Rede de configuracao aberta inicializada."
  );

  Serial.print(
    "IP da rede propria: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  dnsServer.setTTL(0);
  dnsServer.setErrorReplyCode(
    DNSReplyCode::NoError
  );

  captivePortalStarted =
    dnsServer.start(
      53,
      "*",
      WiFi.softAPIP()
    );

  Serial.println(
    captivePortalStarted
      ? "Portal cativo inicializado."
      : "Falha ao iniciar o portal cativo."
  );

  return true;
}

void NetworkModule::
stopConfigurationAccessPoint() {
  if (captivePortalStarted) {
    dnsServer.stop();
    captivePortalStarted = false;
  }

  if (accessPointStarted) {
    WiFi.softAPdisconnect(true);
    accessPointStarted = false;
  }

  WiFi.mode(WIFI_STA);

  configurationCompletionPending =
    false;

  configurationShutdownScheduled =
    false;

  configurationShutdownScheduledAt =
    0;

  Serial.println(
    "Rede de configuracao desativada. Abrindo caminho para o Maltworks Cloud."
  );
}

void NetworkModule::
startStationConnection() {
  if (!hasStationCredentials()) {
    return;
  }

  lastConnectionAttempt =
    millis();

  Serial.print(
    "Tentando conectar ao Wi-Fi: "
  );

  Serial.println(
    stationSsid
  );

  WiFi.disconnect(
    false,
    false
  );

  if (
    stationPassword.length() == 0
  ) {
    WiFi.begin(
      stationSsid.c_str()
    );
  } else {
    WiFi.begin(
      stationSsid.c_str(),
      stationPassword.c_str()
    );
  }
}
