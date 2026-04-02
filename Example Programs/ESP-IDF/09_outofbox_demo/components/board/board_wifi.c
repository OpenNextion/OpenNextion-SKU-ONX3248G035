


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lvgl_port.h"

#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "board_wifi.h"
static volatile signed char factory_rssi = 0x55;



static EventGroupHandle_t   wifi_event;


#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

#define STA_DEFAULT_SSID        "ESP_TEST"
/* wifi密码 */
#define STA_DEFAULT_PWD         "12345678"


/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_MAXIMUM_RETRY 5

#define DEFAULT_SCAN_LIST_SIZE 10

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

SemaphoreHandle_t wifi_scan_Semaphore = NULL;
SemaphoreHandle_t wifi_connect_Semaphore = NULL;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;




signed char wifiGetFactoryRssi(void)
{
    return factory_rssi;
}



static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int s_retry_num = 0;

     if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
       
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    }
    /* 扫描到要连接的WIFI事件 */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    /* 连接WIFI事件 */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {

    }
    /* 连接WIFI失败事件 */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        /* 尝试连接 */
        if (s_retry_num < 20)
        {
            esp_wifi_connect();
            s_retry_num ++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }

        ESP_LOGI(TAG,"connect to the AP fail");
    }
    /* 工作站从连接的AP获得IP */
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event, WIFI_CONNECTED_BIT);
    }
}







static void wifiRssiTest(void *arg)
{
    int rssi;
    while (1)
    {
        if(esp_wifi_sta_get_rssi(&rssi) == ESP_OK)
        {
            factory_rssi = rssi;
        }
        else
        {
            factory_rssi = 0x55;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void vWifiSTAMode(void)
{
    static esp_netif_t *sta_netif = NULL;

    wifi_event = xEventGroupCreate();    /* 创建一个事件标志组 */

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. 配置WiFi驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL) );
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. 设置AP参数
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "8JB0839D",
            .password ="094FAFE8",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    xTaskCreate(wifiRssiTest, "wifiRssiTest", 4096, NULL, 10, NULL);


}



