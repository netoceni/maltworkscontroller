#ifndef PROFILEMODULE_H
#define PROFILEMODULE_H

#include <Arduino.h>
#include <Preferences.h>

#include "clockmodule.h"
#include "controlmodule.h"

class ProfileModule {
public:
  static constexpr uint8_t MAX_STAGES = 8;

  struct Stage {
    float targetTemperature;
    uint32_t durationSeconds;
  };

  enum class RunState {
    STOPPED,
    RUNNING,
    PAUSED,
    COMPLETED,
    WAITING_CLOCK
  };

  ProfileModule(
    ClockModule& clockModule,
    ControlModule& controlModule
  );

  bool begin();
  void update();

  bool saveProfile(
    const String& profileName,
    const Stage* stages,
    uint8_t stageCount
  );

  bool start();
  bool pause();
  bool resume();
  bool cancel();

  bool hasProfile() const;
  bool isActive() const;
  bool isPaused() const;
  bool isCompleted() const;

  String getProfileName() const;
  uint8_t getStageCount() const;
  uint8_t getCurrentStageIndex() const;

  Stage getStage(
    uint8_t index
  ) const;

  float getCurrentTargetTemperature() const;
  uint32_t getRemainingSeconds() const;
  uint32_t getElapsedSeconds() const;
  uint32_t getTotalDurationSeconds() const;
  uint32_t getTotalRemainingSeconds() const;

  RunState getRunState() const;
  const char* getRunStateText() const;

private:
  ClockModule& clock;
  ControlModule& control;

  Preferences preferences;

  String profileName;
  Stage stages[MAX_STAGES];

  uint8_t stageCount;
  uint8_t currentStageIndex;

  RunState runState;

  time_t stageStartEpoch;
  uint32_t pausedRemainingSeconds;

  bool initialized;

  static constexpr const char*
    STORAGE_NAMESPACE =
      "mwprofile";

  bool loadProfile();
  bool loadRuntimeState();

  bool saveDefinition();
  bool saveRuntimeState();
  void clearRuntimeState();

  bool validateProfile(
    const String& name,
    const Stage* candidateStages,
    uint8_t candidateCount
  ) const;

  void applyCurrentStageSetpoint();
  void advanceStagesUsingClock();
  void completeProfile();
};

#endif
