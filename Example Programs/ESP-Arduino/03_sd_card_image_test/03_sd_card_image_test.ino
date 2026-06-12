#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <FS.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>
#include <jpeg_decoder.h>

#define LV_CONF_SKIP
#include <lvgl.h>
// Reuse LVGL's bundled lodepng implementation for PNG decoding. The Arduino
// sketch calls the C API directly, so the C++ wrappers are disabled here.
#define LODEPNG_NO_COMPILE_CPP
extern "C" {
#include <extra/libs/png/lodepng.h>
}

#include "BoardConfig.h"

static const uint32_t LCD_SPI_FREQ = 40000000;
static const uint16_t LVGL_BUF_LINES = 40;
static const char *IMAGE_DIR = "/images";
static const uint8_t MAX_IMAGES = 64;

// Keep the same direction macros as the touch example so the LCD and CST826
// coordinate systems stay aligned across demos.
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

Arduino_DataBus *bus = new Arduino_HWSPI(
    LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_MISO_PIN, &SPI, true);
Arduino_GFX *gfx = new Arduino_ST7796(bus, GFX_NOT_DEFINED, 0, false, LCD_H_RES, LCD_V_RES);

Adafruit_CST8XX touch;
Adafruit_PCF8574 ioExpander;

static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_color_t dispBuf[LCD_H_RES * LVGL_BUF_LINES];
static String images[MAX_IMAGES];
static uint8_t imageCount = 0;
static int imageIndex = 0;
static bool sdReady = false;
static bool imageBrowserMode = false;
static bool imageNeedsRedraw = false;

// Full-screen image pages are rendered into PSRAM first, then pushed to LCD in
// bands. This avoids showing top-to-bottom decode progress on the panel.
static uint16_t *frameBuf = nullptr;

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
  // Keep all expander outputs high except the pins we actively drive below.
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
  // LVGL is still used for the no-card / no-image prompt pages. The image
  // browser draws directly to the LCD after a full frame has been decoded.
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
  Serial.println("\nONX3248G035 Arduino 03_sd_card_image_test");
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

static bool mountSdCard() {
  setExpanderPin(PCF8574_PIN_SDCS, true);
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
  sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  return sdReady;
}

static bool endsWithIgnoreCase(const String &value, const char *suffix) {
  String lower = value;
  lower.toLowerCase();
  return lower.endsWith(suffix);
}

static bool isImageFile(const String &name) {
  return endsWithIgnoreCase(name, ".jpg") || endsWithIgnoreCase(name, ".jpeg") ||
         endsWithIgnoreCase(name, ".png") || endsWithIgnoreCase(name, ".bmp") ||
         endsWithIgnoreCase(name, ".bin") || endsWithIgnoreCase(name, ".qoi") ||
         endsWithIgnoreCase(name, ".sjpg") || endsWithIgnoreCase(name, ".spng") ||
         endsWithIgnoreCase(name, ".sqoi");
}

static String baseNameOf(const char *path) {
  String name(path);
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  return name;
}

static String imagePathAt(int index) {
  return String(IMAGE_DIR) + "/" + images[index];
}

static void sortImages() {
  for (uint8_t i = 0; i + 1 < imageCount; ++i) {
    for (uint8_t j = i + 1; j < imageCount; ++j) {
      String a = images[i];
      String b = images[j];
      a.toLowerCase();
      b.toLowerCase();
      if (a > b) {
        String tmp = images[i];
        images[i] = images[j];
        images[j] = tmp;
      }
    }
  }
}

static bool isJpegFile(const String &name) {
  return endsWithIgnoreCase(name, ".jpg") || endsWithIgnoreCase(name, ".jpeg") ||
         endsWithIgnoreCase(name, ".sjpg");
}

static bool isPngFile(const String &name) {
  return endsWithIgnoreCase(name, ".png") || endsWithIgnoreCase(name, ".spng");
}

static void showCenteredMessage(const char *message, uint32_t color = RGB565_RED) {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(color);
  gfx->setTextSize(2);
  gfx->setCursor(18, gfx->height() / 2 - 24);
  gfx->println(message);
}

static bool ensureFrameBuffer() {
  if (frameBuf) return true;
  frameBuf = static_cast<uint16_t *>(ps_malloc(gfx->width() * gfx->height() * sizeof(uint16_t)));
  if (!frameBuf) {
    Serial.println("Frame buffer allocation failed");
    return false;
  }
  return true;
}

static void clearFrameBuffer(uint16_t color) {
  if (!frameBuf) return;
  uint32_t pixels = gfx->width() * gfx->height();
  for (uint32_t i = 0; i < pixels; ++i) {
    frameBuf[i] = color;
  }
}

static void presentFrameBuffer() {
  if (!frameBuf) return;
  // Arduino_GFX transfers are split into moderate bands to keep SPI writes
  // stable while still updating the finished frame quickly.
  const int16_t linesPerFlush = 40;
  for (int16_t y = 0; y < gfx->height(); y += linesPerFlush) {
    int16_t h = min<int16_t>(linesPerFlush, gfx->height() - y);
    gfx->draw16bitBeRGBBitmap(0, y, frameBuf + y * gfx->width(), gfx->width(), h);
  }
}

static uint16_t rgb565BigEndian(uint8_t r, uint8_t g, uint8_t b) {
  // draw16bitBeRGBBitmap expects RGB565 bytes in big-endian order.
  uint16_t rgb565 = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
  return (uint16_t)((rgb565 << 8) | (rgb565 >> 8));
}

static void drawScaledRgb565Be(const uint16_t *src, uint16_t srcW, uint16_t srcH) {
  if (!src || srcW == 0 || srcH == 0 || !frameBuf) return;

  // Match the verified behavior on hardware: scale every image to screen width
  // and center it vertically when the aspect ratio leaves unused space.
  uint16_t dstW = gfx->width();
  uint32_t scaledH32 = ((uint32_t)srcH * dstW + srcW / 2) / srcW;
  uint16_t dstH = min<uint32_t>(scaledH32, gfx->height());
  int16_t dstX = 0;
  int16_t dstY = (gfx->height() - dstH) / 2;

  for (uint16_t y = 0; y < dstH; ++y) {
    uint32_t srcY = ((uint32_t)y * srcH) / dstH;
    uint16_t *dst = frameBuf + (dstY + y) * gfx->width() + dstX;
    const uint16_t *srcRow = src + srcY * srcW;
    for (uint16_t x = 0; x < dstW; ++x) {
      uint32_t srcX = ((uint32_t)x * srcW) / dstW;
      dst[x] = srcRow[srcX];
    }
  }
}

static void drawScaledRgbaLvgl(const uint8_t *rgba, uint16_t srcW, uint16_t srcH) {
  if (!rgba || srcW == 0 || srcH == 0 || !frameBuf) return;

  // lodepng is compiled through LVGL on this target, whose 32-bit color layout
  // is BGRA in memory. Alpha is composited over the white screen background.
  uint16_t dstW = gfx->width();
  uint32_t scaledH32 = ((uint32_t)srcH * dstW + srcW / 2) / srcW;
  uint16_t dstH = min<uint32_t>(scaledH32, gfx->height());
  int16_t dstX = 0;
  int16_t dstY = (gfx->height() - dstH) / 2;

  for (uint16_t y = 0; y < dstH; ++y) {
    uint32_t srcY = ((uint32_t)y * srcH) / dstH;
    uint16_t *dst = frameBuf + (dstY + y) * gfx->width() + dstX;
    const uint8_t *srcRow = rgba + srcY * srcW * 4;
    for (uint16_t x = 0; x < dstW; ++x) {
      uint32_t srcX = ((uint32_t)x * srcW) / dstW;
      const uint8_t *px = srcRow + srcX * 4;

      uint8_t b = px[0];
      uint8_t g = px[1];
      uint8_t r = px[2];
      uint8_t a = px[3];
      if (a < 255) {
        r = ((uint16_t)r * a + 255U * (255U - a)) / 255U;
        g = ((uint16_t)g * a + 255U * (255U - a)) / 255U;
        b = ((uint16_t)b * a + 255U * (255U - a)) / 255U;
      }
      dst[x] = rgb565BigEndian(r, g, b);
    }
  }
}

static bool readWholeFile(const String &path, uint8_t **data, size_t *size) {
  *data = nullptr;
  *size = 0;

  File file = SD_MMC.open(path, FILE_READ);
  if (!file) return false;

  size_t fileSize = file.size();
  uint8_t *buffer = static_cast<uint8_t *>(ps_malloc(fileSize));
  if (!buffer) {
    file.close();
    return false;
  }

  size_t bytesRead = file.read(buffer, fileSize);
  file.close();
  if (bytesRead != fileSize) {
    free(buffer);
    return false;
  }

  *data = buffer;
  *size = fileSize;
  return true;
}

static bool drawJpegImage(const String &path) {
  uint8_t *jpegData = nullptr;
  size_t jpegSize = 0;
  if (!readWholeFile(path, &jpegData, &jpegSize)) return false;

  esp_jpeg_image_cfg_t infoCfg = {};
  infoCfg.indata = jpegData;
  infoCfg.indata_size = jpegSize;
  infoCfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
  infoCfg.out_scale = JPEG_IMAGE_SCALE_0;

  esp_jpeg_image_output_t originalInfo = {};
  esp_err_t err = esp_jpeg_get_image_info(&infoCfg, &originalInfo);
  if (err != ESP_OK) {
    Serial.printf("esp_jpeg_get_image_info failed: 0x%x\r\n", (unsigned)err);
    free(jpegData);
    return false;
  }

  // Decode at original resolution, then use one common scaler for JPEG and PNG.
  // This keeps same-size images visually consistent on the 320x480 display.
  esp_jpeg_image_cfg_t decodeCfg = infoCfg;
  decodeCfg.out_scale = JPEG_IMAGE_SCALE_0;
  decodeCfg.flags.swap_color_bytes = 1;

  esp_jpeg_image_output_t scaledInfo = {};
  err = esp_jpeg_get_image_info(&decodeCfg, &scaledInfo);
  if (err != ESP_OK) {
    Serial.printf("esp_jpeg_get_scaled_info failed: 0x%x\r\n", (unsigned)err);
    free(jpegData);
    return false;
  }

  uint16_t *decoded = static_cast<uint16_t *>(ps_malloc(scaledInfo.output_len));
  if (!decoded) {
    free(jpegData);
    return false;
  }

  decodeCfg.outbuf = reinterpret_cast<uint8_t *>(decoded);
  decodeCfg.outbuf_size = scaledInfo.output_len;

  err = esp_jpeg_decode(&decodeCfg, &scaledInfo);
  free(jpegData);
  if (err != ESP_OK) {
    Serial.printf("esp_jpeg_decode failed: 0x%x\r\n", (unsigned)err);
    free(decoded);
    return false;
  }

  Serial.printf("JPEG decoded: original=%ux%u scaled=%ux%u scale=%u len=%u\r\n",
                originalInfo.width, originalInfo.height, scaledInfo.width, scaledInfo.height,
                (unsigned)decodeCfg.out_scale, (unsigned)scaledInfo.output_len);
  drawScaledRgb565Be(decoded, scaledInfo.width, scaledInfo.height);
  free(decoded);
  return true;
}

static bool drawPngImage(const String &path) {
  uint8_t *pngData = nullptr;
  size_t pngSize = 0;
  if (!readWholeFile(path, &pngData, &pngSize)) {
    Serial.printf("PNG read failed: %s\r\n", path.c_str());
    return false;
  }

  unsigned char *rgba = nullptr;
  unsigned pngW = 0;
  unsigned pngH = 0;
  // lodepng allocates the output buffer through LVGL memory because
  // LV_MEM_CUSTOM is enabled in build_opt.h.
  unsigned err = lodepng_decode32(&rgba, &pngW, &pngH, pngData, pngSize);
  free(pngData);

  if (err != 0 || !rgba) {
    Serial.printf("lodepng_decode32 failed: err=%u\r\n", err);
    return false;
  }

  Serial.printf("PNG decoded: %ux%u len=%u\r\n", pngW, pngH, (unsigned)(pngW * pngH * 4));

  drawScaledRgbaLvgl(rgba, pngW, pngH);

  lv_mem_free(rgba);
  return true;
}

static void drawCurrentImage() {
  if (imageCount == 0) return;
  if (!ensureFrameBuffer()) {
    showCenteredMessage("No PSRAM frame buffer");
    return;
  }

  String path = imagePathAt(imageIndex);
  Serial.printf("Show image %d/%u: %s, mode=WIDTH_FIT\r\n",
                imageIndex + 1, imageCount, path.c_str());

  clearFrameBuffer(RGB565_BLACK);
  bool ok = false;
  if (isJpegFile(images[imageIndex])) {
    ok = drawJpegImage(path);
  } else if (isPngFile(images[imageIndex])) {
    ok = drawPngImage(path);
  }

  if (ok) {
    presentFrameBuffer();
  } else {
    showCenteredMessage("Image decode failed");
  }
}

static void nextImage(int delta) {
  if (imageCount == 0) return;
  imageIndex = (imageIndex + delta + imageCount) % imageCount;
  imageNeedsRedraw = true;
}

static void pollImageTouch() {
  static bool wasPressed = false;
  static int16_t startX = 0;
  static int16_t startY = 0;
  static int16_t lastX = 0;
  static int16_t lastY = 0;

  bool pressed = touch.touched();
  if (pressed) {
    CST_TS_Point point = touch.getPoint();
    lastX = constrain(point.x, 0, (int16_t)gfx->width() - 1);
    lastY = constrain(point.y, 0, (int16_t)gfx->height() - 1);
    if (!wasPressed) {
      startX = lastX;
      startY = lastY;
      wasPressed = true;
    }
    return;
  }

  if (!wasPressed) return;
  wasPressed = false;

  int16_t dx = lastX - startX;
  int16_t dy = lastY - startY;

  if (abs(dx) > 60 && abs(dx) > abs(dy)) {
    nextImage(dx < 0 ? 1 : -1);
  }
}

static bool isImageFile(const char *name) {
  String lower(name);
  lower.toLowerCase();
  return isImageFile(lower);
}

static void scanImages() {
  imageCount = 0;
  if (!sdReady) return;
  File dir = SD_MMC.open(IMAGE_DIR);
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f && imageCount < MAX_IMAGES; f = dir.openNextFile()) {
    String name = baseNameOf(f.name());
    if (!f.isDirectory() && isImageFile(name)) {
      images[imageCount++] = name;
    }
    f.close();
  }
  sortImages();
}

static void buildUi() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  if (!sdReady) {
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_width(label, 250);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), 0);
    lv_label_set_text(label, " No SD card detected. Please insert it and then power on again.");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  } else if (imageCount == 0) {
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "#ff0000  Image not found. #");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  } else {
    // Image pixels are drawn directly from the PSRAM frame buffer. Avoid LVGL
    // refreshes over the image page, otherwise LVGL can overwrite the LCD.
    imageBrowserMode = true;
    imageNeedsRedraw = true;
  }
}

void setup() {
  boardBegin();
  mountSdCard();
  scanImages();
  buildUi();
  if (!imageBrowserMode) lv_timer_handler();
  if (imageNeedsRedraw) {
    imageNeedsRedraw = false;
    drawCurrentImage();
  }
}

void loop() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  if (imageBrowserMode) {
    pollImageTouch();
  } else {
    lv_timer_handler();
  }
  if (imageNeedsRedraw) {
    imageNeedsRedraw = false;
    drawCurrentImage();
  }
  delay(5);
}
