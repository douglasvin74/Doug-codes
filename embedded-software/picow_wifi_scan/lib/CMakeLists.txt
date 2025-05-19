add_library(mqtt-c STATIC
    mqtt.c
    mqtt_pal.c
)

target_include_directories(mqtt-c PUBLIC ${CMAKE_CURRENT_LIST_DIR})
