#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>

#define LV_CONF_SKIP
#include <lvgl.h>

#include "BoardConfig.h"

// Keep display direction controls aligned with the other screen examples.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

static const uint32_t LCD_SPI_FREQ = 80000000;
static const uint16_t LVGL_BUF_LINES = 40;
// Match the ESP-IDF microphone example: 16 kHz, 16-bit PCM, 30 seconds in RAM.
static const uint32_t AUDIO_SAMPLE_RATE = 16000;
static const uint32_t RECORD_SECONDS = 30;
static const size_t AUDIO_CHUNK_SAMPLES = 256;

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static lv_obj_t *titleLabel = nullptr;
static lv_obj_t *statusLabel = nullptr;
static lv_obj_t *micButton = nullptr;

// Use separate I2S controllers just like the ESP-IDF example: I2S0 for PDM
// microphone input and I2S1 for NS4168 speaker output.
static i2s_chan_handle_t micRx = nullptr;
static i2s_chan_handle_t speakerTx = nullptr;
// Recorded mono PCM is stored in RAM and looped during playback.
static int16_t *recordBuffer = nullptr;
static size_t recordCapacitySamples = 0;
static size_t recordSamples = 0;
static size_t playIndex = 0;

enum class AudioState {
  Idle,
  Recording,
  Playing,
};

static AudioState audioState = AudioState::Idle;
static bool micReady = false;
static bool speakerReady = false;
static bool recordReady = false;

static uint8_t displayRotationFromConfig() {
  // Arduino_GFX encodes mirror/rotation combinations as rotation IDs.
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
  // PCF8574 writes all outputs at once, so keep a shadow byte before toggling
  // LCD reset or the I2S amplifier enable pin.
  static uint8_t state = 0xFF;
  if (level) state |= (1 << pin);
  else state &= ~(1 << pin);
  ioExpander.digitalWriteByte(state);
}

static void speakerAmp(bool enabled) {
  // The NS4168 amplifier enable is routed through PCF8574 I2S_CTRL.
  setExpanderPin(PCF8574_PIN_I2S_CTRL, enabled);
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
  // Select the matching Arduino_GFX path for LVGL's RGB565 byte order.
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
    // The display rotation above keeps CST826 coordinates aligned with LVGL.
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
  Serial.println("\nONX3248G035 Arduino 07_microphone_test");

  Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
  lcdResetByExpander();
  speakerAmp(false);

  gfx->begin(LCD_SPI_FREQ);
  gfx->setRotation(displayRotationFromConfig());
  gfx->fillScreen(RGB565_WHITE);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  touch.begin(&Wire, CST826_ADDR);
  lvglBegin();
}

static bool allocateRecordBuffer() {
  recordCapacitySamples = AUDIO_SAMPLE_RATE * RECORD_SECONDS;
  size_t bytes = recordCapacitySamples * sizeof(int16_t);
  // PSRAM is expected on ESP32S3R8; internal RAM fallback keeps the failure mode
  // explicit if PSRAM is disabled in Arduino IDE.
  recordBuffer = static_cast<int16_t *>(ps_malloc(bytes));
  if (!recordBuffer) recordBuffer = static_cast<int16_t *>(malloc(bytes));
  if (!recordBuffer) {
    Serial.printf("Record buffer allocation failed: %u bytes\r\n", (unsigned)bytes);
    return false;
  }
  Serial.printf("Record buffer ready: %u bytes\r\n", (unsigned)bytes);
  return true;
}

static bool micBegin() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 4;
  chanCfg.dma_frame_num = AUDIO_CHUNK_SAMPLES;
  esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &micRx);
  if (err != ESP_OK) {
    Serial.printf("i2s_new_channel RX failed: 0x%X\r\n", err);
    return false;
  }

  i2s_pdm_rx_config_t pdmCfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
      // The PDM driver returns decoded 16-bit mono PCM samples.
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .clk = (gpio_num_t)PDM_CLK_PIN,
          .din = (gpio_num_t)PDM_DATA_PIN,
          .invert_flags = {.clk_inv = false},
      },
  };
  err = i2s_channel_init_pdm_rx_mode(micRx, &pdmCfg);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_init_pdm_rx_mode failed: 0x%X\r\n", err);
    return false;
  }

  err = i2s_channel_enable(micRx);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_enable RX failed: 0x%X\r\n", err);
    return false;
  }
  return true;
}

static bool speakerBegin() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 4;
  chanCfg.dma_frame_num = AUDIO_CHUNK_SAMPLES;
  esp_err_t err = i2s_new_channel(&chanCfg, &speakerTx, nullptr);
  if (err != ESP_OK) {
    Serial.printf("i2s_new_channel TX failed: 0x%X\r\n", err);
    return false;
  }

  i2s_std_config_t txCfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
      // Feed the mono PCM buffer directly to the NS4168 over standard I2S.
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = (gpio_num_t)SPEAKER_BCLK_PIN,
          .ws = (gpio_num_t)SPEAKER_WS_PIN,
          .dout = (gpio_num_t)SPEAKER_DATA_PIN,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };
  err = i2s_channel_init_std_mode(speakerTx, &txCfg);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_init_std_mode failed: 0x%X\r\n", err);
    return false;
  }

  err = i2s_channel_enable(speakerTx);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_enable TX failed: 0x%X\r\n", err);
    return false;
  }
  return true;
}

static void setMicButtonColor(uint32_t color) {
  if (micButton) lv_obj_set_style_bg_color(micButton, lv_color_hex(color), 0);
}

static void stopAudio() {
  audioState = AudioState::Idle;
  // Disable the amplifier when idle to avoid background noise.
  speakerAmp(false);
  setMicButtonColor(0x2195f6);
  if (statusLabel) {
    lv_label_set_text(statusLabel, recordReady ? "Press and hold MIC to record again." : "Press and hold MIC to record.");
  }
}

static void startRecording() {
  if (!micReady || !recordBuffer) {
    lv_label_set_text(statusLabel, "Microphone init failed");
    return;
  }
  speakerAmp(false);
  // A new long press starts a fresh recording and overwrites the previous take.
  recordSamples = 0;
  playIndex = 0;
  recordReady = false;
  audioState = AudioState::Recording;
  setMicButtonColor(0xff4d4f);
  lv_label_set_text(statusLabel, "Recording... release MIC to play");
}

static void startPlayback() {
  if (!speakerReady || !recordReady || recordSamples == 0) {
    stopAudio();
    return;
  }
  playIndex = 0;
  audioState = AudioState::Playing;
  setMicButtonColor(0x2ecc71);
  // Turn the amplifier on only when valid audio is ready to play.
  speakerAmp(true);
  lv_label_set_text(statusLabel, "Playing recorded audio. Tap STOP to stop.");
}

static void micButtonCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  // Match ESP-IDF behavior: hold MIC to record, release MIC to play.
  if (code == LV_EVENT_PRESSED || code == LV_EVENT_LONG_PRESSED) {
    if (audioState != AudioState::Recording) startRecording();
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (audioState == AudioState::Recording) {
      recordReady = recordSamples > 0;
      Serial.printf("Recorded samples: %u\r\n", (unsigned)recordSamples);
      startPlayback();
    }
  }
}

static void stopButtonCb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) stopAudio();
}

static void buildUi() {
  // Keep the screen layout close to the ESP-IDF version: title, MIC, and Stop.
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  titleLabel = lv_label_create(scr);
  lv_label_set_recolor(titleLabel, true);
  lv_label_set_text(titleLabel, "#808080 Microphone Test#");
  lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, -50);

  statusLabel = lv_label_create(scr);
  lv_obj_set_width(statusLabel, 280);
  lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x606060), 0);
  lv_label_set_text(statusLabel, (micReady && speakerReady && recordBuffer) ? "Press and hold MIC to record." : "Audio init failed");
  lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 0);

  micButton = lv_btn_create(scr);
  lv_obj_set_size(micButton, 70, 40);
  lv_obj_align(micButton, LV_ALIGN_CENTER, -60, 50);
  lv_obj_set_style_bg_color(micButton, lv_color_hex(0x2195f6), 0);
  lv_obj_add_event_cb(micButton, micButtonCb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(micButton, micButtonCb, LV_EVENT_LONG_PRESSED, nullptr);
  lv_obj_add_event_cb(micButton, micButtonCb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(micButton, micButtonCb, LV_EVENT_PRESS_LOST, nullptr);
  lv_obj_t *micLabel = lv_label_create(micButton);
  lv_label_set_text(micLabel, "MIC");
  lv_obj_center(micLabel);

  lv_obj_t *stopButton = lv_btn_create(scr);
  lv_obj_set_size(stopButton, 70, 40);
  lv_obj_align(stopButton, LV_ALIGN_CENTER, 60, 50);
  lv_obj_add_event_cb(stopButton, stopButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *stopLabel = lv_label_create(stopButton);
  lv_label_set_text(stopLabel, "Stop");
  lv_obj_center(stopLabel);
}

static void processRecording() {
  if (audioState != AudioState::Recording || !recordBuffer) return;
  if (recordSamples >= recordCapacitySamples) {
    // Auto-play when the fixed RAM buffer is full.
    recordReady = recordSamples > 0;
    startPlayback();
    return;
  }

  size_t freeSamples = recordCapacitySamples - recordSamples;
  size_t samplesToRead = min(freeSamples, AUDIO_CHUNK_SAMPLES);
  size_t bytesRead = 0;
  esp_err_t err = i2s_channel_read(micRx, recordBuffer + recordSamples,
                                   samplesToRead * sizeof(int16_t), &bytesRead, 0);
  if (err != ESP_OK || bytesRead == 0) return;
  recordSamples += bytesRead / sizeof(int16_t);

  static uint32_t lastUiMs = 0;
  uint32_t now = millis();
  if (now - lastUiMs >= 250) {
    lastUiMs = now;
    lv_label_set_text_fmt(statusLabel, "Recording... %.1f s", recordSamples / (float)AUDIO_SAMPLE_RATE);
  }
}

static void processPlayback() {
  if (audioState != AudioState::Playing || !recordReady || !recordBuffer) return;
  // ESP-IDF version loops the captured buffer until Stop is tapped.
  if (playIndex >= recordSamples) playIndex = 0;

  size_t remain = recordSamples - playIndex;
  size_t samplesToWrite = min(remain, AUDIO_CHUNK_SAMPLES);
  size_t bytesWritten = 0;
  esp_err_t err = i2s_channel_write(speakerTx, recordBuffer + playIndex,
                                    samplesToWrite * sizeof(int16_t), &bytesWritten, 20);
  if (err == ESP_OK && bytesWritten > 0) {
    playIndex += bytesWritten / sizeof(int16_t);
  }
}

void setup() {
  boardBegin();
  bool bufferReady = allocateRecordBuffer();
  micReady = micBegin();
  speakerReady = speakerBegin();
  if (!speakerReady) speakerAmp(false);
  buildUi();
  if (!bufferReady || !micReady || !speakerReady) {
    Serial.println("07 audio init incomplete");
  }
}

void loop() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();
  processRecording();
  processPlayback();
  delay(5);
}
