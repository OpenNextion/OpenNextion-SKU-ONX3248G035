
/**
 * @file     main.c
 * @brief    主程序文件
 * 
 * @copyright Copyright (c) 2024 松诺技术有限公司
 */

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"

#include "app_ui_task.h"

#include "board.h"
#include "board_lcd.h"
#include "board_exio.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "board_touch.h"
#include "board_lvgl.h"

#include "esp_log.h"
#define TAG "main"
/**
 * @brief   主程序入口
 * 
 * @author  XieWeiMing (weiming.xie@itead.cc)
 * @date    2025-10-14
 */
void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    boardIIcAddDevToNum0();
    expandIoInit();
    ESP_LOGI(TAG,"firmware name :%s,ver:%s",NEXTION_FRIMWARE_NAME,NEXTION_VER);
    vTaskDelay(50);

    spiLcdInit();
    vlcdTouchInit(EXAMPLE_LCD_V_RES,EXAMPLE_LCD_H_RES); //初始化触摸
    lv_port_init();       
    spiLcdBlinit();  
    spiLcdSetBl(100);  


    // 初始化界面相关

    guiInit(); 





}


