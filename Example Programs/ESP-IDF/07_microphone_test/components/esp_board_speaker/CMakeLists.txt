idf_component_register(SRCS "board_speaker.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "driver" "esp_lcd_exio_pcf8574" 
                    )