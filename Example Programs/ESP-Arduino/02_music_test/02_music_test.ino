#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <FS.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"
#include "src/gui/gui_music.h"
#include "src/gui/gui_hal.h"

#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static lv_indev_t *touchIndev = nullptr;
static bool sdReady = false;

// PCF8574 ports are quasi-bidirectional and default high. Keep a shadow byte so
// changing LCD reset or SD CS never accidentally toggles the speaker amplifier.
// I2S_CTRL starts LOW to prevent a power-on pop before I2S outputs valid samples.
static uint8_t expanderState = 0xFF & ~(1 << PCF8574_PIN_I2S_CTRL);

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

static void setExpanderPin(uint8_t pin, bool level) {
  if (level) expanderState |= (1 << pin);
  else expanderState &= ~(1 << pin);
  ioExpander.digitalWriteByte(expanderState);
}

// Called by the Arduino audio HAL after the decoder/I2S path has settled.
extern "C" void board_set_speaker_amp(bool enable) {
  setExpanderPin(PCF8574_PIN_I2S_CTRL, enable);
}

static void lcdResetByExpander() {
  if (!ioExpander.begin(PCF8574_ADDR, &Wire)) {
    Serial.println("PCF8574 init failed");
    return;
  }
  // Apply the safe shadow state before touching LCD reset; this keeps I2S_CTRL LOW.
  ioExpander.digitalWriteByte(expanderState);
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
  touchIndev = lv_indev_drv_register(&indevDrv);
}

static void boardBegin() {
  Serial.begin(115200);
  // Initialize I2C/PCF8574 early so the speaker amp is forced off before UI work.
  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();
  Serial.println("\nONX3248G035 Arduino 02_music_test");
  gfx->begin(LCD_SPI_FREQ);
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);
  touch.begin(&Wire, CST826_ADDR);
  lvglBegin();
}

static bool mountSdCard() {
  // The board uses the SD_MMC 1-bit bus; SDCS is held high on the expander.
  setExpanderPin(PCF8574_PIN_SDCS, true);
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
  sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  Serial.printf("SD_MMC mount: %s\r\n", sdReady ? "OK" : "FAILED");
  return sdReady;
}

static void buildUi() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);

  // Match the ESP-IDF example: show a centered red hint when SD/music is absent.
  if (!sdReady) {
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_size(label, 250, 100);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), 0);
    lv_label_set_text(label, "No SD card detected. Please insert it and then power on again.");
    lv_obj_center(label);
    return;
  }
  if (gui_hal_music_scan() != ESP_OK) {
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_size(label, 250, 100);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), 0);
    lv_label_set_text(label, "Please create a folder named \"music\" in the SD card root and copy your audio files into it.");
    lv_obj_center(label);
    return;
  }

  // The visual UI is ported from ESP-IDF/components/gui; gui_hal.cpp adapts its
  // callbacks to Arduino SD_MMC and ESP32-audioI2S playback.
  gui_hal_audio_init();
  gui_music();
}

void setup() {
  boardBegin();
  mountSdCard();
  buildUi();
}

void loop() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();
  gui_hal_audio_loop();
  delay(5);
}
