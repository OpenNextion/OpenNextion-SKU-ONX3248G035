/**
 * @file gui.h
 * @brief Header file for LVGL-based GUI initialization and WiFi UI configuration
 * @details This header declares the global GUI initialization function and defines
 *          configuration macros for WiFi UI behavior (scan timeout, retry limits, auto-refresh interval).
 * @version 1.0
 * @date 2025
 * @dependencies LVGL (lvgl.h), ESP32 LVGL Port (esp_lvgl_port.h)
 */
#pragma once

/* Include Dependencies */
#include "lvgl.h"               /**< LVGL core library header */
#include "esp_lvgl_port.h"      /**< ESP32 LVGL porting layer header */

/* C++ Name Mangling Protection */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global GUI initialization entry point
 * @details Initializes all LVGL-based GUI components (primarily WiFi management UI),
 *          creates necessary FreeRTOS tasks and synchronization primitives,
 *          and sets up initial UI state.
 * @note This function must be called after LVGL is initialized via esp_lvgl_port_init()
 * @return None
 */
void  gui_init(void);

/* -------------------------- WiFi UI Configuration Macros -------------------------- */
/**
 * @def WIFI_WAIT_SCANG_TIME
 * @brief Maximum wait time for WiFi scan completion (milliseconds)
 * @note Typo in name (SCANG -> SCAN) - preserved for backward compatibility
 * @value 5000 (5 seconds)
 */
#define  WIFI_WAIT_SCANG_TIME  5000

/**
 * @def MAX_WIFI_SCAN_RETRY
 * @brief Maximum number of retry attempts for failed WiFi scans
 * @details If no APs are found in a scan, the system will retry up to this number
 *          before showing "No WiFi networks found" in the UI.
 * @value 3 (3 retries)
 */
#define  MAX_WIFI_SCAN_RETRY   3

/**
 * @def UP_WIFI_LIST_TIME
 * @brief Interval for automatic WiFi list refresh (milliseconds)
 * @details The WiFi AP list in the UI will be re-scanned and updated at this interval
 *          when WiFi is connected and auto-refresh is enabled.
 * @value 30000 (30 seconds)
 */
#define  UP_WIFI_LIST_TIME     30000

#ifdef __cplusplus
}
#endif