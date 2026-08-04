#include "profilemodule.h"

ProfileModule::ProfileModule(
  ClockModule& clockModule,
  ControlModule& controlModule
) :
  clock(clockModule),
  control(controlModule),
  profileName(""),
  stageCount(0),
  currentStageIndex(0),
  runState(RunState::STOPPED),
  stageStartEpoch(0),
  pausedRemainingSeconds(0),
  initialized(false) {
  for (
    uint8_t index = 0;
    index < MAX_STAGES;
    index++
  ) {
    stages[index].targetTemperature =
      20.0f;

    stages[index].durationSeconds =
      3600UL;
  }
}

bool ProfileModule::begin() {
  initialized =
    preferences.begin(
      STORAGE_NAMESPACE,
      false
    );

  if (!initialized) {
    Serial.println(
      "Falha ao inicializar o modulo de perfis."
    );

    return false;
  }

  loadProfile();
  loadRuntimeState();

  if (
    isActive() &&
    stageCount > 0 &&
    currentStageIndex < stageCount
  ) {
    applyCurrentStageSetpoint();
  }

  Serial.println(
    "Modulo de perfis inicializado."
  );

  if (hasProfile()) {
    Serial.print(
      "Perfil carregado: "
    );

    Serial.println(
      profileName
    );
  }

  return true;
}

void ProfileModule::update() {
  if (
    !initialized ||
    !isActive() ||
    stageCount == 0
  ) {
    return;
  }

  applyCurrentStageSetpoint();

  if (
    runState ==
    RunState::PAUSED
  ) {
    return;
  }

  if (!clock.isSynchronized()) {
    if (
      runState !=
      RunState::WAITING_CLOCK
    ) {
      runState =
        RunState::WAITING_CLOCK;

      saveRuntimeState();
    }

    return;
  }

  if (
    runState ==
    RunState::WAITING_CLOCK
  ) {
    if (
      pausedRemainingSeconds > 0
    ) {
      stageStartEpoch =
        clock.getEpoch() -
        (
          stages[currentStageIndex]
            .durationSeconds -
          pausedRemainingSeconds
        );

      pausedRemainingSeconds =
        0;
    } else if (
      stageStartEpoch == 0
    ) {
      stageStartEpoch =
        clock.getEpoch();
    }

    runState =
      RunState::RUNNING;

    saveRuntimeState();
  }

  advanceStagesUsingClock();
}

bool ProfileModule::saveProfile(
  const String& candidateName,
  const Stage* candidateStages,
  uint8_t candidateCount
) {
  if (
    !initialized ||
    isActive() ||
    !validateProfile(
      candidateName,
      candidateStages,
      candidateCount
    )
  ) {
    return false;
  }

  profileName =
    candidateName;

  profileName.trim();

  stageCount =
    candidateCount;

  for (
    uint8_t index = 0;
    index < stageCount;
    index++
  ) {
    stages[index] =
      candidateStages[index];
  }

  for (
    uint8_t index = stageCount;
    index < MAX_STAGES;
    index++
  ) {
    stages[index].targetTemperature =
      20.0f;

    stages[index].durationSeconds =
      3600UL;
  }

  currentStageIndex = 0;
  runState =
    RunState::STOPPED;

  stageStartEpoch = 0;
  pausedRemainingSeconds = 0;

  clearRuntimeState();

  bool saved =
    saveDefinition();

  if (saved) {
    Serial.print(
      "Perfil salvo: "
    );

    Serial.println(
      profileName
    );
  }

  return saved;
}

bool ProfileModule::start() {
  if (
    !initialized ||
    !hasProfile() ||
    isActive() ||
    !clock.isSynchronized()
  ) {
    return false;
  }

  currentStageIndex = 0;
  stageStartEpoch =
    clock.getEpoch();

  pausedRemainingSeconds = 0;

  runState =
    RunState::RUNNING;

  applyCurrentStageSetpoint();

  saveRuntimeState();

  Serial.print(
    "Perfil iniciado: "
  );

  Serial.println(
    profileName
  );

  return true;
}

bool ProfileModule::pause() {
  if (
    !initialized ||
    runState !=
      RunState::RUNNING
  ) {
    return false;
  }

  pausedRemainingSeconds =
    getRemainingSeconds();

  runState =
    RunState::PAUSED;

  saveRuntimeState();

  Serial.println(
    "Perfil pausado."
  );

  return true;
}

bool ProfileModule::resume() {
  if (
    !initialized ||
    runState !=
      RunState::PAUSED ||
    !clock.isSynchronized()
  ) {
    return false;
  }

  uint32_t stageDuration =
    stages[currentStageIndex]
      .durationSeconds;

  uint32_t elapsedBeforePause =
    stageDuration >
      pausedRemainingSeconds
      ? stageDuration -
        pausedRemainingSeconds
      : 0;

  stageStartEpoch =
    clock.getEpoch() -
    elapsedBeforePause;

  pausedRemainingSeconds = 0;

  runState =
    RunState::RUNNING;

  applyCurrentStageSetpoint();

  saveRuntimeState();

  Serial.println(
    "Perfil retomado."
  );

  return true;
}

bool ProfileModule::cancel() {
  if (
    !initialized ||
    !isActive()
  ) {
    return false;
  }

  currentStageIndex = 0;
  stageStartEpoch = 0;
  pausedRemainingSeconds = 0;

  runState =
    RunState::STOPPED;

  clearRuntimeState();

  Serial.println(
    "Perfil cancelado."
  );

  return true;
}

bool ProfileModule::hasProfile() const {
  return
    profileName.length() > 0 &&
    stageCount > 0;
}

bool ProfileModule::isActive() const {
  return
    runState ==
      RunState::RUNNING ||
    runState ==
      RunState::PAUSED ||
    runState ==
      RunState::WAITING_CLOCK;
}

bool ProfileModule::isPaused() const {
  return runState ==
    RunState::PAUSED;
}

bool ProfileModule::isCompleted() const {
  return runState ==
    RunState::COMPLETED;
}

String ProfileModule::
getProfileName() const {
  return profileName;
}

uint8_t ProfileModule::
getStageCount() const {
  return stageCount;
}

uint8_t ProfileModule::
getCurrentStageIndex() const {
  return currentStageIndex;
}

ProfileModule::Stage
ProfileModule::getStage(
  uint8_t index
) const {
  Stage emptyStage = {
    20.0f,
    0
  };

  if (index >= stageCount) {
    return emptyStage;
  }

  return stages[index];
}

float ProfileModule::
getCurrentTargetTemperature() const {
  if (
    stageCount == 0 ||
    currentStageIndex >= stageCount
  ) {
    return control.getSetpoint();
  }

  return stages[currentStageIndex]
    .targetTemperature;
}

uint32_t ProfileModule::
getRemainingSeconds() const {
  if (
    !isActive() ||
    currentStageIndex >= stageCount
  ) {
    return 0;
  }

  if (
    runState ==
    RunState::PAUSED
  ) {
    return pausedRemainingSeconds;
  }

  if (
    !clock.isSynchronized() ||
    stageStartEpoch == 0
  ) {
    return stages[currentStageIndex]
      .durationSeconds;
  }

  time_t now =
    clock.getEpoch();

  if (now <= stageStartEpoch) {
    return stages[currentStageIndex]
      .durationSeconds;
  }

  uint32_t elapsed =
    static_cast<uint32_t>(
      now - stageStartEpoch
    );

  uint32_t duration =
    stages[currentStageIndex]
      .durationSeconds;

  if (elapsed >= duration) {
    return 0;
  }

  return duration - elapsed;
}

uint32_t ProfileModule::
getElapsedSeconds() const {
  if (
    !isActive() ||
    currentStageIndex >= stageCount
  ) {
    return 0;
  }

  uint32_t duration =
    stages[currentStageIndex]
      .durationSeconds;

  uint32_t remaining =
    getRemainingSeconds();

  return duration > remaining
    ? duration - remaining
    : 0;
}

uint32_t ProfileModule::
getTotalDurationSeconds() const {
  uint32_t total = 0;

  for (
    uint8_t index = 0;
    index < stageCount;
    index++
  ) {
    total +=
      stages[index].durationSeconds;
  }

  return total;
}

uint32_t ProfileModule::
getTotalRemainingSeconds() const {
  if (!isActive()) {
    return 0;
  }

  uint32_t total =
    getRemainingSeconds();

  for (
    uint8_t index =
      currentStageIndex + 1;
    index < stageCount;
    index++
  ) {
    total +=
      stages[index].durationSeconds;
  }

  return total;
}

ProfileModule::RunState
ProfileModule::getRunState() const {
  return runState;
}

const char*
ProfileModule::getRunStateText() const {
  switch (runState) {
    case RunState::STOPPED:
      return "PARADO";

    case RunState::RUNNING:
      return "EM EXECUCAO";

    case RunState::PAUSED:
      return "PAUSADO";

    case RunState::COMPLETED:
      return "CONCLUIDO";

    case RunState::WAITING_CLOCK:
      return "AGUARDANDO RELOGIO";
  }

  return "DESCONHECIDO";
}

bool ProfileModule::loadProfile() {
  profileName =
    preferences.getString(
      "name",
      ""
    );

  stageCount =
    preferences.getUChar(
      "count",
      0
    );

  if (
    stageCount >
    MAX_STAGES
  ) {
    stageCount = 0;
  }

  for (
    uint8_t index = 0;
    index < stageCount;
    index++
  ) {
    String temperatureKey =
      "t" + String(index);

    String durationKey =
      "d" + String(index);

    stages[index].targetTemperature =
      preferences.getFloat(
        temperatureKey.c_str(),
        20.0f
      );

    stages[index].durationSeconds =
      preferences.getULong(
        durationKey.c_str(),
        3600UL
      );
  }

  return hasProfile();
}

bool ProfileModule::
loadRuntimeState() {
  uint8_t storedState =
    preferences.getUChar(
      "state",
      static_cast<uint8_t>(
        RunState::STOPPED
      )
    );

  if (
    storedState >
    static_cast<uint8_t>(
      RunState::WAITING_CLOCK
    )
  ) {
    storedState =
      static_cast<uint8_t>(
        RunState::STOPPED
      );
  }

  runState =
    static_cast<RunState>(
      storedState
    );

  currentStageIndex =
    preferences.getUChar(
      "stage",
      0
    );

  stageStartEpoch =
    static_cast<time_t>(
      preferences.getULong64(
        "start",
        0
      )
    );

  pausedRemainingSeconds =
    preferences.getULong(
      "remain",
      0
    );

  if (
    currentStageIndex >=
    stageCount
  ) {
    currentStageIndex = 0;
    runState =
      RunState::STOPPED;
  }

  return true;
}

bool ProfileModule::saveDefinition() {
  if (!initialized) {
    return false;
  }

  bool success =
    preferences.putString(
      "name",
      profileName
    ) > 0;

  preferences.putUChar(
    "count",
    stageCount
  );

  for (
    uint8_t index = 0;
    index < MAX_STAGES;
    index++
  ) {
    String temperatureKey =
      "t" + String(index);

    String durationKey =
      "d" + String(index);

    if (index < stageCount) {
      preferences.putFloat(
        temperatureKey.c_str(),
        stages[index]
          .targetTemperature
      );

      preferences.putULong(
        durationKey.c_str(),
        stages[index]
          .durationSeconds
      );
    } else {
      preferences.remove(
        temperatureKey.c_str()
      );

      preferences.remove(
        durationKey.c_str()
      );
    }
  }

  return success;
}

bool ProfileModule::
saveRuntimeState() {
  if (!initialized) {
    return false;
  }

  preferences.putUChar(
    "state",
    static_cast<uint8_t>(
      runState
    )
  );

  preferences.putUChar(
    "stage",
    currentStageIndex
  );

  preferences.putULong64(
    "start",
    static_cast<uint64_t>(
      stageStartEpoch
    )
  );

  preferences.putULong(
    "remain",
    pausedRemainingSeconds
  );

  return true;
}

void ProfileModule::
clearRuntimeState() {
  preferences.putUChar(
    "state",
    static_cast<uint8_t>(
      runState
    )
  );

  preferences.putUChar(
    "stage",
    currentStageIndex
  );

  preferences.putULong64(
    "start",
    0
  );

  preferences.putULong(
    "remain",
    0
  );
}

bool ProfileModule::validateProfile(
  const String& candidateName,
  const Stage* candidateStages,
  uint8_t candidateCount
) const {
  String normalizedName =
    candidateName;

  normalizedName.trim();

  if (
    normalizedName.length() == 0 ||
    normalizedName.length() > 31 ||
    candidateStages == nullptr ||
    candidateCount == 0 ||
    candidateCount > MAX_STAGES
  ) {
    return false;
  }

  for (
    uint8_t index = 0;
    index < candidateCount;
    index++
  ) {
    float target =
      candidateStages[index]
        .targetTemperature;

    uint32_t duration =
      candidateStages[index]
        .durationSeconds;

    if (
      target < -10.0f ||
      target > 40.0f ||
      duration < 60UL ||
      duration >
        90UL * 24UL * 60UL * 60UL
    ) {
      return false;
    }
  }

  return true;
}

void ProfileModule::
applyCurrentStageSetpoint() {
  if (
    stageCount == 0 ||
    currentStageIndex >= stageCount
  ) {
    return;
  }

  control.setSetpoint(
    stages[currentStageIndex]
      .targetTemperature
  );
}

void ProfileModule::
advanceStagesUsingClock() {
  if (
    runState !=
      RunState::RUNNING ||
    !clock.isSynchronized() ||
    currentStageIndex >= stageCount
  ) {
    return;
  }

  time_t now =
    clock.getEpoch();

  if (
    now == 0 ||
    stageStartEpoch == 0
  ) {
    return;
  }

  bool stateChanged =
    false;

  while (
    currentStageIndex < stageCount
  ) {
    uint32_t duration =
      stages[currentStageIndex]
        .durationSeconds;

    time_t stageEnd =
      stageStartEpoch +
      duration;

    if (now < stageEnd) {
      break;
    }

    currentStageIndex++;

    if (
      currentStageIndex >= stageCount
    ) {
      completeProfile();
      return;
    }

    stageStartEpoch =
      stageEnd;

    applyCurrentStageSetpoint();

    stateChanged =
      true;

    Serial.print(
      "Perfil avancou para a etapa "
    );

    Serial.println(
      currentStageIndex + 1
    );
  }

  if (stateChanged) {
    saveRuntimeState();
  }
}

void ProfileModule::
completeProfile() {
  runState =
    RunState::COMPLETED;

  stageStartEpoch = 0;
  pausedRemainingSeconds = 0;

  saveRuntimeState();

  Serial.println(
    "Perfil de fermentacao concluido."
  );
}
