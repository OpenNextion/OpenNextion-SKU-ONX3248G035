#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"

// Keep the same orientation switches as the other Arduino display examples.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;
// Arduino version keeps a lightweight voltage-based estimate instead of pulling
// in the ESP-IDF adc_battery_estimation component.
static const uint16_t BATTERY_EMPTY_MV = 3300;
static const uint16_t BATTERY_FULL_MV = 4200;
static const uint8_t BATTERY_ADC_SAMPLES = 16;
// The percent display intentionally moves slowly so USB plug/unplug transients
// do not look like real battery capacity changes.
static const uint8_t PERCENT_HYSTERESIS = 2;
static const uint32_t PERCENT_STEP_INTERVAL_MS = 30000;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static lv_obj_t *batteryIconLabel = nullptr;
static lv_obj_t *percentLabel = nullptr;
static lv_obj_t *voltageLabel = nullptr;
static lv_obj_t *chargeLabel = nullptr;
static lv_obj_t *levelBar = nullptr;
static uint32_t filteredBatteryMv = 0;
static uint8_t displayedPercent = 0;
static bool percentReady = false;
static bool chargingStateReady = false;
static bool lastChargingState = false;
static uint32_t lastPercentStepMs = 0;

static uint8_t displayRotationFromConfig() {
  // Arduino_GFX packs rotation and mirroring into one rotation ID.
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
  // PCF8574 writes a whole byte each time. Cache output state so toggling LCD
  // reset does not disturb CHG_N pull/read configuration.
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

static void lvglFlush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorP) {
  uint32_t width = area->x2 - area->x1 + 1;
  uint32_t height = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
  // Select the bitmap API that matches LVGL's configured RGB565 byte order.
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
  Serial.println("\nONX3248G035 Arduino 08_battery_test");

  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();
  // CHG_N is read through PCF8574. Writing high releases the quasi-bidirectional
  // pin so the charger IC can pull it low/high.
  setExpanderPin(PCF8574_PIN_CHG_N, true);

  gfx->begin(LCD_SPI_FREQ);
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  analogReadResolution(12);
  touch.begin(&Wire, CST826_ADDR);
  lvglBegin();
}

static uint32_t readBatteryMv() {
  uint32_t adcMvSum = 0;
  // Average several calibrated millivolt readings to suppress ADC jitter.
  for (uint8_t i = 0; i < BATTERY_ADC_SAMPLES; ++i) {
    adcMvSum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }
  uint32_t adcMv = adcMvSum / BATTERY_ADC_SAMPLES;
  // Convert divider voltage back to battery voltage:
  // Vbat = Vadc * (Rupper + Rlower) / Rlower.
  uint64_t scaled = (uint64_t)adcMv * (BATTERY_RESISTOR_UPPER + BATTERY_RESISTOR_LOWER);
  return scaled / BATTERY_RESISTOR_LOWER;
}

static uint8_t estimatePercent(uint32_t mv) {
  // Lightweight approximation for Arduino: linear 3.3 V to 4.2 V mapping.
  // The displayed value is further stabilized in stabilizePercent().
  if (mv <= BATTERY_EMPTY_MV) return 0;
  if (mv >= BATTERY_FULL_MV) return 100;
  return (mv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
}

static const char *batterySymbol(uint8_t percent) {
  if (percent <= 10) return LV_SYMBOL_BATTERY_EMPTY;
  if (percent <= 35) return LV_SYMBOL_BATTERY_1;
  if (percent <= 60) return LV_SYMBOL_BATTERY_2;
  if (percent <= 85) return LV_SYMBOL_BATTERY_3;
  return LV_SYMBOL_BATTERY_FULL;
}

static uint32_t filterBatteryMv(uint32_t mv) {
  // Low-pass filter the terminal voltage. Charging can still raise the voltage
  // quickly, so capacity percentage is rate-limited separately.
  if (filteredBatteryMv == 0) {
    filteredBatteryMv = mv;
  } else {
    filteredBatteryMv = (filteredBatteryMv * 7 + mv) / 8;
  }
  return filteredBatteryMv;
}

static uint8_t stabilizePercent(uint8_t rawPercent, bool charging) {
  uint32_t now = millis();
  if (!percentReady) {
    displayedPercent = rawPercent;
    percentReady = true;
    lastPercentStepMs = now;
  }

  if (!chargingStateReady) {
    lastChargingState = charging;
    chargingStateReady = true;
  } else if (lastChargingState != charging) {
    // USB insertion/removal changes terminal voltage immediately. Keep the
    // displayed capacity steady through the transition.
    lastChargingState = charging;
    lastPercentStepMs = now;
    return displayedPercent;
  }

  if (now - lastPercentStepMs >= PERCENT_STEP_INTERVAL_MS) {
    // Move one percent at a time only after the raw estimate is meaningfully
    // different. This avoids 97/98% boundary flicker and charge-voltage jumps.
    if (rawPercent >= displayedPercent + PERCENT_HYSTERESIS) {
      displayedPercent++;
      lastPercentStepMs = now;
    } else if (rawPercent + PERCENT_HYSTERESIS <= displayedPercent) {
      displayedPercent--;
      lastPercentStepMs = now;
    }
  }
  return displayedPercent;
}

static void buildUi() {
  // Arduino version shows a larger debug-oriented battery panel: icon, percent,
  // filtered voltage, and charge state.
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0xf7f8fb), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Battery Test");
  lv_obj_set_style_text_color(title, lv_color_hex(0x1f2937), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  batteryIconLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(batteryIconLabel, lv_color_hex(0x10b981), 0);
  lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_EMPTY);
  lv_obj_align(batteryIconLabel, LV_ALIGN_CENTER, 0, -80);

  percentLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(percentLabel, lv_color_hex(0x111827), 0);
  lv_label_set_text(percentLabel, "--%");
  lv_obj_align(percentLabel, LV_ALIGN_CENTER, 0, -18);

  levelBar = lv_bar_create(scr);
  lv_obj_set_size(levelBar, 240, 20);
  lv_bar_set_range(levelBar, 0, 100);
  lv_obj_align(levelBar, LV_ALIGN_CENTER, 0, 32);

  voltageLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(voltageLabel, lv_color_hex(0x4b5563), 0);
  lv_label_set_text(voltageLabel, "Voltage: -- mV");
  lv_obj_align(voltageLabel, LV_ALIGN_CENTER, 0, 74);

  chargeLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(chargeLabel, lv_color_hex(0x4b5563), 0);
  lv_label_set_text(chargeLabel, "Charge: --");
  lv_obj_align(chargeLabel, LV_ALIGN_BOTTOM_MID, 0, -34);
}

static void updateBattery() {
  uint32_t rawMv = readBatteryMv();
  uint32_t mv = filterBatteryMv(rawMv);
  uint8_t rawPercent = estimatePercent(mv);
  bool charging = ioExpander.digitalRead(PCF8574_PIN_CHG_N);
  uint8_t percent = stabilizePercent(rawPercent, charging);

  lv_label_set_text(batteryIconLabel, batterySymbol(percent));
  lv_label_set_text_fmt(percentLabel, "%u%%", percent);
  lv_label_set_text_fmt(voltageLabel, "Voltage: %lu mV", (unsigned long)mv);
  lv_label_set_text_fmt(chargeLabel, "Charge: %s", charging ? "Charging" : "Not charging");
  lv_bar_set_value(levelBar, percent, LV_ANIM_OFF);

  Serial.printf("Battery: raw=%lu mV, filtered=%lu mV, raw=%u%%, shown=%u%%, charging=%s\r\n",
                (unsigned long)rawMv, (unsigned long)mv, rawPercent, percent,
                charging ? "yes" : "no");
}

void setup() {
  boardBegin();
  buildUi();
  updateBattery();
}

void loop() {
  static uint32_t lastMs = millis();
  static uint32_t lastBatteryMs = 0;
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();

  if (now - lastBatteryMs >= 1000) {
    lastBatteryMs = now;
    updateBattery();
  }
  delay(5);
}
