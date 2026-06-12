#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"

// Match the ESP-IDF example orientation macros. Arduino_GFX uses a slightly
// different rotation table, so displayRotationFromConfig() maps these values.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;
static const uint16_t TOUCH_DOT_SIZE = 4;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(
    bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_disp_t *disp = nullptr;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static bool touchReady = false;

// Convert the board-level direction flags to Arduino_GFX ST7796 rotation IDs.
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

// LCD reset is routed through PCF8574 P6. Keep all other expander pins high,
// matching the ESP-IDF driver default state, so shared peripherals are not held low.
static void lcdResetByExpander() {
  if (!ioExpander.begin(PCF8574_ADDR, &Wire)) {
    Serial.println("PCF8574 init failed");
    return;
  }

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
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&colorP->full, width, height);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&colorP->full, width, height);
#endif

  lv_disp_flush_ready(drv);
}

// Draw touch feedback directly to the LCD, like the ESP-IDF example's
// esp_lcd_panel_draw_bitmap() path. Avoid creating unbounded LVGL objects.
static void drawTouchDot(lv_coord_t x, lv_coord_t y) {
  gfx->fillRect(x - TOUCH_DOT_SIZE / 2,
                y - TOUCH_DOT_SIZE / 2,
                TOUCH_DOT_SIZE,
                TOUCH_DOT_SIZE,
                RGB565_RED);
}

static void touchDotTimer(lv_timer_t *) {
  static lv_point_t lastPoint = {-1, -1};

  if (!touchReady || !touch.touched()) {
    return;
  }

  CST_TS_Point point = touch.getPoint();
  lv_point_t currentPoint = {
      constrain(point.x, TOUCH_DOT_SIZE / 2, (int16_t)gfx->width() - TOUCH_DOT_SIZE / 2 - 1),
      constrain(point.y, TOUCH_DOT_SIZE / 2, (int16_t)gfx->height() - TOUCH_DOT_SIZE / 2 - 1)};

  if (currentPoint.x != lastPoint.x || currentPoint.y != lastPoint.y) {
    drawTouchDot(currentPoint.x, currentPoint.y);
    Serial.printf("Touch: x=%d y=%d\r\n", currentPoint.x, currentPoint.y);
    lastPoint = currentPoint;
  }
}

static void createUi() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  // The root screen must not scroll when touched near an edge.
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, "Touch Test");
  lv_obj_set_style_text_color(label, lv_color_hex(0x808080), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);
}

// Adafruit_CST8XX validates the ID internally; this helper prints the raw ID
// so hardware bring-up logs stay close to the ESP-IDF example.
static uint8_t readTouchRegister(uint8_t reg) {
  Wire.beginTransmission(CST826_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(CST826_ADDR, (uint8_t)1) != 1) {
    return 0xFF;
  }
  return Wire.read();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ONX3248G035 Arduino 01_touch_test");

  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();

  if (!gfx->begin(LCD_SPI_FREQ)) {
    Serial.println("Arduino_GFX init failed");
  }
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);

  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  touchReady = touch.begin(&Wire, CST826_ADDR);
  uint8_t touchId = readTouchRegister(0xAA);
  Serial.printf("CST826 ID: 0x%02X\r\n", touchId);
  if (!touchReady) {
    Serial.println("CST826 init warning: Adafruit library did not accept chip ID, continue reading via library");
    touchReady = true;
  }

  lv_init();
  lv_disp_draw_buf_init(&drawBuf, dispBuf, nullptr, LCD_H_RES * LVGL_BUF_LINES);
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = gfx->width();
  dispDrv.ver_res = gfx->height();
  dispDrv.flush_cb = lvglFlush;
  dispDrv.draw_buf = &drawBuf;
  disp = lv_disp_drv_register(&dispDrv);

  createUi();
  // Force the first LVGL frame out before touch dots are drawn directly to LCD.
  lv_obj_invalidate(lv_scr_act());
  for (uint8_t i = 0; i < 5; i++) {
    lv_timer_handler();
    lv_refr_now(disp);
    delay(20);
  }
  lv_timer_create(touchDotTimer, 20, nullptr);
}

void loop() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;

  lv_timer_handler();
  delay(5);
}
