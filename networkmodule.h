#ifndef NETWORKMODULE_H
#define NETWORKMODULE_H

#include <Arduino.h>
#include <WiFi.h>

class NetworkModule {
public:
  NetworkModule();

  bool begin(
    const char* accessPointName,
    const char* accessPointPassword,
    const String& stationSsid,
    const String& stationPassword
  );

  void update();

  bool connectStation(
    const String& ssid,
    const String& password
  );

  void disconnectStation();

  bool isAccessPointStarted() const;
  bool hasStationCredentials() const;
  bool isStationConnected() const;

  String getStationStatusText() const;
  String getStationSsid() const;

  IPAddress getAccessPointIp() const;
  IPAddress getStationIp() const;

  int32_t getStationRssi() const;
  uint8_t getConnectedAccessPointClients() const;

private:
  String stationSsid;
  String stationPassword;

  bool accessPointStarted;
  bool connectionPreviouslyReported;

  unsigned long lastConnectionAttempt;

  static constexpr unsigned long
    RECONNECT_INTERVAL_MS =
      15UL * 1000UL;

  void startStationConnection();
};

#endif
