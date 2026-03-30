#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"
#include "freertos/queue.h"

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

#define SERVO_PIN_Y          3
#define SERVO_PIN_X          4
#define SERVO_MODE          LEDC_LOW_SPEED_MODE
#define SERVO_TIMER_Y         LEDC_TIMER_1
#define SERVO_CHANNEL_Y       LEDC_CHANNEL_1
#define SERVO_TIMER_X         LEDC_TIMER_2
#define SERVO_CHANNEL_X       LEDC_CHANNEL_2
#define SERVO_DUTY_RES      LEDC_TIMER_13_BIT
#define SERVO_FREQUENCY     50

#define MIN_PULSE_WIDTH     500
#define MAX_PULSE_WIDTH     2500

#define CAM_WIDTH 240
#define CAM_HEIGHT 240
#define CAM_MID_X 120
#define CAM_MID_Y 120

QueueHandle_t servo_queue;

typedef struct {
    int x;
    int y;
} face_coords_t;

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

static uint32_t angle_to_duty(int angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    uint32_t pulse_width = MIN_PULSE_WIDTH + (((MAX_PULSE_WIDTH - MIN_PULSE_WIDTH) * angle) / 180);

    uint32_t duty = (pulse_width * ((1 << 13) - 1)) / 20000;
    return duty;
}

void servo_task(void *arg)
{
    ledc_timer_config_t ledc_timer_y = {};
    ledc_timer_y.speed_mode       = SERVO_MODE;
    ledc_timer_y.duty_resolution  = SERVO_DUTY_RES;
    ledc_timer_y.timer_num        = SERVO_TIMER_Y;
    ledc_timer_y.freq_hz          = SERVO_FREQUENCY;
    ledc_timer_y.clk_cfg          = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_y));

    ledc_timer_config_t ledc_timer_x = {};
    ledc_timer_x.speed_mode       = SERVO_MODE;
    ledc_timer_x.duty_resolution  = SERVO_DUTY_RES;
    ledc_timer_x.timer_num        = SERVO_TIMER_X;
    ledc_timer_x.freq_hz          = SERVO_FREQUENCY;
    ledc_timer_x.clk_cfg          = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_x));

    // 2. Configure the Channels for X and Y servos
    ledc_channel_config_t ledc_channel_y = {};
    ledc_channel_y.speed_mode     = SERVO_MODE;
    ledc_channel_y.channel        = SERVO_CHANNEL_Y;
    ledc_channel_y.timer_sel      = SERVO_TIMER_Y;
    ledc_channel_y.intr_type      = LEDC_INTR_DISABLE;
    ledc_channel_y.gpio_num       = SERVO_PIN_Y;
    ledc_channel_y.duty           = angle_to_duty(90);
    ledc_channel_y.hpoint         = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_y));

    ledc_channel_config_t ledc_channel_x = {};
    ledc_channel_x.speed_mode     = SERVO_MODE;
    ledc_channel_x.channel        = SERVO_CHANNEL_X;
    ledc_channel_x.timer_sel      = SERVO_TIMER_X;
    ledc_channel_x.intr_type      = LEDC_INTR_DISABLE;
    ledc_channel_x.gpio_num       = SERVO_PIN_X;
    ledc_channel_x.duty           = angle_to_duty(90);
    ledc_channel_x.hpoint         = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_x));

    ESP_LOGI(TAG, "Servos initialized: X on GPIO %d, Y on GPIO %d", SERVO_PIN_X, SERVO_PIN_Y);

    float current_angle_x = 90.0;
    float target_angle_x = 90.0;
    float current_angle_y = 90.0;
    float target_angle_y = 90.0;
    
    //60 degrees / 240 pixels = 0.25 deg/pixel
    float degrees_per_pixel = 0.25; 
    

    while (1) {
        face_coords_t coords;

        if (xQueueReceive(servo_queue, &coords, 0)) {
            
            target_angle_x = current_angle_x - (coords.x - CAM_MID_X) * degrees_per_pixel;
            target_angle_y = current_angle_y + (coords.y - CAM_MID_Y) * degrees_per_pixel;
            
            if (target_angle_x < 0) target_angle_x = 0;
            if (target_angle_x > 180) target_angle_x = 180;
            if (target_angle_y < 60) target_angle_y = 60;
            if (target_angle_y > 120) target_angle_y = 120;

            ESP_LOGI(TAG, "Face: (%d, %d) | Tgt: (%.1f, %.1f)", 
                     coords.x, coords.y, target_angle_x, target_angle_y);
        }

        float smoothing = 0.15; // Tuning knob: 0.05 is very slow/smooth, 0.5 is fast/snappy

        current_angle_x = (current_angle_x * (1.0 - smoothing)) + (target_angle_x * smoothing);
        current_angle_y = (current_angle_y * (1.0 - smoothing)) + (target_angle_y * smoothing);

        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_X, angle_to_duty((int)current_angle_x)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_X));
        
        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_Y, angle_to_duty((int)current_angle_y)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_Y));

        // Sleep to match the exact 50Hz refresh rate of an MG90S
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void face_detect_task(void *arg)
{
    // Startup delay to let power stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Starting Face Detector on Xiao ESP32S3 Sense");

    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera");
        return;
    }

    ESP_LOGI(TAG, "Camera initialized successfully");

    HumanFaceDetect detector;
    
    face_coords_t coords;
    coords.x = -1;
    coords.y = -1;

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
                coords.x = (result.box[0] + result.box[2]) / 2;
                coords.y = (result.box[1] + result.box[3]) / 2;
                xQueueSend(servo_queue, &coords, 0);
            }
        }

        // Return frame buffer
        esp_camera_fb_return(fb);

    }
}


extern "C" void app_main(void)
{
    servo_queue = xQueueCreate(1, sizeof(face_coords_t));

    xTaskCreatePinnedToCore(servo_task, "Servo", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(face_detect_task, "FaceDetect", 8192, NULL, 5, NULL, 1);
}
