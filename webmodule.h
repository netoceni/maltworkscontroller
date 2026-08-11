#ifndef WEBMODULE_H
#define WEBMODULE_H

#include <Arduino.h>
#include <WebServer.h>

#include "temperaturemodule.h"
#include "relaymodule.h"
#include "controlmodule.h"
#include "historymodule.h"
#include "networkmodule.h"
#include "settingsmodule.h"
#include "clockmodule.h"
#include "firmwareinfo.h"
#include "profilemodule.h"
#include "recipelibrarymodule.h"
#include "alarmmodule.h"
#include "cloudmodule.h"
#include "eventlogmodule.h"

class WebModule {
public:
  WebModule(
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
  );

  bool begin();
  void update();

  bool isFirmwareUpdateInProgress() const;

  IPAddress getIpAddress() const;

private:
  WebServer server;

  TemperatureModule& temperatures;
  RelayModule& relays;
  ControlModule& control;
  HistoryModule& history;
  NetworkModule& network;
  SettingsModule& settings;
  ClockModule& clock;
  ProfileModule& profile;
  RecipeLibraryModule& recipeLibrary;
  AlarmModule& alarms;
  EventLogModule& eventLog;
  CloudModule& cloud;

  bool initialized;
  bool firmwareUpdateInProgress;
  bool firmwareUpdateSucceeded;
  bool restartScheduled;

  String firmwareUpdateError;

  unsigned long restartScheduledAt;
  size_t firmwareExpectedSize;
  size_t firmwareReceivedSize;

  static constexpr unsigned long
    RESTART_DELAY_MS =
      1500UL;

  void configureRoutes();

  void handleRoot();
  void handleDashboard();
  void handleSetup();
  void handleSave();
  void handleWifiSave();
  void handleWifiForget();
  void handleSetupComplete();
  void handleFirmwarePreflight();
  void handleFirmwareUpdateEnd();
  void handleFirmwareUpload();
  void handleProfileSave();
  void handleProfileStart();
  void handleProfilePause();
  void handleProfileResume();
  void handleProfileCancel();
  void handleRecipeSave();
  void handleRecipeLoad();
  void handleRecipeStart();
  void handleRecipeDelete();
  void handleAlarmSave();
  void handleAlarmAcknowledge();
  void handleEventLogApi();
  void handleEventLogClear();
  void handleCalibrationSave();
  void handleCalibrationReset();
  void handleCloudSave();
  void handleCloudSync();
  void handleCloudTokenRegenerate();
  void handleApi();
  void handleHistoryApi();
  void handleNotFound();

  String buildApiJson() const;
  String buildHistoryJson() const;
  String buildProfileJson() const;
  String buildRecipeLibraryJson() const;
  String buildAlarmJson() const;
  String buildEventLogJson() const;
  String buildCloudJson() const;

  String temperatureToString(
    float temperature,
    unsigned int decimalPlaces = 1
  ) const;

  String jsonEscape(
    const String& text
  ) const;
};

#endif
