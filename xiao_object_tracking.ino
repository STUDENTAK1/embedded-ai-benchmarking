#include "esp_camera.h"

/**
 * @section Experimental Configuration
 * Standardized parameters to ensure experimental reproducibility across test runs.
 */
#define RUN_NUMBER      1
#define TARGET_COLOUR   3
#define RGB565_SWAP     1         // Byte-order correction for ESP32S3 architecture
#define WARMUP_MS       5000      // Sensor stabilization period (auto-exposure/white-balance)

/**
 * @section Hardware Peripheral Mapping
 * GPIO assignments specific to the Seeed Studio XIAO ESP32S3 Sense hardware.
 */
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40
#define SIOC_GPIO_NUM    39
#define Y9_GPIO_NUM      48
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15
#define VSYNC_GPIO_NUM   38
#define HREF_GPIO_NUM    47
#define PCLK_GPIO_NUM    13

static uint32_t t0 = 0;

/**
 * @brief Performs 16-bit byte-swapping for Big-Endian to Little-Endian conversion.
 * @param buf Pointer to the frame buffer.
 * @param idx Linear index of the pixel.
 * @return Swapped 16-bit RGB565 pixel value.
 */
static inline uint16_t px16(const uint16_t *buf, int idx) {
  uint16_t p = buf[idx];
#if RGB565_SWAP
  p = (p >> 8) | (p << 8);
#endif
  return p;
}

/**
 * @brief Heuristic-based color classification logic for target identification.
 * Implements a dominance threshold to differentiate target hue from background noise.
 * @param p 16-bit RGB565 pixel.
 * @return Boolean result of target classification.
 */
static inline bool isTarget(uint16_t p) {
  uint8_t R = (p >> 11) & 0x1F; R = (R << 3);
  uint8_t G = (p >> 5)  & 0x3F; G = (G << 2);
  uint8_t B = (p & 0x1F);       B = (B << 3);

  // Target dominance thresholds: adjusted for low-light/distance sensitivity
  return (B > 45) && (B > R + 40) && (B > G + 30);
}

void setup() {
  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QVGA;
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    while (1) { Serial.println("SYSTEM_ERR,init_failed"); delay(1000); }
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

  // Static sensor parameters to minimize temporal aliasing
  s->set_saturation(s, 2);
  s->set_brightness(s, 1);
  s->set_gain_ctrl(s, 1);

  delay(WARMUP_MS);

  // Lock White Balance and Gain to maintain consistent lighting conditions during data collection
  s->set_whitebal(s, 0);
  s->set_gain_ctrl(s, 0);

  t0 = millis();

  /**
   * @section Output Format
   * Standardized CSV layout aligned with prior HuskyLens runs.
   * time_ms, det_count, colour_id, x, y, w, h
   * No-detection rows must be: 0,-1,-1,-1,-1,-1
   */
  Serial.println("time_ms,det_count,colour_id,x,y,w,h");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  int W = fb->width, H = fb->height;
  const uint16_t *buf = (const uint16_t*)fb->buf;

  int xmin = W, ymin = H, xmax = -1, ymax = -1, hits = 0;

  /**
   * @section Spatial Localization Pipeline
   * Exhaustive pixel-wise scanning to identify target clusters and derive
   * bounding box extents (Min/Max coordinates).
   */
  for (int y = 0; y < H; y++) {
    int row = y * W;
    for (int x = 0; x < W; x++) {
      if (isTarget(px16(buf, row + x))) {
        hits++;
        if (x < xmin) xmin = x;
        if (y < ymin) ymin = y;
        if (x > xmax) xmax = x;
        if (y > ymax) ymax = y;
      }
    }
  }

  int det_count = 0;
  int colour_id = -1;
  int cx = -1, cy = -1, bw = -1, bh = -1;

  /**
   * @section Bounding Box Validation
   * Validates the identified target based on spatial area and pixel density.
   * Filters out high-frequency sensor noise and full-frame artifacts.
   */
  if (hits > 20 && xmax >= 0) {
    int tmp_w = xmax - xmin;
    int tmp_h = ymax - ymin;

    // Condition: target must not fill the frame and must exceed minimum size
    if (tmp_w < (W * 0.6) && tmp_h < (H * 0.6) && tmp_w > 10 && tmp_h > 10) {
      det_count = 1;
      colour_id = TARGET_COLOUR;
      bw = tmp_w;
      bh = tmp_h;
      cx = xmin + (bw / 2); // Centroid X-axis
      cy = ymin + (bh / 2); // Centroid Y-axis
    }
  }

  Serial.print(millis() - t0); Serial.print(",");
  Serial.print(det_count);     Serial.print(",");
  Serial.print(colour_id);     Serial.print(",");
  Serial.print(cx);            Serial.print(",");
  Serial.print(cy);            Serial.print(",");
  Serial.print(bw);            Serial.print(",");
  Serial.println(bh);

  esp_camera_fb_return(fb);
}