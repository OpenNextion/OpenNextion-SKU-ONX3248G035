file(GLOB MAIN_SOURCES "app_ui_task.c")     
file(GLOB DEMO_SOURCES "widgets/*.c")   
file(GLOB ASSETS_SOURCES "widgets/assets/*.c")         
# file(GLOB IMG_SOURCES "img/*.c")       
# file(GLOB ICON_SOURCES "icon/*.c")     
# file(GLOB FONT_SOURCES "font/*.c")     


list(APPEND ALL_SOURCES 
    ${MAIN_SOURCES} 
    ${DEMO_SOURCES} 
    ${ASSETS_SOURCES} 

    # ${IMG_SOURCES} 
    # ${ICON_SOURCES} 
    # ${FONT_SOURCES}
)

idf_component_register(
                        SRCS ${ALL_SOURCES} 
                        INCLUDE_DIRS "include" "widgets"#"img" "icon" "font"
                        REQUIRES "esp_wifi" "esp_http_client" "json" "nvs_flash" "esp_lcd" "lvgl" "espressif__esp_lvgl_port" "esp_event" "freertos" "board"
       )
