#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"

// Keep the same direction macros as the other display examples. They are
// translated to Arduino_GFX rotation values in displayRotationFromConfig().
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;
static const uint8_t MAX_WIFI_ITEMS = 20;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static lv_obj_t *statusLabel = nullptr;
static lv_obj_t *wifiList = nullptr;
static lv_obj_t *connectPanel = nullptr;
static lv_obj_t *ssidLabel = nullptr;
static lv_obj_t *passwordTa = nullptr;
static lv_obj_t *keyboard = nullptr;
static String wifiSsids[MAX_WIFI_ITEMS];
static String selectedSsid;

static uint8_t displayRotationFromConfig() {
  // Arduino_GFX rotation IDs include both rotation and mirror combinations.
  bool gfxMirrorX = !DISPLAY_MIRROR_X;
  bool gfxMirrorY = DISPLAY_MIRROR_Y;
  if (!DISPLAY_SWAP_XY && !gfxMirrorX && !gfxMirrorY) return 0;
  if (DISPLAY_SWAP_XY && gfxMirrorX && !gfxMirrorY) return 1;
  if (!DISPLAY_SWAP_XY && gfxMirrorX && gfxMirrorY) return 2;
  if (DISPLAY_SWAP_XY && !gfxMirrorX && gfxMirrorY) return 3;
  if (!DISPLAY_SWAP_XY && gfxMirrorX && !gfxMirrorY) return 4;
  if (DISPLAY_SWAP_XY && gfxMirrorX && gfxMirrorY) return 5;
  if (!DISPLAY_SWAP_XY && !gfxMirrorX && gfxMirrorY) return 6;
  return 7;
}

static void lcdResetByExpander() {
  if (!ioExpander.begin(PCF8574_ADDR, &Wire)) {
    Serial.println("PCF8574 init failed");
    return;
  }
  // LCD reset is routed through PCF8574 on this board.
  ioExpander.digitalWriteByte(0xFF);
  delay(20);
  ioExpander.digitalWriteByte(0xFF & ~(1 << PCF8574_PIN_LCD_RST));
  delay(120);
  ioExpander.digitalWriteByte(0xFF);
  delay(120);
}

static void lvglFlush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorP) {
  uint32_t width = area->x2 - area->x1 + 1;
  uint32_t height = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
  // Match LVGL's color byte order to the Arduino_GFX transfer API.
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&colorP->full, width, height);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&colorP->full, width, height);
#endif
  lv_disp_flush_ready(drv);
}

static void lvglTouchRead(lv_indev_drv_t *, lv_indev_data_t *data) {
  static int16_t lastX = 0;
  static int16_t lastY = 0;
  if (touch.touched()) {
    CST_TS_Point point = touch.getPoint();
    // Display rotation already aligns CST826 coordinates with LVGL.
    lastX = constrain(point.x, 0, (int16_t)gfx->width() - 1);
    lastY = constrain(point.y, 0, (int16_t)gfx->height() - 1);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  data->point.x = lastX;
  data->point.y = lastY;
}

static void lvglBegin() {
  // LVGL owns the complete WiFi UI, including the scroll list, modal panel,
  // password textarea, and on-screen keyboard.
  lv_init();
  lv_disp_draw_buf_init(&drawBuf, dispBuf, nullptr, LCD_H_RES * LVGL_BUF_LINES);
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = gfx->width();
  dispDrv.ver_res = gfx->height();
  dispDrv.flush_cb = lvglFlush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);
  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = lvglTouchRead;
  lv_indev_drv_register(&indevDrv);
}

static void boardBegin() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nONX3248G035 Arduino 05_wifi_test");
  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();
  gfx->begin(LCD_SPI_FREQ);
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);
  touch.begin(&Wire, CST826_ADDR);
  lvglBegin();
}

static const char *authText(wifi_auth_mode_t auth) {
  return auth == WIFI_AUTH_OPEN ? "open" : "secure";
}

static void showPanel(bool show) {
  if (show) lv_obj_clear_flag(connectPanel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(connectPanel, LV_OBJ_FLAG_HIDDEN);
}

static void wifiItemCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  String *ssid = static_cast<String *>(lv_event_get_user_data(e));
  selectedSsid = ssid ? *ssid : "";
  lv_label_set_text(ssidLabel, selectedSsid.c_str());
  lv_textarea_set_text(passwordTa, "");
  lv_label_set_text(statusLabel, "Enter password and press Connect.");
  showPanel(true);
}

static void connectCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *password = lv_textarea_get_text(passwordTa);
  lv_label_set_text_fmt(statusLabel, "Connecting to %s ...", selectedSsid.c_str());
  WiFi.begin(selectedSsid.c_str(), password);
}

static void closePanelCb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) showPanel(false);
}

static void scanWifi() {
  lv_label_set_text(statusLabel, "Scanning WiFi...");
  lv_obj_clean(wifiList);
  for (uint8_t i = 0; i < MAX_WIFI_ITEMS; ++i) wifiSsids[i] = "";

  // Use station mode only for this test; disconnecting before scan avoids
  // reporting stale connection state as scan results are refreshed.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  int count = WiFi.scanNetworks();
  if (count <= 0) {
    lv_label_set_text(statusLabel, "No WiFi AP found.");
    return;
  }

  int shown = min(count, (int)MAX_WIFI_ITEMS);
  lv_label_set_text_fmt(statusLabel, "%d AP(s) found. Showing %d.", count, shown);
  for (int i = 0; i < shown; i++) {
    lv_obj_t *btn = lv_btn_create(wifiList);
    lv_obj_set_width(btn, lv_pct(100));
    wifiSsids[i] = WiFi.SSID(i);
    lv_obj_add_event_cb(btn, wifiItemCb, LV_EVENT_CLICKED, &wifiSsids[i]);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%s  %d dBm  %s",
                          WiFi.SSID(i).c_str(),
                          WiFi.RSSI(i),
                          authText(WiFi.encryptionType(i)));
    lv_obj_center(label);
  }
}

static void scanButtonCb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) scanWifi();
}

static void taEventCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
    // Show LVGL's built-in keyboard only while the password field is active.
    lv_keyboard_set_textarea(keyboard, passwordTa);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

static void buildUi() {
  // The screen mirrors the ESP-IDF example's practical flow: scan list first,
  // then a centered password panel for the selected AP.
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "WiFi Test");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *scanBtn = lv_btn_create(scr);
  lv_obj_set_size(scanBtn, 90, 38);
  lv_obj_align(scanBtn, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_add_event_cb(scanBtn, scanButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_label_set_text(lv_label_create(scanBtn), "Scan");
  lv_obj_center(lv_obj_get_child(scanBtn, 0));

  statusLabel = lv_label_create(scr);
  lv_obj_set_width(statusLabel, 290);
  lv_label_set_text(statusLabel, "Press Scan to search WiFi APs.");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 52);

  wifiList = lv_obj_create(scr);
  lv_obj_set_size(wifiList, 300, 340);
  lv_obj_align(wifiList, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_set_flex_flow(wifiList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(wifiList, LV_DIR_VER);

  connectPanel = lv_obj_create(scr);
  lv_obj_set_size(connectPanel, 300, 270);
  lv_obj_align(connectPanel, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_bg_color(connectPanel, lv_color_hex(0xf7f7f7), 0);
  lv_obj_add_flag(connectPanel, LV_OBJ_FLAG_HIDDEN);

  ssidLabel = lv_label_create(connectPanel);
  lv_obj_set_width(ssidLabel, 260);
  lv_label_set_text(ssidLabel, "");
  lv_obj_align(ssidLabel, LV_ALIGN_TOP_MID, 0, 8);

  passwordTa = lv_textarea_create(connectPanel);
  lv_textarea_set_password_mode(passwordTa, true);
  lv_textarea_set_placeholder_text(passwordTa, "Password");
  lv_obj_set_width(passwordTa, 260);
  lv_obj_align(passwordTa, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_add_event_cb(passwordTa, taEventCb, LV_EVENT_ALL, nullptr);

  lv_obj_t *connectBtn = lv_btn_create(connectPanel);
  lv_obj_set_size(connectBtn, 118, 42);
  lv_obj_align(connectBtn, LV_ALIGN_BOTTOM_LEFT, 18, -12);
  lv_obj_add_event_cb(connectBtn, connectCb, LV_EVENT_CLICKED, nullptr);
  lv_label_set_text(lv_label_create(connectBtn), "Connect");
  lv_obj_center(lv_obj_get_child(connectBtn, 0));

  lv_obj_t *closeBtn = lv_btn_create(connectPanel);
  lv_obj_set_size(closeBtn, 90, 42);
  lv_obj_align(closeBtn, LV_ALIGN_BOTTOM_RIGHT, -18, -12);
  lv_obj_add_event_cb(closeBtn, closePanelCb, LV_EVENT_CLICKED, nullptr);
  lv_label_set_text(lv_label_create(closeBtn), "Close");
  lv_obj_center(lv_obj_get_child(closeBtn, 0));

  keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(keyboard, 320, 170);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void pollWifiStatus() {
  // WiFi.begin() is asynchronous. Poll only for status changes so the UI text
  // is updated without blocking LVGL's touch and redraw loop.
  static wl_status_t lastStatus = WL_IDLE_STATUS;
  wl_status_t status = WiFi.status();
  if (status == lastStatus) return;
  lastStatus = status;
  if (status == WL_CONNECTED) {
    lv_label_set_text_fmt(statusLabel, "Connected: %s  IP: %s",
                          WiFi.SSID().c_str(),
                          WiFi.localIP().toString().c_str());
    showPanel(false);
  } else if (status == WL_CONNECT_FAILED) {
    lv_label_set_text(statusLabel, "Connection failed.");
  } else if (status == WL_DISCONNECTED) {
    lv_label_set_text(statusLabel, "Disconnected.");
  }
}

void setup() {
  boardBegin();
  buildUi();
}

void loop() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();
  pollWifiStatus();
  delay(5);
}
