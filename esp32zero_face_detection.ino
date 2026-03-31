#include <Face_detection_-_FOMO_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

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

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS  160
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS  120
#define EI_CAMERA_FRAME_BYTE_SIZE         3

#define RUN_NUMBER    1
#define WARMUP_MS     5000

static bool is_initialised = false;
static uint8_t *snapshot_buf = nullptr;
static uint32_t t0 = 0;

static camera_config_t camera_config = {
  .pin_pwdn     = PWDN_GPIO_NUM,
  .pin_reset    = RESET_GPIO_NUM,
  .pin_xclk     = XCLK_GPIO_NUM,
  .pin_sccb_sda = SIOD_GPIO_NUM,
  .pin_sccb_scl = SIOC_GPIO_NUM,
  .pin_d7       = Y9_GPIO_NUM,
  .pin_d6       = Y8_GPIO_NUM,
  .pin_d5       = Y7_GPIO_NUM,
  .pin_d4       = Y6_GPIO_NUM,
  .pin_d3       = Y5_GPIO_NUM,
  .pin_d2       = Y4_GPIO_NUM,
  .pin_d1       = Y3_GPIO_NUM,
  .pin_d0       = Y2_GPIO_NUM,
  .pin_vsync    = VSYNC_GPIO_NUM,
  .pin_href     = HREF_GPIO_NUM,
  .pin_pclk     = PCLK_GPIO_NUM,
  .xclk_freq_hz = 20000000,
  .ledc_timer   = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_RGB565,   // direct RGB565, no JPEG
  .frame_size   = FRAMESIZE_QQVGA,    // 160x120
  .fb_count     = 1,
  .fb_location  = CAMERA_FB_IN_PSRAM,
  .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

bool ei_camera_init(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!ei_camera_init()) {
    Serial.println("Camera init FAILED");
    while (1) delay(1000);
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_whitebal(s, 1);
  s->set_saturation(s, 0);
  s->set_brightness(s, 1);
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);

  Serial.println("Warming up sensor...");
  delay(WARMUP_MS);
  s->set_whitebal(s, 0);

  t0 = millis();
  Serial.println("time_ms,det_count,class_id,x,y,w,h");
}

void loop() {
  snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS *
                                   EI_CAMERA_RAW_FRAME_BUFFER_ROWS *
                                   EI_CAMERA_FRAME_BYTE_SIZE);
  if (!snapshot_buf) {
    Serial.println("ERR: malloc failed");
    return;
  }

  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data     = &ei_camera_get_data;

  if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
    Serial.println("ERR: capture failed");
    free(snapshot_buf);
    return;
  }

  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
    Serial.print("ERR: classifier failed: ");
    Serial.println(err);
    free(snapshot_buf);
    return;
  }

  uint32_t time_ms = millis() - t0;

  int det_count = 0;
  for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
    if (result.bounding_boxes[i].value > 0) det_count++;
  }

  if (det_count == 0) {
    Serial.print(time_ms); Serial.print(",0,-1,-1,-1,-1,-1");
    Serial.println();
  } else {
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
      ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
      if (bb.value == 0) continue;
      Serial.print(time_ms);   Serial.print(",");
      Serial.print(det_count); Serial.print(",");
      Serial.print(1);         Serial.print(",");
      Serial.print(bb.x + bb.width  / 2); Serial.print(",");
      Serial.print(bb.y + bb.height / 2); Serial.print(",");
      Serial.print(bb.width);  Serial.print(",");
      Serial.println(bb.height);
    }
  }

  free(snapshot_buf);
}

bool ei_camera_init(void) {
  if (is_initialised) return true;
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  is_initialised = true;
  return true;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
  if (!is_initialised) return false;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;

  // Convert RGB565 → RGB888 manually
  uint16_t *src = (uint16_t*)fb->buf;
  uint8_t  *dst = out_buf;
  for (int i = 0; i < EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS; i++) {
    uint16_t px = src[i];
    // swap bytes for OV2640
    px = (px >> 8) | (px << 8);
    dst[i*3 + 0] = ((px >> 11) & 0x1F) << 3; // R
    dst[i*3 + 1] = ((px >>  5) & 0x3F) << 2; // G
    dst[i*3 + 2] = ( px        & 0x1F) << 3; // B
  }

  esp_camera_fb_return(fb);

  // Crop and resize to EI input size (96x96)
  if (img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS ||
      img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS) {
    ei::image::processing::crop_and_interpolate_rgb888(
      out_buf,
      EI_CAMERA_RAW_FRAME_BUFFER_COLS,
      EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
      out_buf,
      img_width,
      img_height);
  }
  return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
  size_t pixel_ix   = offset * 3;
  size_t out_ptr_ix = 0;
  while (out_ptr_ix < length) {
    out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) +
                          (snapshot_buf[pixel_ix + 1] <<  8) +
                           snapshot_buf[pixel_ix];
    out_ptr_ix++;
    pixel_ix += 3;
  }
  return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif