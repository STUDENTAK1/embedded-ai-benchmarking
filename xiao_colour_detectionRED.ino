#include "esp_camera.h"

// Settings for GREEN detection
#define TARGET_COLOUR      1       // 1=RED, 2=GREEN, 3=BLUE
#define RGB565_SWAP        1       // Required for XIAO ESP32S3
#define RUN_NUMBER         1

static const uint32_t WARMUP_MS = 5000; // 5 seconds to let auto-white balance settle

// XIAO ESP32S3 Sense pins
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

static uint32_t t0 = 0;

static inline uint16_t px16(const uint16_t *buf, int idx) {
  uint16_t p = buf[idx];
#if RGB565_SWAP
  p = (p >> 8) | (p << 8);
#endif
  return p;
}

static inline bool isTarget(uint16_t p) {
  // Extract RGB components
  uint8_t R = (p >> 11) & 0x1F; R = (R << 3) | (R >> 2);
  uint8_t G = (p >> 5)  & 0x3F; G = (G << 2) | (G >> 4);
  uint8_t B = (p & 0x1F);       B = (B << 3) | (B >> 2);

  // GREEN DETECTION LOGIC
  // Green is very strong on this sensor.
  // We check that G is bright (>100) and significantly higher than R and B.
  return (G > 100) && (G > R + 45) && (G > B + 45);
}

void setup() {
  Serial.begin(115200);
  delay(500);

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
  config.frame_size   = FRAMESIZE_QVGA; // 320x240
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    while (1) { Serial.println("Camera Init Failed"); delay(1000); }
  }

  sensor_t *s = esp_camera_sensor_get();

  // Settings to help Green stand out
  s->set_whitebal(s, 1);     // Auto white balance ON during warmup
  s->set_saturation(s, 2);   // Boost colors (+2 is max)
  s->set_brightness(s, 0);   // Neutral brightness
  s->set_contrast(s, 1);     // Slight contrast boost

  // Flip the image if needed (common for XIAO)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

  Serial.println("Warming up sensor...");
  delay(WARMUP_MS);

  // LOCK White Balance after warmup so colors don't shift
  s->set_whitebal(s, 0);

  t0 = millis();
  Serial.println("time_ms,det_count,colour_id,x,y,w,h");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  uint32_t time_ms = millis() - t0;
  const int W = fb->width;
  const int H = fb->height;
  const uint16_t *buf = (const uint16_t*)fb->buf;

  int xmin = W, ymin = H, xmax = -1, ymax = -1;
  int hits = 0;

  const int stride = 2; // Scan every 2nd pixel for speed/accuracy balance
  const int MIN_HITS = 60;

  for (int y = 0; y < H; y += stride) {
    int row = y * W;
    for (int x = 0; x < W; x += stride) {
      uint16_t p = px16(buf, row + x);
      if (isTarget(p)) {
        hits++;
        if (x < xmin) xmin = x;
        if (y < ymin) ymin = y;
        if (x > xmax) xmax = x;
        if (y > ymax) ymax = y;
      }
    }
  }

  int det_count = 0;
  int cx = -1, cy = -1, bw = -1, bh = -1;

  if (hits >= MIN_HITS) {
    bw = xmax - xmin;
    bh = ymax - ymin;

    // Validate box size
    if (bw >= 15 && bh >= 15) {
      det_count = 1;
      cx = xmin + bw / 2;
      cy = ymin + bh / 2;
    } else {
      bw = bh = cx = cy = -1;
    }
  }

  // Serial Output
  Serial.printf("%lu,%d,%d,%d,%d,%d,%d\n", time_ms, det_count, TARGET_COLOUR, cx, cy, bw, bh);

  esp_camera_fb_return(fb);
  delay(10); // Small delay to prevent watchdog issues
}