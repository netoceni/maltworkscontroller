#include <math.h>
#include <Update.h>

#include "webmodule.h"
#include "webassets.h"

WebModule::WebModule(
  TemperatureModule& temperatureModule,
  RelayModule& relayModule,
  ControlModule& controlModule,
  HistoryModule& historyModule,
  NetworkModule& networkModule,
  SettingsModule& settingsModule,
  ClockModule& clockModule,
  ProfileModule& profileModule,
  RecipeLibraryModule& recipeLibraryModule,
  AlarmModule& alarmModule,
  EventLogModule& eventLogModule,
  CloudModule& cloudModule
) :
  server(80),
  temperatures(temperatureModule),
  relays(relayModule),
  control(controlModule),
  history(historyModule),
  network(networkModule),
  settings(settingsModule),
  clock(clockModule),
  profile(profileModule),
  recipeLibrary(recipeLibraryModule),
  alarms(alarmModule),
  eventLog(eventLogModule),
  cloud(cloudModule),
  initialized(false),
  firmwareUpdateInProgress(false),
  firmwareUpdateSucceeded(false),
  restartScheduled(false),
  firmwareUpdateError(""),
  restartScheduledAt(0),
  firmwareExpectedSize(0),
  firmwareReceivedSize(0) {
}

bool WebModule::begin() {
  configureRoutes();

  server.begin();

  initialized = true;

  return true;
}

void WebModule::configureRoutes() {
  server.on(
    "/",
    HTTP_GET,
    [this]() {
      handleRoot();
    }
  );

  server.on(
    "/save",
    HTTP_POST,
    [this]() {
      handleSave();
    }
  );

  server.on(
    "/wifi/save",
    HTTP_POST,
    [this]() {
      handleWifiSave();
    }
  );

  server.on(
    "/wifi/forget",
    HTTP_POST,
    [this]() {
      handleWifiForget();
    }
  );

  server.on(
    "/firmware/preflight",
    HTTP_GET,
    [this]() {
      handleFirmwarePreflight();
    }
  );

  server.on(
    "/firmware/update",
    HTTP_POST,
    [this]() {
      handleFirmwareUpdateEnd();
    },
    [this]() {
      handleFirmwareUpload();
    }
  );

  server.on(
    "/profile/save",
    HTTP_POST,
    [this]() {
      handleProfileSave();
    }
  );

  server.on(
    "/profile/start",
    HTTP_POST,
    [this]() {
      handleProfileStart();
    }
  );

  server.on(
    "/profile/pause",
    HTTP_POST,
    [this]() {
      handleProfilePause();
    }
  );

  server.on(
    "/profile/resume",
    HTTP_POST,
    [this]() {
      handleProfileResume();
    }
  );

  server.on(
    "/profile/cancel",
    HTTP_POST,
    [this]() {
      handleProfileCancel();
    }
  );

  server.on(
    "/recipe/save",
    HTTP_POST,
    [this]() {
      handleRecipeSave();
    }
  );

  server.on(
    "/recipe/load",
    HTTP_POST,
    [this]() {
      handleRecipeLoad();
    }
  );

  server.on(
    "/recipe/start",
    HTTP_POST,
    [this]() {
      handleRecipeStart();
    }
  );

  server.on(
    "/recipe/delete",
    HTTP_POST,
    [this]() {
      handleRecipeDelete();
    }
  );

  server.on(
    "/alarms/save",
    HTTP_POST,
    [this]() {
      handleAlarmSave();
    }
  );

  server.on(
    "/alarms/acknowledge",
    HTTP_POST,
    [this]() {
      handleAlarmAcknowledge();
    }
  );

  server.on(
    "/events",
    HTTP_GET,
    [this]() {
      handleEventLogApi();
    }
  );

  server.on(
    "/events/clear",
    HTTP_POST,
    [this]() {
      handleEventLogClear();
    }
  );

  server.on(
    "/calibration/save",
    HTTP_POST,
    [this]() {
      handleCalibrationSave();
    }
  );

  server.on(
    "/calibration/reset",
    HTTP_POST,
    [this]() {
      handleCalibrationReset();
    }
  );

  server.on(
    "/cloud/save",
    HTTP_POST,
    [this]() {
      handleCloudSave();
    }
  );

  server.on(
    "/cloud/sync",
    HTTP_POST,
    [this]() {
      handleCloudSync();
    }
  );

  server.on(
    "/cloud/token/regenerate",
    HTTP_POST,
    [this]() {
      handleCloudTokenRegenerate();
    }
  );

  server.on(
    "/api",
    HTTP_GET,
    [this]() {
      handleApi();
    }
  );

  server.on(
    "/history",
    HTTP_GET,
    [this]() {
      handleHistoryApi();
    }
  );

  server.onNotFound(
    [this]() {
      handleNotFound();
    }
  );
}

void WebModule::update() {
  if (!initialized) {
    return;
  }

  server.handleClient();

  if (
    restartScheduled &&
    millis() - restartScheduledAt >=
      RESTART_DELAY_MS
  ) {
    ESP.restart();
  }
}

bool WebModule::
isFirmwareUpdateInProgress() const {
  return firmwareUpdateInProgress;
}

IPAddress WebModule::getIpAddress() const {
  return network.getAccessPointIp();
}

void WebModule::handleRoot() {
  server.sendHeader(
    "Cache-Control",
    "no-store"
  );

  server.sendHeader(
    "Content-Encoding",
    "gzip"
  );

  server.sendHeader(
    "Vary",
    "Accept-Encoding"
  );

  server.send_P(
    200,
    "text/html; charset=utf-8",
    reinterpret_cast<const char*>(
      INDEX_HTML_GZ
    ),
    INDEX_HTML_GZ_LEN
  );
}

void WebModule::handleSave() {
  if (profile.isActive()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Cancele o perfil ativo antes de alterar manualmente o setpoint.\"}"
    );

    return;
  }

  if (
    !server.hasArg("setpoint") ||
    !server.hasArg("hysteresis")
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Parametros ausentes.\"}"
    );

    return;
  }

  float newSetpoint =
    server.arg("setpoint").toFloat();

  float newHysteresis =
    server.arg("hysteresis").toFloat();

  bool validSetpoint =
    newSetpoint >= -10.0f &&
    newSetpoint <= 40.0f;

  bool validHysteresis =
    newHysteresis >= 0.1f &&
    newHysteresis <= 5.0f;

  if (
    !validSetpoint ||
    !validHysteresis
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Valores fora dos limites.\"}"
    );

    return;
  }

  control.setSetpoint(
    newSetpoint
  );

  control.setHysteresis(
    newHysteresis
  );

  eventLog.add(
    EventLogModule::Category::CONTROL,
    "Configuracao manual alterada: setpoint " +
    String(newSetpoint, 1) +
    " C, histerese " +
    String(newHysteresis, 1) +
    " C"
  );

  String response =
    "{\"success\":true,"
    "\"message\":\"Configuracoes aplicadas com sucesso.\","
    "\"setpoint\":" +
    String(
      control.getSetpoint(),
      1
    ) +
    ",\"hysteresis\":" +
    String(
      control.getHysteresis(),
      1
    ) +
    "}";

  server.sendHeader(
    "Cache-Control",
    "no-store"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    response
  );
}

void WebModule::handleWifiSave() {
  if (!server.hasArg("ssid")) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Informe o nome da rede.\"}"
    );

    return;
  }

  String ssid =
    server.arg("ssid");

  String password =
    server.hasArg("password")
      ? server.arg("password")
      : "";

  ssid.trim();

  bool validSsid =
    ssid.length() >= 1 &&
    ssid.length() <= 32;

  bool validPassword =
    password.length() == 0 ||
    (
      password.length() >= 8 &&
      password.length() <= 63
    );

  if (
    !validSsid ||
    !validPassword
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"SSID invalido ou senha fora da faixa de 8 a 63 caracteres.\"}"
    );

    return;
  }

  bool saved =
    settings.saveWifiCredentials(
      ssid,
      password
    );

  bool connectionStarted =
    network.connectStation(
      ssid,
      password
    );

  if (
    !saved ||
    !connectionStarted
  ) {
    server.send(
      500,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel salvar ou iniciar a conexao.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Credenciais salvas. Tentando conectar ao Wi-Fi domestico.\"}"
  );
}

void WebModule::handleWifiForget() {
  bool cleared =
    settings.clearWifiCredentials();

  network.disconnectStation();

  eventLog.add(
    EventLogModule::Category::NETWORK,
    "Credenciais do Wi-Fi domestico removidas"
  );

  if (!cleared) {
    server.send(
      500,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel apagar as credenciais.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Wi-Fi domestico removido. A rede propria permanece ativa.\"}"
  );
}

void WebModule::handleFirmwarePreflight() {
  if (!server.hasArg("size")) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Tamanho do arquivo ausente.\"}"
    );

    return;
  }

  size_t requestedSize =
    static_cast<size_t>(
      server.arg("size").toDouble()
    );

  size_t freeSketchSpace =
    ESP.getFreeSketchSpace();

  size_t writableSpace =
    freeSketchSpace > 0x1000
      ? (
          freeSketchSpace -
          0x1000
        ) &
        0xFFFFF000
      : 0;

  bool fits =
    requestedSize > 0 &&
    requestedSize <= writableSpace;

  String response;
  response.reserve(280);

  response += "{";
  response += "\"success\":";
  response += fits
    ? "true"
    : "false";

  response += ",\"fileSize\":";
  response += String(
    static_cast<unsigned long>(
      requestedSize
    )
  );

  response += ",\"availableSize\":";
  response += String(
    static_cast<unsigned long>(
      writableSpace
    )
  );

  response += ",\"message\":\"";

  if (fits) {
    response +=
      "O arquivo cabe na particao OTA disponivel.";
  } else {
    response +=
      "O arquivo nao cabe na particao OTA. Verifique o esquema de particao e o tamanho do firmware.";
  }

  response += "\"";
  response += "}";

  server.send(
    fits ? 200 : 409,
    "application/json; charset=utf-8",
    response
  );
}

void WebModule::handleFirmwareUpdateEnd() {
  server.sendHeader(
    "Connection",
    "close"
  );

  server.sendHeader(
    "Cache-Control",
    "no-store"
  );

  if (
    firmwareUpdateSucceeded &&
    !Update.hasError()
  ) {
    server.send(
      200,
      "application/json; charset=utf-8",
      "{\"success\":true,\"message\":\"Firmware instalado. O controlador sera reiniciado.\"}"
    );

    eventLog.add(
      EventLogModule::Category::OTA,
      "Firmware instalado com sucesso; reinicializacao programada"
    );

    restartScheduled =
      true;

    restartScheduledAt =
      millis();

    return;
  }

  String errorMessage =
    firmwareUpdateError.length() > 0
      ? firmwareUpdateError
      : (
          Update.hasError()
            ? String(
                Update.errorString()
              )
            : String(
                "A atualizacao nao foi concluida."
              )
        );

  String response =
    "{\"success\":false,\"message\":\"" +
    jsonEscape(
      errorMessage
    ) +
    "\",\"received\":" +
    String(
      static_cast<unsigned long>(
        firmwareReceivedSize
      )
    ) +
    ",\"expected\":" +
    String(
      static_cast<unsigned long>(
        firmwareExpectedSize
      )
    ) +
    "}";

  server.send(
    500,
    "application/json; charset=utf-8",
    response
  );

  firmwareUpdateInProgress =
    false;

  firmwareUpdateSucceeded =
    false;
}

void WebModule::handleFirmwareUpload() {
  HTTPUpload& upload =
    server.upload();

  if (
    upload.status ==
    UPLOAD_FILE_START
  ) {
    firmwareUpdateInProgress =
      false;

    firmwareUpdateSucceeded =
      false;

    restartScheduled =
      false;

    firmwareUpdateError = "";

    firmwareReceivedSize = 0;

    firmwareExpectedSize =
      server.hasArg("size")
        ? static_cast<size_t>(
            server.arg("size").toDouble()
          )
        : 0;

    relays.allOff();

    Serial.println();
    Serial.println(
      "Atualizacao de firmware iniciada."
    );

    eventLog.add(
      EventLogModule::Category::OTA,
      "Upload de firmware iniciado: " +
      upload.filename
    );

    Serial.print(
      "Arquivo: "
    );

    Serial.println(
      upload.filename
    );

    Serial.print(
      "Tamanho informado: "
    );

    Serial.println(
      static_cast<unsigned long>(
        firmwareExpectedSize
      )
    );

    size_t freeSketchSpace =
      ESP.getFreeSketchSpace();

    size_t writableSpace =
      freeSketchSpace > 0x1000
        ? (
            freeSketchSpace -
            0x1000
          ) &
          0xFFFFF000
        : 0;

    Serial.print(
      "Espaco OTA disponivel: "
    );

    Serial.println(
      static_cast<unsigned long>(
        writableSpace
      )
    );

    if (
      firmwareExpectedSize > 0 &&
      firmwareExpectedSize >
        writableSpace
    ) {
      firmwareUpdateError =
        "Firmware maior que a particao OTA disponivel.";

      Serial.println(
        firmwareUpdateError
      );

      return;
    }

    if (Update.isRunning()) {
      Update.abort();
    }

    /*
      Inicia usando o tamanho total do slot OTA.
      Isso evita falhas causadas pelo tamanho do
      multipart ou por argumentos ainda indisponiveis
      no inicio do upload.
    */
    if (
      !Update.begin(
        writableSpace,
        U_FLASH
      )
    ) {
      firmwareUpdateError =
        String(
          Update.errorString()
        );

      Update.printError(
        Serial
      );

      return;
    }

    firmwareUpdateInProgress =
      true;

    return;
  }

  if (
    upload.status ==
    UPLOAD_FILE_WRITE
  ) {
    if (
      !firmwareUpdateInProgress
    ) {
      return;
    }

    size_t bytesWritten =
      Update.write(
        upload.buf,
        upload.currentSize
      );

    firmwareReceivedSize +=
      bytesWritten;

    if (
      bytesWritten !=
      upload.currentSize
    ) {
      firmwareUpdateError =
        String(
          Update.errorString()
        );

      Update.printError(
        Serial
      );

      Update.abort();

      firmwareUpdateInProgress =
        false;

      return;
    }

    yield();

    return;
  }

  if (
    upload.status ==
    UPLOAD_FILE_END
  ) {
    if (
      !firmwareUpdateInProgress
    ) {
      firmwareUpdateSucceeded =
        false;

      return;
    }

    bool expectedSizeMatches =
      firmwareExpectedSize == 0 ||
      firmwareReceivedSize ==
        firmwareExpectedSize;

    if (!expectedSizeMatches) {
      firmwareUpdateError =
        "O tamanho recebido nao corresponde ao arquivo selecionado.";

      Update.abort();

      firmwareUpdateInProgress =
        false;

      firmwareUpdateSucceeded =
        false;

      return;
    }

    bool updateEnded =
      Update.end(true);

    firmwareUpdateSucceeded =
      updateEnded &&
      !Update.hasError();

    if (firmwareUpdateSucceeded) {
      Serial.print(
        "Firmware recebido com sucesso: "
      );

      Serial.print(
        static_cast<unsigned long>(
          firmwareReceivedSize
        )
      );

      Serial.println(
        " bytes."
      );
    } else {
      firmwareUpdateError =
        String(
          Update.errorString()
        );

      Serial.println(
        "Falha ao finalizar a atualizacao."
      );

      Update.printError(
        Serial
      );

      firmwareUpdateInProgress =
        false;
    }

    return;
  }

  if (
    upload.status ==
    UPLOAD_FILE_ABORTED
  ) {
    Update.abort();

    firmwareUpdateInProgress =
      false;

    firmwareUpdateSucceeded =
      false;

    firmwareUpdateError =
      "Atualizacao cancelada ou conexao interrompida.";

    Serial.println(
      "Atualizacao de firmware cancelada."
    );
  }
}

void WebModule::handleProfileSave() {
  if (profile.isActive()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao e possivel editar um perfil em execucao.\"}"
    );

    return;
  }

  if (
    !server.hasArg("name") ||
    !server.hasArg("count")
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Dados do perfil incompletos.\"}"
    );

    return;
  }

  String name =
    server.arg("name");

  int stageCount =
    server.arg("count").toInt();

  if (
    stageCount < 1 ||
    stageCount >
      ProfileModule::MAX_STAGES
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Quantidade de etapas invalida.\"}"
    );

    return;
  }

  ProfileModule::Stage
    stages[
      ProfileModule::MAX_STAGES
    ];

  for (
    int index = 0;
    index < stageCount;
    index++
  ) {
    String temperatureKey =
      "temperature" +
      String(index);

    String durationSecondsKey =
      "durationSeconds" +
      String(index);

    String legacyDurationKey =
      "duration" +
      String(index);

    bool hasDuration =
      server.hasArg(
        durationSecondsKey
      ) ||
      server.hasArg(
        legacyDurationKey
      );

    if (
      !server.hasArg(
        temperatureKey
      ) ||
      !hasDuration
    ) {
      server.send(
        400,
        "application/json; charset=utf-8",
        "{\"success\":false,\"message\":\"Uma das etapas esta incompleta.\"}"
      );

      return;
    }

    float temperature =
      server.arg(
        temperatureKey
      ).toFloat();

    uint32_t durationSeconds;

    if (
      server.hasArg(
        durationSecondsKey
      )
    ) {
      durationSeconds =
        static_cast<uint32_t>(
          server.arg(
            durationSecondsKey
          ).toDouble()
        );
    } else {
      float durationHours =
        server.arg(
          legacyDurationKey
        ).toFloat();

      durationSeconds =
        static_cast<uint32_t>(
          durationHours *
          3600.0f
        );
    }

    if (
      temperature < -10.0f ||
      temperature > 40.0f ||
      durationSeconds < 60UL ||
      durationSeconds >
        90UL * 24UL * 60UL * 60UL
    ) {
      server.send(
        400,
        "application/json; charset=utf-8",
        "{\"success\":false,\"message\":\"Temperatura ou duracao fora dos limites.\"}"
      );

      return;
    }

    stages[index]
      .targetTemperature =
        temperature;

    stages[index]
      .durationSeconds =
        durationSeconds;
  }

  bool saved =
    profile.saveProfile(
      name,
      stages,
      static_cast<uint8_t>(
        stageCount
      )
    );

  if (!saved) {
    server.send(
      500,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel salvar o perfil.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Perfil salvo com sucesso.\"}"
  );
}

void WebModule::handleProfileStart() {
  if (!clock.isSynchronized()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"O relogio precisa estar sincronizado para iniciar o perfil.\"}"
    );

    return;
  }

  if (!profile.start()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel iniciar o perfil.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Perfil iniciado.\"}"
  );
}

void WebModule::handleProfilePause() {
  if (!profile.pause()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel pausar o perfil.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Perfil pausado.\"}"
  );
}

void WebModule::handleProfileResume() {
  if (!clock.isSynchronized()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"O relogio precisa estar sincronizado para retomar o perfil.\"}"
    );

    return;
  }

  if (!profile.resume()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel retomar o perfil.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Perfil retomado.\"}"
  );
}

void WebModule::handleProfileCancel() {
  if (!profile.cancel()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao existe perfil ativo para cancelar.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Perfil cancelado.\"}"
  );
}

void WebModule::handleRecipeSave() {
  if (
    !server.hasArg("slot") ||
    !server.hasArg("name") ||
    !server.hasArg("count")
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Dados da receita incompletos.\"}"
    );

    return;
  }

  int slot =
    server.arg("slot").toInt();

  int stageCount =
    server.arg("count").toInt();

  if (
    slot < 0 ||
    slot >=
      RecipeLibraryModule::MAX_RECIPES ||
    stageCount < 1 ||
    stageCount >
      ProfileModule::MAX_STAGES
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Slot ou quantidade de etapas invalida.\"}"
    );

    return;
  }

  ProfileModule::Stage
    stages[
      ProfileModule::MAX_STAGES
    ];

  for (
    int index = 0;
    index < stageCount;
    index++
  ) {
    String temperatureKey =
      "temperature" +
      String(index);

    String durationSecondsKey =
      "durationSeconds" +
      String(index);

    String legacyDurationKey =
      "duration" +
      String(index);

    bool hasDuration =
      server.hasArg(
        durationSecondsKey
      ) ||
      server.hasArg(
        legacyDurationKey
      );

    if (
      !server.hasArg(
        temperatureKey
      ) ||
      !hasDuration
    ) {
      server.send(
        400,
        "application/json; charset=utf-8",
        "{\"success\":false,\"message\":\"Uma das etapas esta incompleta.\"}"
      );

      return;
    }

    float temperature =
      server.arg(
        temperatureKey
      ).toFloat();

    uint32_t durationSeconds;

    if (
      server.hasArg(
        durationSecondsKey
      )
    ) {
      durationSeconds =
        static_cast<uint32_t>(
          server.arg(
            durationSecondsKey
          ).toDouble()
        );
    } else {
      float durationHours =
        server.arg(
          legacyDurationKey
        ).toFloat();

      durationSeconds =
        static_cast<uint32_t>(
          durationHours *
          3600.0f
        );
    }

    stages[index]
      .targetTemperature =
        temperature;

    stages[index]
      .durationSeconds =
        durationSeconds;
  }

  bool saved =
    recipeLibrary.saveRecipe(
      static_cast<uint8_t>(
        slot
      ),
      server.arg("name"),
      stages,
      static_cast<uint8_t>(
        stageCount
      )
    );

  if (!saved) {
    server.send(
      500,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel salvar a receita.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Receita salva na biblioteca.\"}"
  );
}

void WebModule::handleRecipeLoad() {
  if (!server.hasArg("slot")) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Slot ausente.\"}"
    );

    return;
  }

  int slot =
    server.arg("slot").toInt();

  if (
    slot < 0 ||
    slot >=
      RecipeLibraryModule::MAX_RECIPES ||
    !recipeLibrary.loadRecipeIntoProfile(
      static_cast<uint8_t>(
        slot
      ),
      profile
    )
  ) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel carregar a receita. Cancele o perfil ativo primeiro.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Receita carregada no editor de perfil.\"}"
  );
}

void WebModule::handleRecipeStart() {
  if (!clock.isSynchronized()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"O relogio precisa estar sincronizado.\"}"
    );

    return;
  }

  if (!server.hasArg("slot")) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Slot ausente.\"}"
    );

    return;
  }

  int slot =
    server.arg("slot").toInt();

  if (
    slot < 0 ||
    slot >=
      RecipeLibraryModule::MAX_RECIPES ||
    !recipeLibrary.startRecipe(
      static_cast<uint8_t>(
        slot
      ),
      profile
    )
  ) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel iniciar a receita.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Receita iniciada.\"}"
  );
}

void WebModule::handleRecipeDelete() {
  if (!server.hasArg("slot")) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Slot ausente.\"}"
    );

    return;
  }

  int slot =
    server.arg("slot").toInt();

  if (
    slot < 0 ||
    slot >=
      RecipeLibraryModule::MAX_RECIPES ||
    !recipeLibrary.deleteRecipe(
      static_cast<uint8_t>(
        slot
      )
    )
  ) {
    server.send(
      500,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel excluir a receita.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Receita excluida.\"}"
  );
}

void WebModule::handleAlarmSave() {
  if (
    !server.hasArg("highLimit") ||
    !server.hasArg("lowLimit") ||
    !server.hasArg("minimumChange") ||
    !server.hasArg("responseMinutes")
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Configuracao de alarmes incompleta.\"}"
    );

    return;
  }

  AlarmModule::Configuration configuration;

  configuration.sensorAlarmEnabled =
    server.hasArg("sensorEnabled") &&
    server.arg("sensorEnabled") == "1";

  configuration.highTemperatureEnabled =
    server.hasArg("highEnabled") &&
    server.arg("highEnabled") == "1";

  configuration.lowTemperatureEnabled =
    server.hasArg("lowEnabled") &&
    server.arg("lowEnabled") == "1";

  configuration.responseAlarmEnabled =
    server.hasArg("responseEnabled") &&
    server.arg("responseEnabled") == "1";

  configuration.highTemperatureLimit =
    server.arg("highLimit").toFloat();

  configuration.lowTemperatureLimit =
    server.arg("lowLimit").toFloat();

  configuration.minimumExpectedChange =
    server.arg("minimumChange").toFloat();

  float responseMinutes =
    server.arg("responseMinutes").toFloat();

  configuration.responseTimeoutSeconds =
    static_cast<uint32_t>(
      responseMinutes *
      60.0f
    );

  if (
    !alarms.saveConfiguration(
      configuration
    )
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Valores de alarme fora dos limites.\"}"
    );

    return;
  }

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Configuracao de alarmes salva.\"}"
  );
}

void WebModule::
handleAlarmAcknowledge() {
  alarms.acknowledgeAll();

  eventLog.add(
    EventLogModule::Category::ALARM,
    "Alarmes ativos reconhecidos pelo usuario"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Alarmes ativos reconhecidos.\"}"
  );
}

void WebModule::handleEventLogApi() {
  server.sendHeader(
    "Cache-Control",
    "no-store"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    buildEventLogJson()
  );
}

void WebModule::handleEventLogClear() {
  eventLog.clear();

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Historico de eventos limpo.\"}"
  );
}

void WebModule::handleCalibrationSave() {
  if (
    !server.hasArg(
      "refrigeratorOffset"
    ) ||
    !server.hasArg(
      "thermalWellOffset"
    )
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Valores de calibracao ausentes.\"}"
    );

    return;
  }

  float refrigeratorOffset =
    server.arg(
      "refrigeratorOffset"
    ).toFloat();

  float thermalWellOffset =
    server.arg(
      "thermalWellOffset"
    ).toFloat();

  if (
    !temperatures.saveCalibration(
      refrigeratorOffset,
      thermalWellOffset
    )
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Offsets invalidos ou falha ao salvar. Use valores entre -10 e +10 C.\"}"
    );

    return;
  }

  eventLog.add(
    EventLogModule::Category::SYSTEM,
    "Calibracao alterada: geladeira " +
    String(
      refrigeratorOffset,
      2
    ) +
    " C; poco termico " +
    String(
      thermalWellOffset,
      2
    ) +
    " C"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Calibracao salva e aplicada imediatamente.\"}"
  );
}

void WebModule::handleCalibrationReset() {
  if (
    !temperatures.resetCalibration()
  ) {
    server.send(
      500,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Nao foi possivel zerar a calibracao.\"}"
    );

    return;
  }

  eventLog.add(
    EventLogModule::Category::SYSTEM,
    "Offsets de calibracao zerados"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Calibracao dos dois sensores zerada.\"}"
  );
}

void WebModule::handleCloudSave() {
  if (
    !server.hasArg("url") ||
    !server.hasArg("interval")
  ) {
    server.send(
      400,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Configuracao cloud incompleta.\"}"
    );

    return;
  }

  bool enabled =
    server.hasArg("enabled") &&
    (
      server.arg("enabled") == "1" ||
      server.arg("enabled") == "true" ||
      server.arg("enabled") == "on"
    );

  String telemetryUrl =
    server.arg("url");

  uint32_t intervalSeconds =
    static_cast<uint32_t>(
      server.arg("interval").toInt()
    );

  if (
    !cloud.saveConfiguration(
      enabled,
      telemetryUrl,
      intervalSeconds
    )
  ) {
    String response =
      "{\"success\":false,\"message\":\"";

    response += jsonEscape(
      cloud.getLastError()
    );

    response += "\"}";

    server.send(
      400,
      "application/json; charset=utf-8",
      response
    );

    return;
  }

  eventLog.add(
    EventLogModule::Category::CLOUD,
    enabled
      ? "Sincronizacao cloud configurada"
      : "Sincronizacao cloud desabilitada"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Configuracao cloud salva.\"}"
  );
}

void WebModule::handleCloudSync() {
  if (!cloud.requestImmediateSync()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"A nuvem esta desabilitada, incompleta ou ja esta enviando.\"}"
    );

    return;
  }

  eventLog.add(
    EventLogModule::Category::CLOUD,
    "Sincronizacao cloud manual solicitada"
  );

  server.send(
    202,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Sincronizacao agendada.\"}"
  );
}

void WebModule::handleCloudTokenRegenerate() {
  if (!cloud.regenerateDeviceToken()) {
    server.send(
      409,
      "application/json; charset=utf-8",
      "{\"success\":false,\"message\":\"Aguarde o envio atual terminar para renovar o token.\"}"
    );

    return;
  }

  eventLog.add(
    EventLogModule::Category::CLOUD,
    "Token do dispositivo renovado"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    "{\"success\":true,\"message\":\"Token renovado. O dispositivo precisara ser vinculado novamente.\"}"
  );
}

void WebModule::handleApi() {
  server.sendHeader(
    "Cache-Control",
    "no-store"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    buildApiJson()
  );
}

void WebModule::handleHistoryApi() {
  server.sendHeader(
    "Cache-Control",
    "no-store"
  );

  server.send(
    200,
    "application/json; charset=utf-8",
    buildHistoryJson()
  );
}

void WebModule::handleNotFound() {
  server.sendHeader(
    "Location",
    "/"
  );

  server.send(
    302,
    "text/plain",
    ""
  );
}

String WebModule::buildApiJson() const {
  float temperature =
    temperatures.getRefrigeratorTemperature();

  bool sensorConnected =
    temperatures.isRefrigeratorSensorConnected();

  bool waitingForCompressor =
    control.getState() ==
    ControlModule::State::WAITING_COOLING;

  String json;
  json.reserve(1800);

  json += "{";
  json += "\"temperature\":";

  if (sensorConnected) {
    json += String(
      temperature,
      1
    );
  } else {
    json += "null";
  }

  json += ",\"temperatureText\":\"";

  if (sensorConnected) {
    json += temperatureToString(
      temperature,
      1
    );
  } else {
    json += "--.-";
  }

  json += "\"";
  json += ",\"sensorConnected\":";
  json += sensorConnected ? "true" : "false";

  json += ",\"refrigeratorRawTemperature\":";

  if (
    temperatures
      .isRefrigeratorSensorConnected()
  ) {
    json += String(
      temperatures
        .getRefrigeratorRawTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"refrigeratorOffset\":";
  json += String(
    temperatures
      .getRefrigeratorOffset(),
    2
  );

  json += ",\"thermalWellConnected\":";
  json += temperatures
    .isThermalWellSensorConnected()
      ? "true"
      : "false";

  json += ",\"thermalWellTemperature\":";

  if (
    temperatures
      .isThermalWellSensorConnected()
  ) {
    json += String(
      temperatures
        .getThermalWellTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"thermalWellRawTemperature\":";

  if (
    temperatures
      .isThermalWellSensorConnected()
  ) {
    json += String(
      temperatures
        .getThermalWellRawTemperature(),
      2
    );
  } else {
    json += "null";
  }

  json += ",\"thermalWellOffset\":";
  json += String(
    temperatures
      .getThermalWellOffset(),
    2
  );

  json += ",\"sensorCount\":";
  json += String(
    temperatures.getSensorCount()
  );

  json += ",\"setpoint\":";
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
    String(
      control.getStateText()
    )
  );
  json += "\"";

  json += ",\"cooling\":";
  json += relays.isCoolingOn()
    ? "true"
    : "false";

  json += ",\"heating\":";
  json += relays.isHeatingOn()
    ? "true"
    : "false";

  json += ",\"waitingForCompressor\":";
  json += waitingForCompressor
    ? "true"
    : "false";

  json += ",\"protectionSeconds\":";
  json += String(
    control.getCoolingDelayRemainingSeconds()
  );

  json += ",\"ipAddress\":\"";
  json += network.getAccessPointIp().toString();
  json += "\"";

  json += ",\"connectedClients\":";
  json += String(
    network.getConnectedAccessPointClients()
  );

  json += ",\"historyPoints\":";
  json += String(
    history.getPointCount()
  );

  /*
    Mantem a assinatura Maltworks incorporada ao .bin.
    O retorno nao precisa ser enviado separadamente,
    mas a chamada impede a remocao do marcador.
  */
  String firmwareMetadataRetention =
    FirmwareInfo::getMetadataJson();

  (void)firmwareMetadataRetention;

  json += ",\"firmwareVersion\":\"";
  json += FirmwareInfo::VERSION;
  json += "\"";

  json += ",\"firmwareProduct\":\"";
  json += FirmwareInfo::PRODUCT;
  json += "\"";

  json += ",\"firmwareBoardFamily\":\"";
  json += FirmwareInfo::BOARD_FAMILY;
  json += "\"";

  json += ",\"firmwarePhase\":\"";
  json += FirmwareInfo::BUILD_PHASE;
  json += "\"";

  json += ",\"firmwareUpdateInProgress\":";
  json += firmwareUpdateInProgress
    ? "true"
    : "false";

  json += ",\"otaAvailableBytes\":";
  json += String(
    static_cast<unsigned long>(
      (
        ESP.getFreeSketchSpace() >
          0x1000
      )
        ? (
            (
              ESP.getFreeSketchSpace() -
              0x1000
            ) &
            0xFFFFF000
          )
        : 0
    )
  );

  json += ",\"clockSynchronized\":";
  json += clock.isSynchronized()
    ? "true"
    : "false";

  json += ",\"clockPending\":";
  json += clock.isSynchronizationPending()
    ? "true"
    : "false";

  json += ",\"clockStatus\":\"";
  json += jsonEscape(
    clock.getStatusText()
  );
  json += "\"";

  json += ",\"dateText\":\"";
  json += jsonEscape(
    clock.getDateText()
  );
  json += "\"";

  json += ",\"timeText\":\"";
  json += jsonEscape(
    clock.getTimeText()
  );
  json += "\"";

  json += ",\"dateTimeText\":\"";
  json += jsonEscape(
    clock.getDateTimeText()
  );
  json += "\"";

  json += ",\"lastSynchronization\":\"";
  json += jsonEscape(
    clock.getLastSynchronizationText()
  );
  json += "\"";

  json += ",\"epoch\":";
  json += String(
    static_cast<unsigned long>(
      clock.getEpoch()
    )
  );

  json += ",\"stationConfigured\":";
  json += network.hasStationCredentials()
    ? "true"
    : "false";

  json += ",\"stationConnected\":";
  json += network.isStationConnected()
    ? "true"
    : "false";

  json += ",\"stationStatus\":\"";
  json += jsonEscape(
    network.getStationStatusText()
  );
  json += "\"";

  json += ",\"stationSsid\":\"";
  json += jsonEscape(
    network.getStationSsid()
  );
  json += "\"";

  json += ",\"stationIp\":\"";

  if (network.isStationConnected()) {
    json += network.getStationIp().toString();
  } else {
    json += "--";
  }

  json += "\"";

  json += ",\"stationRssi\":";
  json += String(
    network.getStationRssi()
  );

  json += ",\"profile\":";
  json += buildProfileJson();

  json += ",\"recipeLibrary\":";
  json += buildRecipeLibraryJson();

  json += ",\"alarms\":";
  json += buildAlarmJson();

  json += ",\"cloud\":";
  json += buildCloudJson();

  json += ",\"eventCount\":";
  json += String(
    eventLog.getCount()
  );

  json += "}";

  return json;
}

String WebModule::buildProfileJson() const {
  String json;

  json.reserve(900);

  json += "{";

  json += "\"configured\":";
  json += profile.hasProfile()
    ? "true"
    : "false";

  json += ",\"active\":";
  json += profile.isActive()
    ? "true"
    : "false";

  json += ",\"paused\":";
  json += profile.isPaused()
    ? "true"
    : "false";

  json += ",\"completed\":";
  json += profile.isCompleted()
    ? "true"
    : "false";

  json += ",\"name\":\"";
  json += jsonEscape(
    profile.getProfileName()
  );
  json += "\"";

  json += ",\"state\":\"";
  json += jsonEscape(
    String(
      profile.getRunStateText()
    )
  );
  json += "\"";

  json += ",\"stageCount\":";
  json += String(
    profile.getStageCount()
  );

  json += ",\"currentStage\":";
  json += String(
    profile.getCurrentStageIndex()
  );

  json += ",\"remainingSeconds\":";
  json += String(
    profile.getRemainingSeconds()
  );

  json += ",\"elapsedSeconds\":";
  json += String(
    profile.getElapsedSeconds()
  );

  json += ",\"totalRemainingSeconds\":";
  json += String(
    profile.getTotalRemainingSeconds()
  );

  json += ",\"totalDurationSeconds\":";
  json += String(
    profile.getTotalDurationSeconds()
  );

  json += ",\"currentTarget\":";
  json += String(
    profile.getCurrentTargetTemperature(),
    1
  );

  json += ",\"stages\":[";

  for (
    uint8_t index = 0;
    index <
      profile.getStageCount();
    index++
  ) {
    if (index > 0) {
      json += ",";
    }

    ProfileModule::Stage stage =
      profile.getStage(index);

    json += "{";
    json += "\"temperature\":";
    json += String(
      stage.targetTemperature,
      1
    );

    json += ",\"durationSeconds\":";
    json += String(
      stage.durationSeconds
    );

    json += "}";
  }

  json += "]";
  json += "}";

  return json;
}

String WebModule::
buildRecipeLibraryJson() const {
  String json;

  json.reserve(2500);

  json += "{";
  json += "\"maxRecipes\":";
  json += String(
    RecipeLibraryModule::MAX_RECIPES
  );

  json += ",\"count\":";
  json += String(
    recipeLibrary.getRecipeCount()
  );

  json += ",\"recipes\":[";

  for (
    uint8_t slot = 0;
    slot <
      RecipeLibraryModule::MAX_RECIPES;
    slot++
  ) {
    if (slot > 0) {
      json += ",";
    }

    RecipeLibraryModule::Recipe recipe =
      recipeLibrary.getRecipe(slot);

    json += "{";
    json += "\"slot\":";
    json += String(slot);

    json += ",\"used\":";
    json += recipe.used
      ? "true"
      : "false";

    json += ",\"name\":\"";
    json += jsonEscape(
      recipe.name
    );
    json += "\"";

    json += ",\"stageCount\":";
    json += String(
      recipe.stageCount
    );

    json += ",\"stages\":[";

    for (
      uint8_t index = 0;
      index < recipe.stageCount;
      index++
    ) {
      if (index > 0) {
        json += ",";
      }

      json += "{";
      json += "\"temperature\":";
      json += String(
        recipe.stages[index]
          .targetTemperature,
        1
      );

      json += ",\"durationSeconds\":";
      json += String(
        recipe.stages[index]
          .durationSeconds
      );

      json += "}";
    }

    json += "]";
    json += "}";
  }

  json += "]";
  json += "}";

  return json;
}

String WebModule::buildAlarmJson() const {
  String json;

  json.reserve(850);

  AlarmModule::Configuration configuration =
    alarms.getConfiguration();

  json += "{";

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

  json += "\"sensorEnabled\":";
  json += configuration.sensorAlarmEnabled
    ? "true"
    : "false";

  json += ",\"highEnabled\":";
  json += configuration.highTemperatureEnabled
    ? "true"
    : "false";

  json += ",\"lowEnabled\":";
  json += configuration.lowTemperatureEnabled
    ? "true"
    : "false";

  json += ",\"responseEnabled\":";
  json += configuration.responseAlarmEnabled
    ? "true"
    : "false";

  json += ",\"highLimit\":";
  json += String(
    configuration.highTemperatureLimit,
    1
  );

  json += ",\"lowLimit\":";
  json += String(
    configuration.lowTemperatureLimit,
    1
  );

  json += ",\"minimumChange\":";
  json += String(
    configuration.minimumExpectedChange,
    1
  );

  json += ",\"responseTimeoutSeconds\":";
  json += String(
    configuration.responseTimeoutSeconds
  );

  json += "}";

  json += ",\"items\":[";

  for (
    uint8_t index = 0;
    index < 4;
    index++
  ) {
    if (index > 0) {
      json += ",";
    }

    AlarmModule::AlarmId id =
      static_cast<AlarmModule::AlarmId>(
        index
      );

    AlarmModule::AlarmState state =
      alarms.getAlarmState(id);

    json += "{";

    json += "\"id\":";
    json += String(index);

    json += ",\"name\":\"";
    json += jsonEscape(
      String(
        alarms.getAlarmName(id)
      )
    );
    json += "\"";

    json += ",\"active\":";
    json += state.active
      ? "true"
      : "false";

    json += ",\"acknowledged\":";
    json += state.acknowledged
      ? "true"
      : "false";

    json += ",\"activeSeconds\":";

    if (
      state.active &&
      state.activeSinceMs > 0
    ) {
      json += String(
        (
          millis() -
          state.activeSinceMs
        ) / 1000UL
      );
    } else {
      json += "0";
    }

    json += "}";
  }

  json += "]";
  json += "}";

  return json;
}

String WebModule::buildEventLogJson() const {
  String json;

  uint16_t count =
    eventLog.getCount();

  json.reserve(
    100 +
    count * 145
  );

  json += "{";
  json += "\"count\":";
  json += String(count);
  json += ",\"events\":[";

  for (
    uint16_t index = 0;
    index < count;
    index++
  ) {
    EventLogModule::Event event;

    if (
      !eventLog.getEvent(
        index,
        event
      )
    ) {
      continue;
    }

    if (index > 0) {
      json += ",";
    }

    json += "{";

    json += "\"category\":\"";
    json += jsonEscape(
      String(
        eventLog.getCategoryText(
          event.category
        )
      )
    );
    json += "\"";

    json += ",\"dateTime\":\"";
    json += jsonEscape(
      eventLog.formatDateTime(
        event
      )
    );
    json += "\"";

    json += ",\"message\":\"";
    json += jsonEscape(
      String(
        event.message
      )
    );
    json += "\"";

    json += ",\"epoch\":";
    json += String(
      event.epoch
    );

    json += "}";

    if (
      index % 25 == 0
    ) {
      yield();
    }
  }

  json += "]";
  json += "}";

  return json;
}

String WebModule::buildCloudJson() const {
  String json;
  json.reserve(650);

  json += "{";

  json += "\"deviceId\":\"";
  json += jsonEscape(
    cloud.getDeviceId()
  );
  json += "\"";

  json += ",\"enabled\":";
  json += cloud.isEnabled()
    ? "true"
    : "false";

  json += ",\"configured\":";
  json += cloud.isConfigured()
    ? "true"
    : "false";

  json += ",\"online\":";
  json += cloud.isOnline()
    ? "true"
    : "false";

  json += ",\"status\":\"";
  json += jsonEscape(
    String(
      cloud.getStatusText()
    )
  );
  json += "\"";

  json += ",\"telemetryUrl\":\"";
  json += jsonEscape(
    cloud.getTelemetryUrl()
  );
  json += "\"";

  json += ",\"intervalSeconds\":";
  json += String(
    cloud
      .getTelemetryIntervalSeconds()
  );

  json += ",\"tokenHint\":\"";
  json += jsonEscape(
    cloud.getDeviceTokenHint()
  );
  json += "\"";

  json += ",\"requestInProgress\":";
  json += cloud.isRequestInProgress()
    ? "true"
    : "false";

  json += ",\"lastHttpCode\":";
  json += String(
    cloud.getLastHttpCode()
  );

  json += ",\"lastError\":\"";
  json += jsonEscape(
    cloud.getLastError()
  );
  json += "\"";

  json += ",\"lastSuccessEpoch\":";
  json += String(
    cloud.getLastSuccessEpoch()
  );

  json += ",\"nextAttemptSeconds\":";
  json += String(
    cloud.getSecondsUntilNextAttempt()
  );

  json += "}";

  return json;
}

String WebModule::buildHistoryJson() const {
  String json;

  uint16_t pointCount =
    history.getPointCount();

  json.reserve(
    80 +
    pointCount * 48
  );

  json += "{";
  json += "\"count\":";
  json += String(pointCount);
  json += ",\"points\":[";

  bool firstPointWritten =
    false;

  for (
    uint16_t index = 0;
    index < pointCount;
    index++
  ) {
    float temperature;
    float setpoint;
    bool temperatureValid;
    unsigned long ageSeconds;

    if (
      !history.getPoint(
        index,
        temperature,
        setpoint,
        temperatureValid,
        ageSeconds
      )
    ) {
      continue;
    }

    if (firstPointWritten) {
      json += ",";
    }

    firstPointWritten =
      true;

    json += "{";
    json += "\"ageSeconds\":";
    json += String(ageSeconds);

    json += ",\"temperature\":";

    if (temperatureValid) {
      json += String(
        temperature,
        2
      );
    } else {
      json += "null";
    }

    json += ",\"setpoint\":";
    json += String(
      setpoint,
      2
    );

    json += "}";

    if (
      index % 40 == 0
    ) {
      yield();
    }
  }

  json += "]}";

  return json;
}

String WebModule::temperatureToString(
  float temperature,
  unsigned int decimalPlaces
) const {
  if (isnan(temperature)) {
    return "--.-";
  }

  return String(
    temperature,
    decimalPlaces
  );
}

String WebModule::jsonEscape(
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
    char character =
      text.charAt(index);

    switch (character) {
      case '"':
        escaped += "\\\"";
        break;

      case '\\':
        escaped += "\\\\";
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
          ) < 0x20
        ) {
          char unicodeEscape[7];

          snprintf(
            unicodeEscape,
            sizeof(unicodeEscape),
            "\\u%04x",
            static_cast<uint8_t>(
              character
            )
          );

          escaped +=
            unicodeEscape;
        } else {
          escaped +=
            character;
        }

        break;
    }
  }

  return escaped;
}
