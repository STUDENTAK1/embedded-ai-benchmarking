#include "esp_camera.h"

// --- TEST SETTINGS ---
#define RUN_NUMBER      1
#define TARGET_COLOUR   3        // 1=RED, 2=GREEN, 3=BLUE
#define RGB565_SWAP     1
#define WARMUP_MS       5000

// --- XIAO ESP32S3 PINS ---
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

static inline uint16_t px16(const uint16_t *buf, int idx) {
  uint16_t p = buf[idx];
#if RGB565_SWAP
  p = (p >> 8) | (p << 8);
#endif
  return p;
}

// --- BLUE DETECTION LOGIC (as you had it) ---
static inline bool isTarget(uint16_t p) {
  uint8_t R = (p >> 11) & 0x1F; R = (R << 3);
  uint8_t G = (p >> 5)  & 0x3F; G = (G << 2);
  uint8_t B = (p & 0x1F);       B = (B << 3);

  bool is_bright = (B > 90);
  bool beats_red = (B > R + 55) && (G > R + 40);
  bool is_cyan   = (B > G - 30);

  return is_bright && beats_red && is_cyan;
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
    while (1) { Serial.println("ERR,camera_init_failed"); delay(1000); }
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
  s->set_saturation(s, 2);
  s->set_brightness(s, 0);
  s->set_contrast(s, 1);

  delay(WARMUP_MS);
  s->set_whitebal(s, 0);

  t0 = millis();

  // HuskyLens-consistent output format
  Serial.println("time_ms,det_count,colour_id,x,y,w,h");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  int W = fb->width, H = fb->height;
  const uint16_t *buf = (const uint16_t*)fb->buf;

  int xmin = W, ymin = H, xmax = -1, ymax = -1, hits = 0;

  for (int y = 0; y < H; y += 2) {
    int row = y * W;
    for (int x = 0; x < W; x += 2) {
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

  if (hits > 60 && xmax >= 0) {
    int tmp_w = xmax - xmin;
    int tmp_h = ymax - ymin;

    if (tmp_w > 10 && tmp_h > 10 && tmp_w < (W * 0.9) && tmp_h < (H * 0.9)) {
      det_count = 1;
      colour_id = TARGET_COLOUR;
      bw = tmp_w;
      bh = tmp_h;
      cx = xmin + (bw / 2);
      cy = ymin + (bh / 2);
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
  delay(20);
}