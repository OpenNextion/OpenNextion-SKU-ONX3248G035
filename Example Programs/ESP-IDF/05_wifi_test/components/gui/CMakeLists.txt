file(GLOB MAIN_SOURCES "*.c")        
file(GLOB FONT_SOURCES "font/*.c")     


list(APPEND ALL_SOURCES 
    ${MAIN_SOURCES} 
    ${FONT_SOURCES}
)

idf_component_register(
                        SRCS ${ALL_SOURCES} 
                        INCLUDE_DIRS  "font" "include" 
                        REQUIRES "esp_wifi"    "lvgl" "espressif__esp_lvgl_port" "esp_event" "freertos"   "wifi_manager" 
       )
