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

// These macros use the same orientation model as 01_touch_test. Keeping them
// visible in each display demo makes panel/touch direction changes easy to test.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;
static const char *TEST_FILE = "/SD CARD TEST.txt";

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static lv_obj_t *resultLabel = nullptr;
static lv_obj_t *detailLabel = nullptr;
static bool sdReady = false;

static uint8_t displayRotationFromConfig() {
  // Arduino_GFX encodes mirror/rotation differently from the ST7796U command
  // bits, so translate the public direction macros into the library rotation.
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
  // PCF8574 only supports byte-wide output state. Cache the last written byte so
  // changing LCD reset or SDCS does not disturb the other expander outputs.
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
  // Match the hardware reset sequence used by the ESP-IDF example.
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
  // build_opt.h lets LVGL keep its default color settings; select the matching
  // Arduino_GFX transfer API at compile time.
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
    // The display rotation above already aligns CST826 coordinates with LVGL.
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
  // LVGL owns the SD-card test UI: result text, button, and touch events.
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
  Serial.println("\nONX3248G035 Arduino 04_sd_card_test");
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

static bool ensureSdCard() {
  // Arduino SD_MMC does not expose the ESP-IDF CMD13 card-status probe used by
  // the original example. Remount on every button test so hot remove/insert is
  // reflected in the UI instead of reusing a stale mounted flag.
  if (sdReady) {
    SD_MMC.end();
    sdReady = false;
    delay(20);
  }
  // The board uses SDMMC 1-bit mode. SDCS is kept high by PCF8574, matching the
  // original schematic and ESP-IDF initialization.
  setExpanderPin(PCF8574_PIN_SDCS, true);
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
  sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  Serial.printf("SD_MMC mount: %s\r\n", sdReady ? "OK" : "FAILED");
  return sdReady;
}

static void setResult(const char *result, uint32_t color, const char *detail) {
  lv_label_set_text(resultLabel, result);
  lv_obj_set_style_text_color(resultLabel, lv_color_hex(color), 0);
  lv_label_set_text(detailLabel, detail);
}

static bool runSdTest(String &written, String &readBack) {
  // Use a fresh random payload so stale files or cached reads are easy to catch.
  written = "SD CARD TEST RAND NUM " + String((uint32_t)esp_random());

  // Write, flush, close, then reopen for readback. This follows the visible
  // behavior of the ESP-IDF sample while using Arduino's FS API.
  File w = SD_MMC.open(TEST_FILE, FILE_WRITE);
  if (!w) return false;
  size_t n = w.print(written);
  w.flush();
  w.close();
  if (n != written.length()) return false;

  File r = SD_MMC.open(TEST_FILE, FILE_READ);
  if (!r) return false;
  readBack = r.readString();
  r.close();
  return readBack == written;
}

static void testButtonCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  String written;
  String readBack;
  // Re-check the card at click time so removing the SD card after a successful
  // test reports "NO SD CARD DETECTED" instead of a generic write failure.
  if (!ensureSdCard()) {
    setResult("NO SD CARD DETECTED", 0xff0000, "Insert SD card and press the button again.");
    return;
  }
  if (runSdTest(written, readBack)) {
    setResult("WRITE SUCCESS", 0x00aa00, written.c_str());
  } else {
    String detail = "Written: " + written + "\nRead: " + readBack;
    setResult("WRITE ERROR", 0xff0000, detail.c_str());
  }
}

static void buildUi() {
  // Keep the UI layout close to the ESP-IDF example: centered result text,
  // centered action button, and a short status/detail line below it.
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  resultLabel = lv_label_create(scr);
  lv_label_set_text(resultLabel, "SD CARD TEST");
  lv_obj_set_style_text_color(resultLabel, lv_color_hex(0x444444), 0);
  lv_obj_align(resultLabel, LV_ALIGN_CENTER, 0, -76);

  lv_obj_t *btn = lv_btn_create(scr);
  lv_obj_set_size(btn, 190, 58);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_radius(btn, 20, 0);
  lv_obj_add_event_cb(btn, testButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, "SD CARD TEST");
  lv_obj_center(label);

  detailLabel = lv_label_create(scr);
  lv_obj_set_width(detailLabel, 280);
  lv_obj_set_style_text_align(detailLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(detailLabel, "Press the button to write and read /SD CARD TEST.txt.");
  lv_obj_align(detailLabel, LV_ALIGN_CENTER, 0, 92);
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
  delay(5);
}
