#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"
#include "src/widgets/lv_demo_widgets.h"

// Keep the same display transform used by the verified touch/display examples.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;

// Arduino_GFX drives the ST7796U panel; LVGL renders into the line buffer below.
Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];

// Map the user-facing transform macros to Arduino_GFX rotation values.
static uint8_t displayRotationFromConfig() {
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

// The LCD reset line is routed through PCF8574, so keep one cached output byte.
static void setExpanderPin(uint8_t pin, bool level) {
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

  ioExpander.digitalWriteByte(0xFF);
  delay(20);
  setExpanderPin(PCF8574_PIN_LCD_RST, false);
  delay(120);
  setExpanderPin(PCF8574_PIN_LCD_RST, true);
  delay(120);
}

// LVGL flush callback: copy each invalidated area to the physical LCD.
static void lvglFlush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorP) {
  uint32_t width = area->x2 - area->x1 + 1;
  uint32_t height = area->y2 - area->y1 + 1;

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&colorP->full, width, height);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&colorP->full, width, height);
#endif
  lv_disp_flush_ready(drv);
}

// CST826 touch callback used by LVGL's pointer input device.
static void lvglTouchRead(lv_indev_drv_t *, lv_indev_data_t *data) {
  static int16_t lastX = 0;
  static int16_t lastY = 0;

  if (touch.touched()) {
    CST_TS_Point point = touch.getPoint();
    lastX = constrain(point.x, 0, (int16_t)gfx->width() - 1);
    lastY = constrain(point.y, 0, (int16_t)gfx->height() - 1);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  data->point.x = lastX;
  data->point.y = lastY;
}

// Register the display and touch drivers before creating any LVGL widgets.
static void lvglBegin() {
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

// Bring up the shared board peripherals used by the out-of-box demo.
static void boardBegin() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nONX3248G035 Arduino 09_outofbox_demo");

  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();

  gfx->begin(LCD_SPI_FREQ);
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);

  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  if (!touch.begin(&Wire, CST826_ADDR)) {
    Serial.println("CST826 init warning");
  }

  lvglBegin();
}

void setup() {
  boardBegin();
  // This is the ESP-IDF example's customized LVGL widgets demo.
  lv_demo_widgets();
}

void loop() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();

  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();
  delay(5);
}
