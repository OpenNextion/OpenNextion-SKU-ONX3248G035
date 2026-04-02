#ifndef _WIFI_MANAGER_INCLUDE_WIFI_MANAGER_H_
#define _WIFI_MANAGER_INCLUDE_WIFI_MANAGER_H_

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"

#define  MAX_SCAN_NUM   20
#define  LINK_MAX_TIME  20000

typedef enum {
    WIFI_MANAGER_LINK_STATE_SUCCESS = 0,
    WIFI_MANAGER_LINK_STATE_ON,
    WIFI_MANAGER_LINK_STATE_DISCONNECTED,
    WIFI_MANAGER_LINK_STATE_TIMEOUT,
    WIFI_MANAGER_LINK_STATE_NULL,
} WifiManagerLinkState;



typedef enum {
    WIFI_MANAGER_SCAN_STATE_SUCCESS = 0,
    WIFI_MANAGER_SCAN_STATE_ERROR ,
    WIFI_MANAGER_SCAN_STATE_TIMEOUT
} WifiManagerScanState;




/**
 * @brief Callback function type for WiFi scan results
 * 
 * @param status The current status of the WiFi scanning process
 * @param scan_num Number of WiFi networks discovered during the scan
 * @param ap_info Pointer to an array of WiFi access point records containing 
 *                detailed information about each discovered network
 */
typedef void (*wifiManagerScanCallback)(WifiManagerScanState status,uint8_t scan_num, wifi_ap_record_t *ap_info);


/**
 * @brief Callback function type for WiFi connection status updates
 * 
 * @param status The current connection state of the WiFi link
 */
typedef void (*wifiManagerLinkStareCallback)(WifiManagerLinkState status);



/**
 * @brief Initializes the WiFi module and related functionalities
 * 
 * This function initializes the WiFi driver, configures necessary parameters,
 * and prepares the WiFi module for operation. It must be called before
 * using any other WiFi management functions.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);


/**
 * @brief Turns on the WiFi functionality
 * 
 * This function activates the WiFi module, starts the WiFi stack,
 * and begins the connection process according to the configured settings.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_open(void);

/**
 * @brief Turns off the WiFi functionality
 * 
 * This function deactivates the WiFi module, stops any active connections,
 * and powers down the WiFi hardware to conserve energy.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_close(void);



/**
 * @brief Initiates a WiFi network scan
 * 
 * This function starts scanning for available WiFi networks with specified
 * time and quantity limits. The scan results are delivered asynchronously
 * through the provided callback function.
 * 
 * @param scan_time Maximum duration for the scan operation (in milliseconds)
 * @param max_scan_num Maximum number of WiFi networks to scan for
 * @param callback Callback function to handle scan results
 * 
 * @return esp_err_t ESP_OK on success, appropriate error code on failure
 */
esp_err_t wifi_manager_scan(uint16_t max_scan_time,uint16_t max_scan_num,wifiManagerScanCallback callback);


/**
 * @brief Connects to a specified WiFi network
 * 
 * This function attempts to establish a connection to a WiFi network
 * using the provided SSID and password. The connection progress and
 * final status are reported asynchronously through the callback function.
 * 
 * @param wifi_ssid The SSID/name of the WiFi network to connect to
 * @param wifi_password The password for the WiFi network
 * @param callback Callback function to report connection status
 * 
 * @return esp_err_t ESP_OK on success, appropriate error code on failure
 */
esp_err_t wifi_manager_link(char *wifi_ssid,char *wifi_password,wifiManagerLinkStareCallback callback);


/**
 * @brief Retrieves the current WiFi connection state and RSSI value
 * 
 * This function queries and returns the current connection status
 * and received signal strength indicator (RSSI) of the WiFi connection.
 * 
 * @param output_state Pointer to get the current WiFi connection state
 * @param output_rssi Pointer to get the current RSSI value
 * 
 * @return esp_err_t ESP_OK on success, appropriate error code on failure
 */
esp_err_t wifi_manager_get_link_state(WifiManagerLinkState *output_state, int *output_rssi );



#endif /* _WIFI_MANAGER_INCLUDE_WIFI_MANAGER_H_ */


