/**
 * @file gui_wifi.c
 * @brief LVGL-based WiFi GUI Implementation for ESP32
 * @details This module implements a graphical user interface for WiFi management 
 *          (scan, connect, status display) using LVGL library on ESP32 platform.
 *          It integrates with the WiFi manager module to handle WiFi scan/connection
 *          and updates the UI in real-time with scan results and connection status.
 * @version 1.0
 * @date 2025
 * @dependencies LVGL, wifi_manager.h, esp_log.h
 */

#include "gui.h"
#include "esp_log.h"
#include "wifi_manager.h"

/* LVGL Resource Declarations */
LV_IMG_DECLARE(wifi_on_28);          /**< WiFi ON icon (28x28px) */
LV_IMG_DECLARE(wifi_closed_28);      /**< WiFi OFF icon (28x28px) */
LV_IMG_DECLARE(left_handle);         /**< Back button icon */
LV_FONT_DECLARE(wifi_dropdown_icom_18px); /**< Custom font for WiFi strength icons */

/* Log tag for ESP_LOGx functions */
static const char *TAG = "GUI";

/**
 * @struct gui_wifi_t
 * @brief Container for all WiFi GUI components (UI elements)
 * @details Stores handles to LVGL objects (containers, buttons, labels, keyboard, etc.)
 *          used in the WiFi management interface.
 */
typedef struct {
    lv_obj_t *cont_main;              /**< Main container for entire WiFi UI */

    lv_obj_t *img_wifi_state;         /**< WiFi status icon (ON/OFF) */
    lv_obj_t *cont_title;             /**< Title container (WiFi label + switch) */
    lv_obj_t *label_cont_title;       /**< "WiFi" text label in title bar */
    lv_obj_t *switch_wifi_state;      /**< ON/OFF switch for WiFi control */
    lv_obj_t *list_wifi_info;         /**< List view for scanned WiFi APs */
    lv_obj_t *list_bnt[MAX_SCAN_NUM]; /**< Buttons for each scanned WiFi AP */
    lv_obj_t *spinner_wait;           /**< Loading spinner (scan/connect in progress) */
    lv_style_t style_wifi_list;       /**< Custom style for WiFi list items */

    lv_obj_t *cont_wifi_link;         /**< WiFi connection dialog (password input) */
    lv_obj_t *scroll_return;          /**< Unused scroll container for back button */
    lv_obj_t *btn_return;             /**< Back button for connection dialog */
    lv_obj_t *label_ssid;             /**< SSID display label in connection dialog */
    lv_obj_t *keyboard;               /**< On-screen keyboard for password input */
    lv_obj_t *text_pwd;               /**< Password input textarea */
    lv_obj_t *label_log;              /**< Connection status message label */
} gui_wifi_t;

/**
 * @struct wifi_info_t
 * @brief Simplified WiFi AP information for GUI display
 * @details Stores only essential data (SSID, RSSI) needed for UI rendering,
 *          stripped from the full wifi_ap_record_t structure.
 */
typedef struct {
    int8_t rssi;                      /**< Received Signal Strength Indicator (dBm) */
    char   ssid[33];                  /**< WiFi AP SSID (null-terminated, max 32 chars) */
} wifi_info_t;

/* Static Global Variables */
static gui_wifi_t  gui_wifi;          /**< Instance of WiFi GUI component container */
static wifi_info_t wifi_info[MAX_SCAN_NUM]; /**< Array of scanned WiFi AP info */

static uint8_t list_wifi_num           = 0;  /**< Number of unique WiFi APs displayed */
static uint8_t s_wifi_scan_num         = 0;  /**< Total scanned WiFi APs (raw count) */
static uint8_t s_wifi_scan_retry_count = 0;  /**< Retry counter for failed scans */
static uint8_t link_wifi_number;             /**< Index of selected WiFi AP for connection */

static SemaphoreHandle_t semaphore_wifi_scanf;    /**< Semaphore: trigger GUI list update after scan */
static SemaphoreHandle_t semaphore_up_wifi_state; /**< Semaphore: trigger WiFi status refresh */

static char     s_connected_ssid[33] = {0};   /**< SSID of currently connected WiFi AP */
static bool     s_connected_valid    = false; /**< Flag: whether connected SSID is valid */
static uint32_t up_list_time         = 0;     /**< Timer for periodic list refresh (ms) */
static bool     up_list_sign         = true;  /**< Flag: allow automatic list refresh */

/**
 * @brief Callback for closing the WiFi connection dialog
 * @param e LVGL event object (unused)
 */
static void wifi_connect_close_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_obj_add_flag(gui_wifi.cont_wifi_link, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Gesture handler for WiFi connection dialog
 * @details Hides the connection dialog when a right swipe gesture is detected
 * @param e LVGL event object (unused)
 */
static void wifi_link_swipe_cb(lv_event_t *e)
{
    static lv_point_t start_point;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(lv_indev_get_act(), &start_point);
    } 
    else if (code == LV_EVENT_PRESSING) {
        lv_point_t now;
        lv_indev_get_point(lv_indev_get_act(), &now);

        int dx = now.x - start_point.x;
        int dy = now.y - start_point.y;

        if (dx > 60 && LV_ABS(dy) < 40) {
            ESP_LOGI(TAG, "Manual right swipe detected, close password panel");
            lv_obj_add_flag(gui_wifi.cont_wifi_link, LV_OBJ_FLAG_HIDDEN);
            start_point = now;
        }
    }
}



/**
 * @brief Textarea focus event callback (show keyboard)
 * @details Triggers when password textarea is clicked: shows the on-screen keyboard
 *          and binds it to the textarea for input.
 * @param e LVGL event object (contains target textarea and keyboard user data)
 */
static void wifi_ta_event_cb(lv_event_t *e)
{
    if (e == NULL) {
        ESP_LOGW(TAG, "NULL event in wifi_ta_event_cb");
        return;
    }

    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *kb = lv_event_get_user_data(e);

    lv_keyboard_set_textarea(kb, ta);
    lv_obj_move_foreground(kb);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
}
  
/**
 * @brief Textarea defocus event callback (hide keyboard)
 * @details Triggers when password textarea loses focus: hides the on-screen keyboard.
 * @param e LVGL event object (contains keyboard user data)
 */
static void wifi_ta_defocus_cb(lv_event_t *e)
{
    if (e == NULL) {
        ESP_LOGW(TAG, "NULL event in wifi_ta_defocus_cb");
        return;
    }

    lv_obj_t *kb = lv_event_get_user_data(e);
    if (kb != NULL && lv_obj_is_valid(kb)) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Callback for WiFi connection state updates (from WiFi manager)
 * @details Updates UI to reflect connection status (success/timeout/failure),
 *          highlights connected SSID in the list, and triggers UI refresh.
 * @param status Current WiFi link state (from WifiManagerLinkState enum)
 */
static void wifi_link_state_callback(WifiManagerLinkState status)
{
    if (!lvgl_port_lock(portMAX_DELAY)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock in wifi_link_state_callback");
        return;
    }

    lv_obj_add_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(gui_wifi.label_log, LV_OBJ_FLAG_HIDDEN);

    switch (status) {
        case WIFI_MANAGER_LINK_STATE_SUCCESS: {
            ESP_LOGI(TAG, "WiFi connect success");
            lv_label_set_text(gui_wifi.label_log, "Connected");
            lv_obj_add_flag(gui_wifi.cont_wifi_link, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(gui_wifi.cont_title, LV_OBJ_FLAG_HIDDEN);

            /* Save connected SSID for highlighting in the list */
            strncpy(s_connected_ssid, wifi_info[link_wifi_number].ssid, sizeof(s_connected_ssid) - 1);
            s_connected_ssid[sizeof(s_connected_ssid) - 1] = '\0';

            s_connected_valid = true;
            up_list_sign      = true;
            up_list_time      = UP_WIFI_LIST_TIME;

            xSemaphoreGive(semaphore_up_wifi_state);
            break;
        }

        case WIFI_MANAGER_LINK_STATE_TIMEOUT: {
            ESP_LOGI(TAG, "WiFi connection timeout");
            lv_label_set_text(gui_wifi.label_log, "Connection timeout");
            s_connected_valid = false;
            break;
        }

        default: {
            ESP_LOGI(TAG, "WiFi connection failed");
            lv_label_set_text(gui_wifi.label_log, "Connection failed");
            s_connected_valid = false;
            break;
        }
    }

    lvgl_port_unlock();
}

/**
 * @brief Keyboard "Ready" event callback (password input complete)
 * @details Triggers when user presses "Ready" on the keyboard: hides keyboard/spinner,
 *          retrieves password input, and initiates WiFi connection via WiFi manager.
 * @param e LVGL event object (contains keyboard target)
 */
static void wifi_kb_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Keyboard event");

    if (e == NULL) {
        ESP_LOGW(TAG, "NULL event in wifi_kb_event_cb");
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_READY) {
        lv_obj_add_flag(gui_wifi.label_log, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(gui_wifi.keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *kb = lv_event_get_target(e);
        lv_obj_t *ta = lv_keyboard_get_textarea(kb);

        const char *password = lv_textarea_get_text(ta);
        const char *ssid     = (const char *)wifi_info[link_wifi_number].ssid;

        if (ssid == NULL) {
            ESP_LOGE(TAG, "SSID pointer is NULL");
            return;
        }

        if (password == NULL) {
            password = "";
        }

        ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

        esp_err_t ret = wifi_manager_link(ssid, (char *)password, wifi_link_state_callback);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WiFi link start failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "WiFi link start success");
        }
    }
}

/**
 * @brief Set background color for letter keys on the on-screen keyboard
 * @details Customizes the visual style of letter keys (lower/upper case mode)
 *          while leaving other keys (numbers/symbols) unchanged.
 * @param kb Pointer to LVGL keyboard object
 * @param color Target background color for letter keys
 */
static void set_letter_keys_background(lv_obj_t *kb, lv_color_t color)
{
    if (kb == NULL || !lv_obj_is_valid(kb)) {
        ESP_LOGW(TAG, "Invalid keyboard object in set_letter_keys_background");
        return;
    }

    lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);
    if (mode == LV_KEYBOARD_MODE_TEXT_LOWER || mode == LV_KEYBOARD_MODE_TEXT_UPPER) {
        lv_obj_t *btn_matrix = lv_obj_get_child(kb, 0);
        if (btn_matrix && lv_obj_is_valid(btn_matrix)) {
            lv_obj_set_style_bg_color(btn_matrix, color, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(btn_matrix, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_color(btn_matrix, lv_color_hex(0xA0A0A0), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(btn_matrix, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
        }
    }
}

/**
 * @brief Callback for WiFi list item click (open password dialog)
 * @details Triggers when a WiFi AP button is clicked: opens the connection dialog,
 *          populates the SSID label, and resets the password input field.
 * @param e LVGL event object (contains user data = WiFi AP index)
 */
static void wifi_list_item_click_cb(lv_event_t *e)
{
    int wifi_index = (int)(intptr_t)lv_event_get_user_data(e);

    if (wifi_index >= 0 && wifi_index < s_wifi_scan_num) {
        up_list_sign     = false;
        link_wifi_number = (uint8_t)wifi_index;

        lv_obj_add_flag(gui_wifi.label_log,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(gui_wifi.label_ssid, (char *)wifi_info[link_wifi_number].ssid);
        lv_textarea_set_text(gui_wifi.text_pwd, "");
        lv_obj_clear_flag(gui_wifi.cont_wifi_link, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Apply visual style to highlight connected WiFi AP in list
 * @details Sets a distinct background color for the button corresponding to
 *          the currently connected WiFi AP (blue color for active state).
 * @param btn Pointer to LVGL list button object
 */
static void wifi_list_set_connected_style(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0096FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0096FF), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0096FF), LV_PART_MAIN | LV_STATE_PRESSED);
}

/**
 * @brief FreeRTOS Task: Update WiFi AP list in GUI
 * @details Waits for scan completion semaphore, then renders scanned WiFi APs
 *          in the LVGL list (with strength icons, deduplicated SSIDs, and connected style).
 * @param data Unused task parameter
 */
static void up_gui_wifi_list(void *data)
{
    LV_UNUSED(data);

    char strength_icon[16]  = {0};  /**< Buffer for WiFi strength icon (unicode) */
    char strength_text[64]  = {0};  /**< Buffer for SSID + strength icon text */
    char added_ssids[MAX_SCAN_NUM][33] = {0}; /**< Track unique SSIDs to avoid duplicates */
    int  added_count         = 0;   /**< Count of unique SSIDs added to list */

    while (1) {
        if (xSemaphoreTake(semaphore_wifi_scanf, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Updating WiFi list in GUI");

            memset(added_ssids, 0, sizeof(added_ssids));
            added_count = 0;

            const int batch_size = 5;  /**< Batch size for UI rendering (performance optimization) */

            if (!lvgl_port_lock(portMAX_DELAY)) {
                ESP_LOGE(TAG, "Failed to lock LVGL in up_gui_wifi_list");
                continue;
            }

            if (gui_wifi.list_wifi_info == NULL || !lv_obj_is_valid(gui_wifi.list_wifi_info)) {
                ESP_LOGE(TAG, "WiFi list object invalid");
                lvgl_port_unlock();
                continue;
            }

            lv_obj_clear_flag(gui_wifi.list_wifi_info, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clean(gui_wifi.list_wifi_info);

            ESP_LOGI(TAG, "sizeof(added_ssids) = %d", (int)sizeof(added_ssids));

            WifiManagerLinkState link_state = WIFI_MANAGER_LINK_STATE_NULL;
            int rssi_value                  = 0;
            wifi_manager_get_link_state(&link_state, &rssi_value);

            /* Process scanned APs in batches to optimize LVGL rendering */
            for (int batch_start = 0; batch_start < s_wifi_scan_num; batch_start += batch_size) {
                int batch_end = batch_start + batch_size;
                if (batch_end > s_wifi_scan_num) {
                    batch_end = s_wifi_scan_num;
                }

                for (int i = batch_start; i < batch_end; i++) {
                    const char *current_scan_ssid = (char *)wifi_info[i].ssid;
                    if (current_scan_ssid[0] == '\0') {
                        continue;
                    }

                    /* Skip duplicate SSIDs */
                    bool ssid_already_added = false;
                    for (int j = 0; j < added_count; j++) {
                        if (strcmp(added_ssids[j], current_scan_ssid) == 0) {
                            ssid_already_added = true;
                            break;
                        }
                    }

                    if (ssid_already_added) {
                        continue;
                    }

                    /* Add SSID to unique list */
                    if (added_count < MAX_SCAN_NUM) {
                        ESP_LOGI(TAG, "added_count = %d", added_count);
                        strncpy(added_ssids[added_count], current_scan_ssid, sizeof(added_ssids[0]) - 1);
                        added_ssids[added_count][sizeof(added_ssids[0]) - 1] = '\0';
                        added_count++;
                    }

                    /* Determine WiFi strength icon based on RSSI */
                    int rssi = wifi_info[i].rssi;
                    if (rssi >= -50) {
                        strcpy(strength_icon, "\ue671\ue644");    /* Strong signal */
                    } else if (rssi >= -70) {
                        strcpy(strength_icon, "\ue672\ue644");    /* Medium signal */
                    } else {
                        strcpy(strength_icon, "\ue673\ue644");    /* Weak signal */
                    }

                    /* Combine icon and SSID for display */
                    snprintf(strength_text, sizeof(strength_text), "%s %s", strength_icon, current_scan_ssid);

                    /* Create list button for AP */
                    gui_wifi.list_bnt[i] = lv_list_add_btn(gui_wifi.list_wifi_info, NULL, strength_text);
                    if (gui_wifi.list_bnt[i]) {
                        lv_obj_remove_style_all(gui_wifi.list_bnt[i]);
                        lv_obj_add_style(gui_wifi.list_bnt[i], &gui_wifi.style_wifi_list, 0);
                        lv_obj_set_width(gui_wifi.list_bnt[i], lv_pct(100));
                        lv_obj_set_height(gui_wifi.list_bnt[i], 30);
        
                        lv_obj_t * bnt_label = lv_obj_get_child(gui_wifi.list_bnt[i], 0);
                        if(bnt_label != NULL)
                        {
                            lv_obj_set_height(bnt_label, 24);
                        }

                        lv_obj_add_event_cb(gui_wifi.list_bnt[i], wifi_list_item_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
                        lv_obj_set_user_data(gui_wifi.list_bnt[i], (void *)(intptr_t)i);

                        /* Highlight connected AP */
                        bool is_connected = false;
                        if (s_connected_valid && current_scan_ssid[0] != '\0' && strcmp(current_scan_ssid, s_connected_ssid) == 0) {
                            is_connected = true;
                        }

                        if (is_connected && link_state == WIFI_MANAGER_LINK_STATE_SUCCESS) {
                            wifi_list_set_connected_style(gui_wifi.list_bnt[i]);
                        }
                    }
                }
            }

            /* Update displayed AP count (capped at MAX_SCAN_NUM) */
            list_wifi_num = added_count;
            if (list_wifi_num >= MAX_SCAN_NUM) {
                list_wifi_num = MAX_SCAN_NUM;
            }
            ESP_LOGI(TAG, "list_wifi_num = %d", list_wifi_num);

            lv_obj_clear_flag(gui_wifi.list_wifi_info, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);

            lvgl_port_unlock();
        }
    }
}

/**
 * @brief Callback for WiFi scan completion (from WiFi manager)
 * @details Processes scan results: stores simplified AP info, triggers UI update,
 *          or retries scan if no APs are found (up to MAX_WIFI_SCAN_RETRY times).
 * @param status Scan result status (success/timeout/error)
 * @param scan_num Number of scanned APs
 * @param ap_info Pointer to full scanned AP records (wifi_ap_record_t array)
 */
static void wifi_scan_callback(WifiManagerScanState status, uint8_t scan_num, wifi_ap_record_t *ap_info)
{
    ESP_LOGI(TAG, "WiFi scan callback, status: %d, scan_num: %d", status, scan_num);

    if (status == WIFI_MANAGER_SCAN_STATE_SUCCESS) {
        if ((scan_num > 0) && (ap_info != NULL)) {
            /* Cap scan results at MAX_SCAN_NUM to avoid buffer overflow */
            if (scan_num > MAX_SCAN_NUM) {
                scan_num = MAX_SCAN_NUM;
            }

            s_wifi_scan_num         = scan_num;
            s_wifi_scan_retry_count = 0;

            /* Clear previous scan results */
            memset(wifi_info, 0, sizeof(wifi_info));

            /* Copy essential AP info (SSID, RSSI) to GUI-friendly structure */
            for (int8_t i = 0; i < scan_num; i++) {
                memcpy(wifi_info[i].ssid, ap_info[i].ssid, 32);
                wifi_info[i].ssid[32] = '\0';
                wifi_info[i].rssi     = ap_info[i].rssi;
                ESP_LOGI(TAG, "%d ssid = %s, rssi = %d", i, wifi_info[i].ssid, wifi_info[i].rssi);
            }

            /* Trigger GUI list update */
            xSemaphoreGive(semaphore_wifi_scanf);
        }
    } else {
        /* Retry scan if max retries not reached */
        if (s_wifi_scan_retry_count < MAX_WIFI_SCAN_RETRY) {
            s_wifi_scan_retry_count++;
            ESP_LOGI(TAG, "No WiFi APs found, retry scan (%d/%d)", s_wifi_scan_retry_count, MAX_WIFI_SCAN_RETRY);

            esp_err_t ret = wifi_manager_scan(WIFI_WAIT_SCANG_TIME, MAX_SCAN_NUM, wifi_scan_callback);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "wifi_manager_scan ERROR: %s", esp_err_to_name(ret));
            }
        } else {
            /* Max retries reached: show "No networks found" in UI */
            ESP_LOGI(TAG, "Maximum WiFi scan retries reached, no APs found");
            s_wifi_scan_retry_count = 0;

            if (gui_wifi.list_wifi_info && lv_obj_is_valid(gui_wifi.list_wifi_info)) {
                if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                    lv_obj_clean(gui_wifi.list_wifi_info);
                    lv_list_add_btn(gui_wifi.list_wifi_info, NULL, "No WiFi networks found");
                    lv_obj_clear_flag(gui_wifi.list_wifi_info, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);
                    lvgl_port_unlock();
                }
            }
        }
    }
}

/**
 * @brief Callback for WiFi ON/OFF switch toggle
 * @details Enables/disables WiFi via WiFi manager: starts scan when enabled,
 *          stops scan/hides list when disabled, and updates status icon.
 * @param e LVGL event object (contains switch target)
 */
static void wifi_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *switch_obj = lv_event_get_target(e);

    if (lv_obj_has_state(switch_obj, LV_STATE_CHECKED)) {
        /* WiFi ON: start WiFi and trigger scan */
        esp_err_t ret = wifi_manager_open();
        if (ret == ESP_OK) {
            lv_obj_clear_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);
            up_list_time = 0;

            ret = wifi_manager_scan(WIFI_WAIT_SCANG_TIME, MAX_SCAN_NUM, wifi_scan_callback);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "wifi_manager_scan ERROR: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGE(TAG, "wifi_manager_open ERROR: %s", esp_err_to_name(ret));
        }
    } else {
        /* WiFi OFF: stop WiFi and hide UI elements */
        lv_obj_add_flag(gui_wifi.list_wifi_info, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(gui_wifi.img_wifi_state, &wifi_closed_28);
        esp_wifi_scan_stop();

        esp_err_t ret = wifi_manager_close();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disconnect WiFi: %s", esp_err_to_name(ret));
        }
    }
}

/**
 * @brief FreeRTOS Task: Periodically check and update WiFi status
 * @details Monitors WiFi connection state, updates status icon (ON/OFF),
 *          and triggers automatic list refresh at regular intervals.
 * @param data Unused task parameter
 */
static void wifi_status_check(void *data)
{
    LV_UNUSED(data);

    static WifiManagerLinkState last_link_state = WIFI_MANAGER_LINK_STATE_NULL;

    while (1) {
        xSemaphoreTake(semaphore_up_wifi_state, pdMS_TO_TICKS(5000));

        /* Get current WiFi link state and RSSI */
        WifiManagerLinkState link_state = WIFI_MANAGER_LINK_STATE_NULL;
        int rssi_value = 0;
        wifi_manager_get_link_state(&link_state, &rssi_value);

        /* Trigger list refresh if state changes */
        if (link_state != last_link_state) {
            last_link_state = link_state;
            up_list_time    = UP_WIFI_LIST_TIME;
        }

        /* Update status icon (thread-safe LVGL access) */
        if (lvgl_port_lock(portMAX_DELAY)) {
            if (link_state == WIFI_MANAGER_LINK_STATE_SUCCESS) {
                lv_img_set_src(gui_wifi.img_wifi_state, &wifi_on_28);
                lv_obj_move_foreground(gui_wifi.img_wifi_state);
            } else {
                lv_img_set_src(gui_wifi.img_wifi_state, &wifi_closed_28);
            }
            lvgl_port_unlock();
        }

        /* Auto-refresh WiFi list at regular intervals */
        up_list_time += 5000;
        if ((up_list_time >= UP_WIFI_LIST_TIME) && (up_list_sign == true)) {
            up_list_time = 0;
            esp_err_t ret = wifi_manager_scan(WIFI_WAIT_SCANG_TIME, MAX_SCAN_NUM, wifi_scan_callback);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "auto wifi scan failed: %s", esp_err_to_name(ret));
            }
        }
    }
}

/**
 * @brief Initialize WiFi GUI components
 * @details Creates all LVGL UI elements (containers, buttons, list, keyboard, etc.),
 *          sets up styles and event callbacks, and initializes FreeRTOS semaphores/tasks.
 * @param parent Parent LVGL object to attach the WiFi UI to (typically screen)
 */
void wifi_gui_init(lv_obj_t *parent)
{
    /* Skip initialization if UI already exists */
    if (gui_wifi.cont_main) {
        ESP_LOGD(TAG, "WiFi window already exists");
        return;
    }

    if (parent == NULL) {
        ESP_LOGD(TAG, "parent is NULL");
        return;
    }

    /* Thread-safe LVGL access: create UI elements */
    if (lvgl_port_lock(portMAX_DELAY)) {
        /* Main container (full screen) */
        gui_wifi.cont_main = lv_obj_create(parent);
        lv_obj_set_size(gui_wifi.cont_main, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(gui_wifi.cont_main, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(gui_wifi.cont_main, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_opa(gui_wifi.cont_main, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_radius(gui_wifi.cont_main, 0, LV_PART_MAIN);

        /* WiFi status icon (top-right corner) */
        gui_wifi.img_wifi_state = lv_img_create(lv_layer_top());
        lv_img_set_src(gui_wifi.img_wifi_state, &wifi_closed_28);
        lv_obj_align(gui_wifi.img_wifi_state, LV_ALIGN_TOP_RIGHT, -5, 0);
        lv_obj_set_size(gui_wifi.img_wifi_state, 30, 30);

        /* Title container (WiFi label + switch) */
        gui_wifi.cont_title = lv_obj_create(gui_wifi.cont_main);
        lv_obj_set_size(gui_wifi.cont_title, 250, 60);
        lv_obj_set_style_radius(gui_wifi.cont_title, 10, LV_PART_MAIN);
        lv_obj_set_style_bg_color(gui_wifi.cont_title, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(gui_wifi.cont_title, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_align(gui_wifi.cont_title, LV_ALIGN_TOP_MID, 0, 50);

        gui_wifi.label_cont_title = lv_label_create(gui_wifi.cont_title);
        lv_label_set_text(gui_wifi.label_cont_title, "WiFi");
        lv_obj_align(gui_wifi.label_cont_title, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_set_style_text_font(gui_wifi.label_cont_title, &lv_font_montserrat_16, LV_PART_MAIN);

        gui_wifi.switch_wifi_state = lv_switch_create(gui_wifi.cont_title);
        lv_obj_align(gui_wifi.switch_wifi_state, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_clear_state(gui_wifi.switch_wifi_state, LV_STATE_CHECKED);
        lv_obj_add_event_cb(gui_wifi.switch_wifi_state, wifi_switch_event_cb, LV_EVENT_CLICKED, NULL);

        /* WiFi AP list view */
        gui_wifi.list_wifi_info = lv_list_create(gui_wifi.cont_main);
        lv_obj_set_size(gui_wifi.list_wifi_info, 250, 280);
        lv_obj_set_style_bg_color(gui_wifi.list_wifi_info, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_pad_row(gui_wifi.list_wifi_info, 10, 0);
        lv_obj_set_style_min_height(gui_wifi.list_wifi_info, 40, LV_PART_ITEMS);
        lv_obj_align(gui_wifi.list_wifi_info, LV_ALIGN_TOP_MID, 0, 130);
        lv_obj_add_flag(gui_wifi.list_wifi_info, LV_OBJ_FLAG_HIDDEN);

        /* Custom style for WiFi list items */
        lv_style_init(&gui_wifi.style_wifi_list);
        lv_style_set_text_font(&gui_wifi.style_wifi_list, &wifi_dropdown_icom_18px);
        lv_style_set_pad_top(&gui_wifi.style_wifi_list, 3);
        lv_style_set_pad_bottom(&gui_wifi.style_wifi_list, 0);
        lv_style_set_pad_left(&gui_wifi.style_wifi_list, 0);
        lv_style_set_pad_right(&gui_wifi.style_wifi_list, 0);

        /* Loading spinner (scan/connect in progress) */
        gui_wifi.spinner_wait = lv_spinner_create(lv_layer_top(), 1000, 200);
        lv_obj_center(gui_wifi.spinner_wait);
        lv_obj_set_size(gui_wifi.spinner_wait, 80, 80);
        lv_obj_add_flag(gui_wifi.spinner_wait, LV_OBJ_FLAG_HIDDEN);



        
        /* WiFi connection dialog (password input) */
        gui_wifi.cont_wifi_link = lv_obj_create(lv_scr_act());
        lv_obj_set_size(gui_wifi.cont_wifi_link, 320, 480);
        lv_obj_set_style_bg_color(gui_wifi.cont_wifi_link, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_bg_opa(gui_wifi.cont_wifi_link, LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(gui_wifi.cont_wifi_link, LV_OPA_0, 0);
        lv_obj_clear_flag(gui_wifi.cont_wifi_link, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(gui_wifi.cont_wifi_link, wifi_link_swipe_cb, LV_EVENT_ALL, NULL);

        /* Back button (close connection dialog) */
        gui_wifi.btn_return = lv_img_create(gui_wifi.cont_wifi_link);
        lv_img_set_src(gui_wifi.btn_return, &left_handle);
        lv_obj_align(gui_wifi.btn_return, LV_ALIGN_TOP_LEFT, 0, 25);
        lv_obj_add_flag(gui_wifi.btn_return, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(gui_wifi.btn_return, wifi_connect_close_btn_cb, LV_EVENT_CLICKED, NULL);

        /* SSID label (connection dialog) */
        gui_wifi.label_ssid = lv_label_create(gui_wifi.cont_wifi_link);
        lv_obj_align(gui_wifi.label_ssid, LV_ALIGN_TOP_MID, 0, 70);

        lv_obj_t *SSID_label2 = lv_label_create(gui_wifi.cont_wifi_link);
        lv_label_set_text(SSID_label2, "SSID:");
        lv_obj_align(SSID_label2, LV_ALIGN_TOP_MID, -100, 70);

        /* Password input container */
        lv_obj_t *ta_cont = lv_obj_create(gui_wifi.cont_wifi_link);
        lv_obj_set_size(ta_cont, 290, 40);
        lv_obj_set_layout(ta_cont, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(ta_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ta_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_flex_grow(ta_cont, 10);
        lv_obj_clear_flag(ta_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(ta_cont, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_opa(ta_cont, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
        lv_obj_align(ta_cont, LV_ALIGN_TOP_MID, 0, 100);

        lv_obj_t *lbl = lv_label_create(ta_cont);
        lv_label_set_text(lbl, "Password:");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);

        /* On-screen keyboard (password input) */
        gui_wifi.keyboard = lv_keyboard_create(gui_wifi.cont_wifi_link);
        lv_obj_set_size(gui_wifi.keyboard, 320, 180);
        lv_obj_align(gui_wifi.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
       
        //修改的颜色
        lv_obj_set_style_bg_color(gui_wifi.keyboard, lv_color_hex(0xe0e0e0), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(gui_wifi.keyboard, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);


        lv_obj_set_style_bg_color(gui_wifi.keyboard, lv_color_hex(0xD0D0D0), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(gui_wifi.keyboard, 255, LV_PART_ITEMS | LV_STATE_CHECKED);

      
        lv_obj_add_flag(gui_wifi.keyboard, LV_OBJ_FLAG_HIDDEN);
       // lv_keyboard_set_mode(gui_wifi.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_add_event_cb(gui_wifi.keyboard, wifi_kb_event_cb, LV_EVENT_READY, NULL);


        /* Password input textarea */
        gui_wifi.text_pwd = lv_textarea_create(ta_cont);
        lv_textarea_set_one_line(gui_wifi.text_pwd, true);
        lv_obj_set_flex_grow(gui_wifi.text_pwd, 1);
        lv_obj_set_style_height(gui_wifi.text_pwd, 40, LV_PART_MAIN);
        lv_obj_add_event_cb(gui_wifi.text_pwd, wifi_ta_event_cb, LV_EVENT_CLICKED, gui_wifi.keyboard);
        lv_obj_add_event_cb(gui_wifi.text_pwd, wifi_ta_defocus_cb, LV_EVENT_DEFOCUSED, gui_wifi.keyboard);


        lv_obj_add_flag(gui_wifi.text_pwd, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(gui_wifi.text_pwd, LV_DIR_HOR);
        lv_obj_set_scroll_snap_y(gui_wifi.text_pwd, LV_SCROLL_SNAP_NONE);

        /* Connection status message label */
        gui_wifi.label_log = lv_label_create(gui_wifi.cont_wifi_link);
        lv_obj_align(gui_wifi.label_log, LV_ALIGN_CENTER, 0, 50);
        lv_obj_add_flag(gui_wifi.label_log, LV_OBJ_FLAG_HIDDEN);

        /* Hide connection dialog by default */
        lv_obj_add_flag(gui_wifi.cont_wifi_link, LV_OBJ_FLAG_HIDDEN);

        lvgl_port_unlock();
    }

    /* Create FreeRTOS semaphores for inter-task communication */
    semaphore_wifi_scanf    = xSemaphoreCreateBinary();
    semaphore_up_wifi_state = xSemaphoreCreateBinary();

    /* Create FreeRTOS tasks for UI update and status monitoring */
    xTaskCreate(up_gui_wifi_list, "up_gui_wifi_list", 8192, NULL, 10, NULL);
    xTaskCreate(wifi_status_check, "wifi_status_check", 8192, NULL, 8, NULL);
}

/**
 * @brief Global GUI initialization entry point
 * @details Top-level init function: calls WiFi GUI init with main screen as parent,
 *          ensuring thread-safe LVGL access.
 */
void gui_init(void)
{
    if (lvgl_port_lock(0)) {
        wifi_gui_init(lv_scr_act());
        lvgl_port_unlock();
    }
}