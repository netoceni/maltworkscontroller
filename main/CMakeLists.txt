set(APP_SOURCES
    "../MaltworksController_Fase5_2_0.cpp"
    "../alarmmodule.cpp"
    "../clockmodule.cpp"
    "../cloudmodule.cpp"
    "../controlmodule.cpp"
    "../displaymodule.cpp"
    "../eventlogmodule.cpp"
    "../firmwareinfo.cpp"
    "../historymodule.cpp"
    "../networkmodule.cpp"
    "../profilemodule.cpp"
    "../recipelibrarymodule.cpp"
    "../relaymodule.cpp"
    "../settingsmodule.cpp"
    "../setupassets.cpp"
    "../temperaturemodule.cpp"
    "../webassets.cpp"
    "../webmodule.cpp"
)

idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS ".."
    REQUIRES
        arduino-esp32
        esp_websocket_client
        esp_https_ota
        app_update
        nvs_flash
        onewire
        dallas_temperature
        adafruit_busio
        adafruit_gfx
        adafruit_ssd1306
)

# Arduino 3.3.x ainda inicializa algumas estruturas do ESP-IDF 6 sem os
# campos adicionados mais recentemente. Elas permanecem zeradas por contrato;
# evite que esse aviso de compatibilidade interrompa o build do componente.
idf_component_get_property(arduino_component espressif__arduino-esp32 COMPONENT_LIB)
target_compile_options(${arduino_component} PRIVATE -Wno-error=missing-field-initializers)
