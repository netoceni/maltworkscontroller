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
        nvs_flash
        onewire
        dallas_temperature
        adafruit_busio
        adafruit_gfx
        adafruit_ssd1306
)
