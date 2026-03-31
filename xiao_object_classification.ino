#include <XIAO_Object_Classification_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

// XIAO ESP32S3 Sense camera pins
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

#define CAM_WIDTH  320
#define CAM_HEIGHT 240

static bool is_initialised = false;
static uint8_t *snapshot_buf = nullptr;
static uint8_t *rgb888_buf   = nullptr;
unsigned long startTime;

static camera_config_t camera_config = {
    .pin_pwdn     = PWDN_GPIO_NUM,
    .pin_reset    = RESET_GPIO_NUM,
    .pin_xclk     = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
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
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_LATEST,
};

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t pixel_ix   = offset * 3;
    size_t out_ptr_ix = 0;
    while (out_ptr_ix < length) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix+1] << 8) + snapshot_buf[pixel_ix+2];
        out_ptr_ix++;
        pixel_ix += 3;
    }
    return 0;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!psramFound()) {
        Serial.println("ERROR: PSRAM not found!");
        while(1);
    }
    Serial.println("PSRAM OK");

    snapshot_buf = (uint8_t*)ps_malloc(EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * 3);
    rgb888_buf   = (uint8_t*)ps_malloc(CAM_WIDTH * CAM_HEIGHT * 3);

    if (!snapshot_buf || !rgb888_buf) {
        Serial.println("ERROR: Buffer allocation failed!");
        while(1);
    }

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed 0x%x\n", err);
        while(1);
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);

    is_initialised = true;
    Serial.println("Camera OK");
    startTime = millis();
    Serial.println("time_ms,det_count,class_id,x,y,w,h");
    delay(2000);
}

void loop() {
    if (!is_initialised) return;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;

    uint16_t *src = (uint16_t*)fb->buf;
    for (int i = 0; i < CAM_WIDTH * CAM_HEIGHT; i++) {
        uint16_t px = src[i];
        rgb888_buf[i*3+0] = ((px >> 11) & 0x1F) << 3;
        rgb888_buf[i*3+1] = ((px >> 5)  & 0x3F) << 2;
        rgb888_buf[i*3+2] = (px & 0x1F) << 3;
    }
    esp_camera_fb_return(fb);

    ei::image::processing::crop_and_interpolate_rgb888(
        rgb888_buf, CAM_WIDTH, CAM_HEIGHT,
        snapshot_buf, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        Serial.printf("ERR: classifier failed (%d)\n", err);
        return;
    }

    unsigned long t = millis() - startTime;
    int det_count = 0;

    #if EI_CLASSIFIER_OBJECT_DETECTION == 1
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) continue;
        if (String(bb.label) == "BACKGROUND") continue;
        det_count++;
        int class_id = -1;
        if      (String(bb.label) == "OBJECTA") class_id = 1;
        else if (String(bb.label) == "OBJECTB") class_id = 2;
        else if (String(bb.label) == "OBJECTC") class_id = 3;

        Serial.print(t);                     Serial.print(",");
        Serial.print(det_count);             Serial.print(",");
        Serial.print(class_id);              Serial.print(",");
        Serial.print(bb.x + bb.width / 2);  Serial.print(",");
        Serial.print(bb.y + bb.height / 2); Serial.print(",");
        Serial.print(bb.width);              Serial.print(",");
        Serial.println(bb.height);
    }
    if (det_count == 0) {
        Serial.print(t);
        Serial.println(",0,-1,-1,-1,-1,-1");
    }
    #endif
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif