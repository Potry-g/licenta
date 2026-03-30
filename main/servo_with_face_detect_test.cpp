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

// Configuration
#define SERVO_PIN           3       // D0 on Xiao ESP32S3 (GPIO 3)
#define SERVO_MODE          LEDC_LOW_SPEED_MODE
#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL       LEDC_CHANNEL_0
#define SERVO_DUTY_RES      LEDC_TIMER_13_BIT // 13-bit resolution (0-8191)
#define SERVO_FREQUENCY     50      // 50Hz for standard servos

// Pulse width limits for MG90S (in microseconds)
// These might need slight tuning for your specific servo
#define MIN_PULSE_WIDTH     500     // 0 degrees
#define MAX_PULSE_WIDTH     2500    // 180 degrees

#define CAM_WIDTH 240
#define CAM_HEIGHT 240
#define CAM_MID_X 120
#define CAM_MID_Y 120

static uint32_t angle_to_duty(int angle)
{
    // Clamp angle
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // Calculate pulse width in microseconds
    uint32_t pulse_width = MIN_PULSE_WIDTH + (((MAX_PULSE_WIDTH - MIN_PULSE_WIDTH) * angle) / 180);

    // Calculate duty cycle for LEDC
    // Duty = (PulseWidth / Period) * (2^Resolution - 1)
    // Period = 1000000 / Frequency = 20000 us
    uint32_t duty = (pulse_width * ((1 << 13) - 1)) / 20000;
    return duty;
}

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

    // 1. Setup Timer
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode       = SERVO_MODE;
    ledc_timer.duty_resolution  = SERVO_DUTY_RES;
    ledc_timer.timer_num        = SERVO_TIMER;
    ledc_timer.freq_hz          = SERVO_FREQUENCY;
    ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Setup Channel
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode     = SERVO_MODE;
    ledc_channel.channel        = SERVO_CHANNEL;
    ledc_channel.timer_sel      = SERVO_TIMER;
    ledc_channel.intr_type      = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num       = SERVO_PIN;
    ledc_channel.duty           = 0; // Start at 0
    ledc_channel.hpoint         = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));


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

    int face_x = -1;
    
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

        if (!results.empty()) {
            for (const auto &result : results) {
                face_x = (result.box[0] + result.box[2]) / 2;

            }
        }
        else    {
            face_x = -1;
        }

        if (face_x != -1) {
            int angle = (face_x - CAM_MID_X) * 180 / CAM_WIDTH + 90;
            ESP_LOGI(TAG, "Face detected at x: %d, angle: %d", face_x, angle);
            ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(angle)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL));
        }


        // Return frame buffer
        esp_camera_fb_return(fb);

    }
}
