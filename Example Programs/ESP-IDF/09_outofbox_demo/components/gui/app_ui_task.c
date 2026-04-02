
#include "lvgl.h"
#include "esp_log.h"
#include "board_lvgl.h" 
#include "lv_demo_widgets.h"

void  guiInit(void) {
    if(lvgl_port_lock(0))
    {

        lv_demo_widgets();
        lvgl_port_unlock();
    }   
}
