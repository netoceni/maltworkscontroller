#include "firmwareinfo.h"

namespace FirmwareInfo {
  const char PRODUCT[] =
    "MaltworksController";

  const char VERSION[] =
    "5.5.1";

  const char BOARD_FAMILY[] =
    "ESP32";

  const char BUILD_PHASE[] =
    "Fase5_5_1";

  /*
    Este texto fica incorporado ao arquivo .bin.

    A interface web procura o marcador abaixo
    antes de permitir a atualizacao OTA.
  */
  /*
    used impede que o linker remova o marcador,
    mesmo que nenhuma funcao o leia diretamente.
  */
  const char METADATA_MARKER[]
    PROGMEM
    __attribute__((used)) =
      "MALTWORKS_FW_META:"
      "{\"product\":\"MaltworksController\","
      "\"version\":\"5.5.1\","
      "\"boardFamily\":\"ESP32\","
      "\"phase\":\"Fase5_5_1\"}"
      ":MALTWORKS_FW_META_END";

  /*
    A referencia volatil cria uma dependencia real da funcao que publica os
    metadados para o marcador. Isso impede que o linker descarte o texto
    embarcado ao eliminar secoes nao referenciadas.
  */
  const char* volatile METADATA_MARKER_REFERENCE =
    METADATA_MARKER;

  String getMetadataJson() {
    String json;

    json.reserve(150);

    json += "{";
    json += "\"product\":\"";
    json += PRODUCT;
    json += "\"";

    json += ",\"version\":\"";
    json += VERSION;
    json += "\"";

    json += ",\"boardFamily\":\"";
    json += BOARD_FAMILY;
    json += "\"";

    json += ",\"phase\":\"";
    json += BUILD_PHASE;
    json += "\"";

    json += "}";

    /*
      strlen_P percorre o marcador completo.
      Como esta funcao e chamada pelo firmware,
      o linker nao pode remover o bloco.
    */
    const char* marker =
      METADATA_MARKER_REFERENCE;

    volatile size_t markerLength =
      strlen_P(marker);

    (void)markerLength;

    return json;
  }
}
