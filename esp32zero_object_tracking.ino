#include "esp_camera.h"

#define RUN_NUMBER      1
#define TARGET_COLOUR   3
#define RGB565_SWAP     1
#define WARMUP_MS       5000

#define PWDN_GPIO_NUM    16
#define RESET_GPIO_NUM   15
#define XCLK_GPIO_NUM     6
#define SIOD_GPIO_NUM     1
#define SIOC_GPIO_NUM     2
#define Y9_GPIO_NUM      14
#define Y8_GPIO_NUM      13
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      11
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM       9
#define Y3_GPIO_NUM       8
#define Y2_GPIO_NUM       7
#define VSYNC_GPIO_NUM    3
#define HREF_GPIO_NUM     4
#define PCLK_GPIO_NUM     5

static uint32_t t0 = 0;

static inline uint16_t px16(const uint16_t *buf, int idx) {
  uint16_t p = buf[idx];
#if RGB565_SWAP
  p = (p >> 8) | (p << 8);
#endif
  return p;
}

static inline bool isTarget(uint16_t p) {
  uint8_t R = (p >> 11) & 0x1F; R = (R << 3) | (R >> 2);
  uint8_t G = (p >>  5) & 0x3F; G = (G << 2) | (G >> 4);
  uint8_t B = (p      ) & 0x1F; B = (B << 3) | (B >> 2);

  return (R < 20) && (G > 125) && (B > 125) && (G < B + 30) && (B < G + 30);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
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
  config.frame_size   = FRAMESIZE_QQVGA;
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    while (1) { Serial.println("Camera Init Failed"); delay(1000); }
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_whitebal(s, 1);
  s->set_saturation(s, 2);
  s->set_brightness(s, 1);
  s->set_contrast(s, 1);
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);

  Serial.println("Warming up...");
  delay(WARMUP_MS);
  s->set_whitebal(s, 0);
  s->set_gain_ctrl(s, 0);

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

  if (hits > 30 && xmax >= 0) {
    int tmp_w = xmax - xmin;
    int tmp_h = ymax - ymin;

    // Background under bright light spans nearly full frame — reject it
    // Lid is always smaller than 90% of frame in both dimensions
    bool valid_size = (tmp_w > 5) && (tmp_h > 5) &&
                      (tmp_w < W * 0.90) && (tmp_h < H * 0.90);

    if (valid_size) {
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