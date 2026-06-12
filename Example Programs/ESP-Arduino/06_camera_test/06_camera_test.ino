#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>
#include <esp_camera.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"

// Shared display direction controls. These match the other Arduino examples so
// touch and LCD orientation can be adjusted consistently from one place.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;
// The Arduino camera driver captures QVGA RGB565 frames. They are drawn at the
// native 320-pixel screen width and vertically placed below the status text.
static const uint16_t PREVIEW_W = 320;
static const uint16_t PREVIEW_H = 240;
static const uint16_t PREVIEW_Y = 104;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static lv_obj_t *statusLabel = nullptr;
static bool cameraReady = false;

static uint8_t displayRotationFromConfig() {
  // Arduino_GFX uses rotation IDs for rotation and mirror combinations. Convert
  // the public direction macros into that library-specific value.
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

static void setExpanderPin(uint8_t pin, bool level) {
  // PCF8574 writes the full output byte each time. Keep a software copy so LCD
  // reset and camera power-down can be toggled independently.
  static uint8_t state = 0xFF;
  if (level) state |= (1 << pin);
  else state &= ~(1 << pin);
  ioExpander.digitalWriteByte(state);
}

static void lcdResetByExpander() {
  if (!ioExpander.begin(PCF8574_ADDR, &Wire)) {
    Serial.println("PCF8574 init failed");
    return;
  }
  // Match the hardware reset pulse used by the ESP-IDF example.
  ioExpander.digitalWriteByte(0xFF);
  delay(20);
  setExpanderPin(PCF8574_PIN_LCD_RST, false);
  delay(120);
  setExpanderPin(PCF8574_PIN_LCD_RST, true);
  delay(120);
}

static void lvglFlush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorP) {
  uint32_t width = area->x2 - area->x1 + 1;
  uint32_t height = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
  // LVGL may store RGB565 bytes swapped; use the matching Arduino_GFX API.
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
    // Display rotation already aligns the raw CST826 coordinates with LVGL.
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
  // LVGL is used for the title/status area. Camera frames are pushed directly
  // with Arduino_GFX to avoid copying every frame into an LVGL image object.
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
  Serial.println("\nONX3248G035 Arduino 06_camera_test");

  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();
  // Camera PWDN is controlled by PCF8574 on this board. Pull it low before
  // esp_camera_init() so the sensor is awake during SCCB probing.
  setExpanderPin(PCF8574_PIN_CAM_PWDN, false);

  gfx->begin(LCD_SPI_FREQ);
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  touch.begin(&Wire, CST826_ADDR);
  lvglBegin();
}

static bool cameraBegin() {
  camera_config_t config = {};
  // The pin map matches BoardConfig.h and the ESP-IDF reference example.
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_Y2_GPIO;
  config.pin_d1 = CAM_Y3_GPIO;
  config.pin_d2 = CAM_Y4_GPIO;
  config.pin_d3 = CAM_Y5_GPIO;
  config.pin_d4 = CAM_Y6_GPIO;
  config.pin_d5 = CAM_Y7_GPIO;
  config.pin_d6 = CAM_Y8_GPIO;
  config.pin_d7 = CAM_Y9_GPIO;
  config.pin_xclk = CAM_XCLK_GPIO;
  config.pin_pclk = CAM_PCLK_GPIO;
  config.pin_vsync = CAM_VSYNC_GPIO;
  config.pin_href = CAM_HREF_GPIO;
  config.pin_sccb_sda = CAM_SIOD_GPIO;
  config.pin_sccb_scl = CAM_SIOC_GPIO;
  config.pin_pwdn = CAM_PWDN_GPIO;
  config.pin_reset = CAM_RESET_GPIO;
  config.xclk_freq_hz = 20000000;
  // QVGA RGB565 is displayed directly at 320x240. PSRAM enables double
  // buffering; DRAM fallback keeps the sketch usable when PSRAM is unavailable.
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_RGB565;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%X\r\n", err);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_framesize(sensor, FRAMESIZE_QVGA);
  }
  return true;
}

static void buildUi() {
  // Keep the Arduino UI minimal and close to the ESP-IDF demo: status text plus
  // a continuously refreshed camera preview, with no extra controls.
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Camera Test");
  lv_obj_set_style_text_color(title, lv_color_hex(0x20232a), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

  statusLabel = lv_label_create(scr);
  lv_obj_set_width(statusLabel, 290);
  lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x5d6470), 0);
  lv_label_set_text(statusLabel, cameraReady ? "Streaming RGB565 frames" : "Camera init failed");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 44);
}

static void drawCameraFrame() {
  if (!cameraReady) return;
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    lv_label_set_text(statusLabel, "Frame capture failed");
    return;
  }
  // esp_camera returns RGB565 data in the byte order expected by the big-endian
  // Arduino_GFX bitmap call on this panel.
  if (fb->format == PIXFORMAT_RGB565 && fb->width == PREVIEW_W && fb->height == PREVIEW_H) {
    gfx->draw16bitBeRGBBitmap(0, PREVIEW_Y, (uint16_t *)fb->buf, PREVIEW_W, PREVIEW_H);
  }
  esp_camera_fb_return(fb);
}

void setup() {
  boardBegin();
  cameraReady = cameraBegin();
  buildUi();
}

void loop() {
  static uint32_t lastMs = millis();
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();

  if (now - lastFrameMs >= 80) {
    // About 12.5 FPS keeps the UI responsive while providing a smooth preview.
    lastFrameMs = now;
    drawCameraFrame();
  }
  delay(5);
}
