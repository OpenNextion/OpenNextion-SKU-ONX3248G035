/**
 * @file wifi_manager.c
 * @brief ESP32 WiFi Manager Implementation (STA Mode Only)
 * @details This module provides a high-level WiFi management interface for ESP32,
 *          including WiFi initialization, scan for APs, connect to AP, 
 *          link state monitoring, and asynchronous task handling for scan/link operations.
 * @version 1.0
 * @date 2025
 * @note This module only supports STA (Station) mode, AP mode is not implemented.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_manager.h"



#define WIFI_EVT_STA_STARTED    (1 << 0)
#define WIFI_EVT_STA_STOPPED    (1 << 1)
#define WIFI_EVT_SCAN_DONE      (1 << 2)
#define WIFI_EVT_SCAN_TIMEOUT   (1 << 3)
#define WIFI_EVT_LINK_GOT_IP    (1 << 4)
#define WIFI_EVT_LINK_TIMEOUT   (1 << 5)
#define WIFI_EVT_LINK_DISCONN   (1 << 6)


/* -------------------------- Enum Definitions ---------------------------- */
/**
 * @enum WifiManagerState
 * @brief WiFi Manager core operating state
 */
typedef enum {
    WIFI_MANAGER_STATE_UNINITIALIZED,  /**< WiFi manager not initialized */
    WIFI_MANAGER_STATE_OFF,            /**< WiFi STA interface is off */
    WIFI_MANAGER_STATE_ON              /**< WiFi STA interface is on */
} WifiManagerState;

/* -------------------------- Static Constants ---------------------------- */
/**
 * @brief Tag for ESP_LOGx functions (WiFi Manager module)
 */
static const char *TAG = "WIFI_MANAGER";

/* -------------------------- Static Variables ---------------------------- */
/**
 * @brief Binary semaphore to trigger WiFi scan task
 */
SemaphoreHandle_t wifi_scan_start_semaphore = NULL;

/**
 * @brief Binary semaphore to trigger WiFi link (connect) task
 */
SemaphoreHandle_t wifi_link_start_semaphore = NULL;

/**
 * @brief Maximum allowed scan time (in milliseconds)
 */
static uint16_t wifi_scan_time;

/**
 * @brief Maximum number of APs to scan and store
 */
static uint8_t wifi_max_scan_num;

/**
 * @brief Buffer to store target WiFi password (max 64 chars + null terminator)
 */
static char s_wifi_password[65] = {0};

/**
 * @brief Buffer to store target WiFi SSID (max 32 chars + null terminator)
 */
static char s_wifi_ssid[33] = {0};

/**
 * @brief Callback function pointer for scan completion/error/timeout
 */
static wifiManagerScanCallback scan_callback = NULL;

/**
 * @brief Callback function pointer for link state changes
 * @note Typo in original name (LinkStare -> LinkState) - preserved for compatibility
 */
static wifiManagerLinkStareCallback link_callback = NULL;

/**
 * @brief Current WiFi link connection state
 */
static WifiManagerLinkState s_link_state = WIFI_MANAGER_LINK_STATE_NULL;

/**
 * @brief Current WiFi manager core state
 */
static WifiManagerState s_wifi_state = WIFI_MANAGER_STATE_UNINITIALIZED;

/**
 * @brief Buffer to store scanned AP records (max MAX_SCAN_NUM + 1 entries)
 */
static wifi_ap_record_t s_ap_info[MAX_SCAN_NUM + 1];

/**
 * @brief FreeRTOS event group for WiFi event bit management
 */
static EventGroupHandle_t wifi_event;

/**
 * @brief Initialization flag (true = manager initialized, false = not initialized)
 */
static bool wifi_manager_init_sign = false;

/* -------------------------- Public Function Implementations ------------ */
/**
 * @brief Get current WiFi link state and RSSI (Received Signal Strength Indicator)
 * 
 * @param[out] output_state Pointer to store current link state
 * @param[out] output_rssi Pointer to store current RSSI value (in dBm)
 * @return esp_err_t ESP_OK on success, ESP_FAIL if input pointers are NULL
 */
esp_err_t wifi_manager_get_link_state(WifiManagerLinkState *output_state, int *output_rssi)
{
    if (output_state == NULL || output_rssi == NULL) {
        return ESP_FAIL;
    }

    *output_state = s_link_state;
    return esp_wifi_sta_get_rssi(output_rssi);
}

/* -------------------------- Static Function Implementations ------------ */
/**
 * @brief Global WiFi event handler for processing WiFi/IP events
 * 
 * @param arg Unused user argument
 * @param event_base Event base (WIFI_EVENT or IP_EVENT)
 * @param event_id Specific event ID (e.g., WIFI_EVENT_STA_START, IP_EVENT_STA_GOT_IP)
 * @param event_data Pointer to event-specific data structure
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_link_state = WIFI_MANAGER_LINK_STATE_ON;
        xEventGroupSetBits(wifi_event, WIFI_EVT_STA_STARTED);
        xEventGroupClearBits(wifi_event, WIFI_EVT_STA_STOPPED);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnected from AP, reason: %d", event->reason);

        xEventGroupSetBits(wifi_event, WIFI_EVT_LINK_DISCONN);
        xEventGroupClearBits(wifi_event, WIFI_EVT_LINK_GOT_IP);

        // Stop auto-reconnect on authentication failure
        if (event->reason == WIFI_REASON_AUTH_FAIL ||
            event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
            ESP_LOGI(TAG, "Authentication failed, stop reconnect");
        }

        s_link_state = WIFI_MANAGER_LINK_STATE_DISCONNECTED;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "WiFi scan completed");
        xEventGroupSetBits(wifi_event, WIFI_EVT_SCAN_DONE);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_link_state = WIFI_MANAGER_LINK_STATE_SUCCESS;
        xEventGroupSetBits(wifi_event, WIFI_EVT_LINK_GOT_IP);
    }
}

/**
 * @brief Asynchronous WiFi scan task (FreeRTOS task)
 * 
 * @details Waits for scan trigger semaphore, performs AP scan, 
 *          retrieves scan results, and calls scan callback with results/error.
 * @param arg Unused task argument
 */
static void wifi_scan_task(void *arg)
{
    uint16_t scan_num = 0;
    uint16_t real_scan_num = 0;

    while (1) {
        // Wait indefinitely for scan trigger semaphore
        if (xSemaphoreTake(wifi_scan_start_semaphore, portMAX_DELAY) == pdTRUE) {
            xEventGroupClearBits(wifi_event, WIFI_EVT_SCAN_DONE | WIFI_EVT_SCAN_TIMEOUT);

            // Abort scan if WiFi is not in ON state
            if (s_wifi_state != WIFI_MANAGER_STATE_ON) {
                ESP_LOGW(TAG, "WiFi not started, scan aborted");
                scan_callback(WIFI_MANAGER_SCAN_STATE_ERROR, 0, NULL);
                continue;
            }

            // Wait for scan completion or timeout
            EventBits_t bits = xEventGroupWaitBits(
                wifi_event,
                WIFI_EVT_SCAN_DONE,
                pdTRUE,          // Clear bit on exit
                pdFALSE,         // Wait for any bit (not all)
                pdMS_TO_TICKS(wifi_scan_time)
            );

            if (bits & WIFI_EVT_SCAN_DONE) {
                real_scan_num = wifi_max_scan_num;
                memset(s_ap_info, 0, sizeof(s_ap_info));

                // Get number of scanned APs
                esp_err_t err = esp_wifi_scan_get_ap_num(&scan_num);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(err));
                    scan_callback(WIFI_MANAGER_SCAN_STATE_ERROR, 0, NULL);
                    continue;
                }

                // Limit results to max scan number
                if (scan_num < real_scan_num) {
                    real_scan_num = scan_num;
                }

                // Get scanned AP records
                err = esp_wifi_scan_get_ap_records(&real_scan_num, s_ap_info);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(err));
                    scan_callback(WIFI_MANAGER_SCAN_STATE_ERROR, 0, NULL);
                    continue;
                }

                // Call callback with successful scan results
                scan_callback(WIFI_MANAGER_SCAN_STATE_SUCCESS, real_scan_num, s_ap_info);
            }
            else {
                // Call callback with timeout status
                scan_callback(WIFI_MANAGER_SCAN_STATE_TIMEOUT, scan_num, NULL);
            }
        }
    }
}

/**
 * @brief Asynchronous WiFi link (connect) task (FreeRTOS task)
 * 
 * @details Waits for link trigger semaphore, disconnects existing connection (if any),
 *          configures WiFi credentials, attempts to connect, and calls link callback
 *          with connection state (success/timeout/disconnected).
 * @param arg Unused task argument
 */
static void wifi_link_task(void *arg)
{
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Default to WPA2 PSK
        },
    };

    while (1) {
        // Wait indefinitely for link trigger semaphore
        if (xSemaphoreTake(wifi_link_start_semaphore, portMAX_DELAY) == pdTRUE) {
            xEventGroupClearBits(wifi_event,
                                 WIFI_EVT_LINK_GOT_IP |
                                 WIFI_EVT_LINK_TIMEOUT |
                                 WIFI_EVT_LINK_DISCONN);

            // Disconnect existing connection if connected
            if (s_link_state == WIFI_MANAGER_LINK_STATE_SUCCESS) {
                ESP_LOGI(TAG, "Disconnecting previous WiFi");
                esp_err_t dret = esp_wifi_disconnect();
                if (dret == ESP_OK) {
                    // Wait for disconnect event (max 3 seconds)
                    xEventGroupWaitBits(
                        wifi_event,
                        WIFI_EVT_LINK_DISCONN,
                        pdTRUE,
                        pdFALSE,
                        pdMS_TO_TICKS(3000)
                    );
                }
            }

            // Start WiFi if not already on
            if (s_wifi_state != WIFI_MANAGER_STATE_ON) {
                esp_err_t ret = wifi_manager_open();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to start WiFi");
                    s_link_state = WIFI_MANAGER_LINK_STATE_DISCONNECTED;
                    link_callback(s_link_state);
                    continue;
                }
            }

            // Copy WiFi credentials to config structure
            strcpy((char *)wifi_config.sta.ssid, s_wifi_ssid);
            strcpy((char *)wifi_config.sta.password, s_wifi_password);

            ESP_LOGI(TAG, "Connecting to SSID:%s ",wifi_config.sta.ssid);
                     

            // Apply WiFi configuration and initiate connection
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            esp_wifi_connect();
            s_link_state = WIFI_MANAGER_LINK_STATE_ON;

            // Wait for connection success or timeout
            EventBits_t bits = xEventGroupWaitBits(
                wifi_event,
                WIFI_EVT_LINK_GOT_IP,
                pdTRUE,          // Clear bit on exit
                pdFALSE,         // Wait for any bit (not all)
                pdMS_TO_TICKS(LINK_MAX_TIME)
            );

            // Handle connection results
            if (bits & WIFI_EVT_LINK_GOT_IP) {
                s_link_state = WIFI_MANAGER_LINK_STATE_SUCCESS;
                link_callback(s_link_state);
            }
            else if (bits & WIFI_EVT_LINK_DISCONN) {
                s_link_state = WIFI_MANAGER_LINK_STATE_DISCONNECTED;
                link_callback(s_link_state);
            }
            else {
                s_link_state = WIFI_MANAGER_LINK_STATE_TIMEOUT;
                xEventGroupSetBits(wifi_event, WIFI_EVT_LINK_TIMEOUT);
                link_callback(s_link_state);
            }
        }
    }
}

/**
 * @brief Initialize WiFi Manager module
 * 
 * @details Initializes FreeRTOS semaphores/event groups, ESP32 WiFi stack,
 *          registers event handlers, creates scan/link tasks, and sets initial state.
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_manager_init(void)
{
    // Return success if already initialized
    if (wifi_manager_init_sign) {
        ESP_LOGI(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    // Create binary semaphores for scan/link tasks
    wifi_scan_start_semaphore = xSemaphoreCreateBinary();
    wifi_link_start_semaphore = xSemaphoreCreateBinary();
    // Create event group for WiFi event bits
    wifi_event = xEventGroupCreate();

    // Initialize network interface and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Create default WiFi STA network interface
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif); // Ensure STA interface creation succeeded

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    // Register WiFi/IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set WiFi mode to STA (Station)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set initial WiFi state to OFF
    s_wifi_state = WIFI_MANAGER_STATE_OFF;

    // Create FreeRTOS tasks for scan and link operations
    // Stack size: 10KB, Priority: 3
    xTaskCreate(wifi_scan_task, "wifi_scan_task", 1024 * 10, NULL, 3, NULL);
    xTaskCreate(wifi_link_task, "wifi_link_task", 1024 * 10, NULL, 3, NULL);

    // Mark manager as initialized
    wifi_manager_init_sign = true;
    return ESP_OK;
}

/**
 * @brief Turn on WiFi STA interface
 * 
 * @details Starts the ESP32 WiFi STA interface and waits for start confirmation.
 * @return esp_err_t ESP_OK on success, ESP_FAIL on timeout/error, 
 *         ESP_ERR_INVALID_STATE if uninitialized, ESP_FAIL if not initialized
 */
esp_err_t wifi_manager_open(void)
{
    // Check initialization status
    if (!wifi_manager_init_sign) {
        return ESP_FAIL;
    }

    // Check valid state
    if (s_wifi_state == WIFI_MANAGER_STATE_UNINITIALIZED) {
        return ESP_ERR_INVALID_STATE;
    }

    // Return success if already ON
    if (s_wifi_state == WIFI_MANAGER_STATE_ON) {
        return ESP_OK;
    }

    // Clear start event bit before starting
    xEventGroupClearBits(wifi_event, WIFI_EVT_STA_STARTED);

    // Start WiFi STA interface
    esp_err_t ret = esp_wifi_start();
    if (ret == ESP_OK) {
        // Wait for STA start event (max 2 seconds)
        EventBits_t bits = xEventGroupWaitBits(
            wifi_event,
            WIFI_EVT_STA_STARTED,
            pdTRUE,          // Clear bit on exit
            pdFALSE,         // Wait for any bit (not all)
            pdMS_TO_TICKS(2000)
        );

        // Update state if start succeeded
        if (bits & WIFI_EVT_STA_STARTED) {
            s_wifi_state = WIFI_MANAGER_STATE_ON;
        }
        else {
            ESP_LOGW(TAG, "WiFi start timeout");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

/**
 * @brief Turn off WiFi STA interface
 * 
 * @details Stops the ESP32 WiFi STA interface and updates manager state.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if uninitialized, 
 *         ESP_FAIL if not initialized
 */
esp_err_t wifi_manager_close(void)
{
    // Check initialization status
    if (!wifi_manager_init_sign) {
        return ESP_FAIL;
    }

    // Check valid state
    if (s_wifi_state == WIFI_MANAGER_STATE_UNINITIALIZED) {
        return ESP_ERR_INVALID_STATE;
    }

    // Return success if already OFF
    if (s_wifi_state == WIFI_MANAGER_STATE_OFF) {
        return ESP_OK;
    }

    // Stop WiFi STA interface
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        s_wifi_state = WIFI_MANAGER_STATE_OFF;
    }

    return ret;
}

/**
 * @brief Start asynchronous WiFi AP scan
 * 
 * @details Configures scan parameters, triggers scan operation, and uses callback
 *          to return scan results/error/timeout.
 * @param[in] max_scan_time Maximum scan duration (milliseconds)
 * @param[in] max_scan_num Maximum number of AP records to retrieve
 * @param[in] callback Callback function to receive scan results
 * @return esp_err_t ESP_OK on success, ESP_FAIL if uninitialized/callback NULL,
 *         ESP_ERR_WIFI_NOT_STARTED if WiFi is not ON
 */
esp_err_t wifi_manager_scan(uint16_t max_scan_time, uint16_t max_scan_num,
                          wifiManagerScanCallback callback)
{
    // Check initialization status
    if (!wifi_manager_init_sign) {
        return ESP_FAIL;
    }

    // Check if WiFi is ON (required for scan)
    if (s_wifi_state != WIFI_MANAGER_STATE_ON) {
        ESP_LOGE(TAG, "WiFi not started, cannot scan");
        return ESP_ERR_WIFI_NOT_STARTED;
    }

    // Validate callback
    if (callback == NULL) {
        return ESP_FAIL;
    }

    // Set scan parameters
    wifi_scan_time = max_scan_time;
    wifi_max_scan_num = (max_scan_num >= MAX_SCAN_NUM) ? MAX_SCAN_NUM : max_scan_num;
    scan_callback = callback;

    // Stop any ongoing scan
    esp_wifi_scan_stop();

    // Configure scan settings (active scan, show hidden APs)
    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    // Start scan (non-blocking)
    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret != ESP_OK) {
        return ret;
    }

    // Clear scan event bits and trigger scan task
    xEventGroupClearBits(wifi_event, WIFI_EVT_SCAN_DONE | WIFI_EVT_SCAN_TIMEOUT);
    xSemaphoreGive(wifi_scan_start_semaphore);

    return ret;
}

/**
 * @brief Start asynchronous WiFi connection to target AP
 * 
 * @details Stores WiFi credentials, triggers link task, and uses callback
 *          to return connection state (success/timeout/disconnected).
 * @param[in] wifi_ssid Target WiFi SSID (null-terminated string)
 * @param[in] wifi_password Target WiFi password (null-terminated string)
 * @param[in] callback Callback function to receive link state updates
 * @return esp_err_t ESP_OK on success, ESP_FAIL if any input is NULL/uninitialized
 */
esp_err_t wifi_manager_link(char *wifi_ssid, char *wifi_password,
                          wifiManagerLinkStareCallback callback)
{
    // Check initialization status
    if (!wifi_manager_init_sign) {
        return ESP_FAIL;
    }

    // Validate input parameters
    if (wifi_ssid == NULL || wifi_password == NULL || callback == NULL) {
        return ESP_FAIL;
    }

    // Store WiFi credentials
    strcpy(s_wifi_password, wifi_password);
    strcpy(s_wifi_ssid, wifi_ssid);

    // Set callback and trigger link task
    link_callback = callback;
    xSemaphoreGive(wifi_link_start_semaphore);

    return ESP_OK;
}