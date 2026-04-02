#ifndef _APP_UI_TASK_
#define _APP_UI_TASK_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"


#define LVGL_UI_TASK_STACK_SIZE   1024
#define LVGL_UI_TASK_PRIORITY     12
#define LVGL_UI_TASK_NAME         "LVGL_UI_TASK"  

void lvgl_ui_init(void);
void guiInit(void);
#endif

