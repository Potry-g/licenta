#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"

static const char *TAG = "face_detector";

// Xiao ESP32S3 Sense Camera Pinout
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

static esp_err_t init_camera(void)
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 10000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_240X240;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    return ESP_OK;
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Face Detector on Xiao ESP32S3 Sense");

    // Initialize camera
    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera");
        return;
    }

    ESP_LOGI(TAG, "Camera initialized successfully");

    // Create face detection model
    HumanFaceDetect detector;

    // Variables for FPS calculation
    int frame_count = 0;
    int64_t last_time = esp_timer_get_time();
    
    // Variables for average coordinates
    long long sum_x1 = 0, sum_y1 = 0, sum_x2 = 0, sum_y2 = 0;
    int total_faces = 0;

    while (1) {
        // Get frame from camera
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            continue;
        }

        // Prepare image for detection
        dl::image::img_t img = {
            .data = fb->buf,
            .width = (uint16_t)fb->width,
            .height = (uint16_t)fb->height,
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565
        };

        // Detect faces
        std::list<dl::detect::result_t> results = detector.run(img);

        frame_count++;

        if (!results.empty()) {
            for (const auto &result : results) {
                sum_x1 += result.box[0];
                sum_y1 += result.box[1];
                sum_x2 += result.box[2];
                sum_y2 += result.box[3];
                total_faces++;
            }
        }

        // Return frame buffer
        esp_camera_fb_return(fb);

        // Check if 1 second has passed
        int64_t current_time = esp_timer_get_time();
        if (current_time - last_time >= 1000000) {
            float fps = (float)frame_count / ((current_time - last_time) / 1000000.0f);
            
            if (total_faces > 0) {
                ESP_LOGI(TAG, "FPS: %.2f | Detections: %d | Avg Face: (%lld, %lld) - (%lld, %lld)", 
                         fps, total_faces,
                         sum_x1 / total_faces, sum_y1 / total_faces,
                         sum_x2 / total_faces, sum_y2 / total_faces);
            } else {
                ESP_LOGI(TAG, "FPS: %.2f | No faces detected", fps);
            }

            // Reset counters
            frame_count = 0;
            sum_x1 = 0; sum_y1 = 0; sum_x2 = 0; sum_y2 = 0;
            total_faces = 0;
            last_time = current_time;
        }
    }
}
