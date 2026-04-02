file(GLOB MAIN_SOURCES "*.c")

file(GLOB ASSET_SOURCES "assets/*.c")

file(GLOB ASSET_HEADERS "assets/*.h")


list(APPEND ALL_SOURCES 
    ${MAIN_SOURCES} 
    ${ASSET_SOURCES}
)

idf_component_register(
    SRCS ${ALL_SOURCES} 
    INCLUDE_DIRS  "include"  "assets" 
    REQUIRES  "lvgl__lvgl" "espressif__esp_lvgl_port" "chmorgan__esp-file-iterator" "chmorgan__esp-audio-player" "esp_board_speaker" "esp_ting_mp3"
)