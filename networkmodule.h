#ifndef NETWORKMODULE_H
#define NETWORKMODULE_H

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>

class NetworkModule {
public:
  NetworkModule();

  bool begin(
    const char* accessPointName,
    const String& stationSsid,
    const String& stationPassword
  );

  void update();

  bool connectStation(
    const String& ssid,
    const String& password
  );

  void disconnectStation();

  bool completeConfiguration();

  bool isAccessPointStarted() const;
  bool isConfigurationCompletionPending() const;
  bool hasStationCredentials() const;
  bool isStationConnected() const;

  String getStationStatusText() const;
  String getStationSsid() const;

  IPAddress getAccessPointIp() const;
  IPAddress getStationIp() const;

  int32_t getStationRssi() const;
  uint8_t getConnectedAccessPointClients() const;
  bool shouldRestartForConnectivity() const;

private:
  String accessPointName;
  String stationSsid;
  String stationPassword;

  DNSServer dnsServer;

  bool accessPointStarted;
  bool captivePortalStarted;
  bool connectionPreviouslyReported;
  bool configurationCompletionPending;
  bool configurationShutdownScheduled;
  bool hasConnectedSinceBoot;

  unsigned long lastConnectionAttempt;
  unsigned long stationDisconnectedSince;
  unsigned long configurationShutdownScheduledAt;
  unsigned long lastRadioRecoveryAt;

  static constexpr unsigned long
    RECONNECT_INTERVAL_MS =
      15UL * 1000UL;

  static constexpr unsigned long
    CONFIGURATION_FALLBACK_DELAY_MS =
      30UL * 1000UL;

  static constexpr unsigned long
    CONFIGURATION_SHUTDOWN_DELAY_MS =
      5000UL;

  static constexpr unsigned long
    RADIO_RECOVERY_DELAY_MS =
      60UL * 1000UL;

  static constexpr unsigned long
    RADIO_RECOVERY_INTERVAL_MS =
      60UL * 1000UL;

  static constexpr unsigned long
    SYSTEM_RECOVERY_DELAY_MS =
      5UL * 60UL * 1000UL;

  bool startConfigurationAccessPoint();
  void stopConfigurationAccessPoint();
  void startStationConnection();
  void recoverStationRadio();
};

#endif
