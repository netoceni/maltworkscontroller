#ifndef FIRMWAREINFO_H
#define FIRMWAREINFO_H

#include <Arduino.h>

namespace FirmwareInfo {
  extern const char PRODUCT[];
  extern const char VERSION[];
  extern const char BOARD_FAMILY[];
  extern const char BUILD_PHASE[];
  extern const char METADATA_MARKER[];

  String getMetadataJson();
}

#endif
