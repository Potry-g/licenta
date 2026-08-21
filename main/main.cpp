#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <algorithm>

static const char *TAG = "vision_tracker";

// Tracking modes
enum TrackingMode { MODE_FACE, MODE_COLOR };
volatile TrackingMode current_mode = MODE_FACE;

#define BUTTON_PIN 1

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

// --- Button ISR ---
static void IRAM_ATTR button_isr_handler(void* arg) {
    static uint32_t last_isr_time = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Simple software debounce (200ms)
    if (current_time - last_isr_time > 200) {
        if (current_mode == MODE_FACE) {
            current_mode = MODE_COLOR;
        } else {
            current_mode = MODE_FACE;
        }
        last_isr_time = current_time;
    }
}

// --- HSV Color Detection ---
static void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float &h, float &s, float &v) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max_c = rf;
    if (gf > max_c) max_c = gf;
    if (bf > max_c) max_c = bf;

    float min_c = rf;
    if (gf < min_c) min_c = gf;
    if (bf < min_c) min_c = bf;

    float delta = max_c - min_c;

    v = max_c;
    if (max_c > 0.0f) {
        s = delta / max_c;
    } else {
        s = 0.0f;
    }

    if (s == 0.0f) {
        h = 0.0f;
    } else {
        if (max_c == rf) {
            h = 60.0f * (gf - bf) / delta;
            if (h < 0.0f) h += 360.0f;
        } else if (max_c == gf) {
            h = 60.0f * (2.0f + (bf - rf) / delta);
        } else {
            h = 60.0f * (4.0f + (rf - gf) / delta);
        }
    }
}

static bool find_color_blob(camera_fb_t *fb, face_coords_t *coords) {
    const int CELL_SIZE = 16;
    const int GRID_W = 15;
    const int GRID_H = 15;
    
    int grid[GRID_H][GRID_W] = {0};
    
    // --- PASS 1: Find the densest grid cell ---
    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            int i = (y * fb->width + x) * 2;

            // Extract RGB from big-endian RGB565
            uint16_t pixel = ((uint16_t)fb->buf[i] << 8) | fb->buf[i + 1];
            uint8_t r = (pixel >> 8) & 0xF8;
            uint8_t g = (pixel >> 3) & 0xFC;
            uint8_t b = (pixel << 3)  & 0xF8;
            
            float h, s, v;
            rgb_to_hsv(r, g, b, h, s, v);
            
            // Blue threshold: Hue around 227 (range 200-255), enough saturation and brightness
            if (h > 200.0f && h < 255.0f && s > 0.4f && v > 0.2f) {
                int gx = x / CELL_SIZE;
                int gy = y / CELL_SIZE;
                if (gx < GRID_W && gy < GRID_H) {
                    grid[gy][gx]++;
                }
            }
        }
    }
    
    // Find the grid cell with the highest concentration of blue pixels
    int max_count = 0;
    int max_gx = -1;
    int max_gy = -1;
    
    for (int gy = 0; gy < GRID_H; gy++) {
        for (int gx = 0; gx < GRID_W; gx++) {
            if (grid[gy][gx] > max_count) {
                max_count = grid[gy][gx];
                max_gx = gx;
                max_gy = gy;
            }
        }
    }
    
    // INCREASED THRESHOLD: A single grid cell must have at least 25 valid pixels 
    // to be considered a valid object (ignores small noise)
    if (max_count < 60) {
        return false;
    }
    
    // --- PASS 2: Calculate precise center of the blob ---
    // Only look at the 3x3 grid cells around the densest cell to filter out noise
    int total_x = 0;
    int total_y = 0;
    int final_count = 0;
    
    // Calculate pixel boundaries for our 3x3 search window
    int start_y = std::max(0, (max_gy - 1) * CELL_SIZE);
    int end_y = std::min((int)fb->height, (max_gy + 2) * CELL_SIZE);
    int start_x = std::max(0, (max_gx - 1) * CELL_SIZE);
    int end_x = std::min((int)fb->width, (max_gx + 2) * CELL_SIZE);
    
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            int i = (y * fb->width + x) * 2;

            uint16_t pixel = ((uint16_t)fb->buf[i] << 8) | fb->buf[i + 1];
            uint8_t r = (pixel >> 8) & 0xF8;
            uint8_t g = (pixel >> 3) & 0xFC;
            uint8_t b = (pixel << 3)  & 0xF8;
            
            float h, s, v;
            rgb_to_hsv(r, g, b, h, s, v);
            
            if (h > 200.0f && h < 255.0f && s > 0.4f && v > 0.2f) {
                total_x += x;
                total_y += y;
                final_count++;
            }
        }
    }
    
    // INCREASED THRESHOLD: Ensure the total blob has at least 40 pixels
    if (final_count > 80) {
        coords->x = total_x / final_count;
        coords->y = total_y / final_count;
        return true;
    }
    
    return false;
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

    // Configure the Channels for X and Y servos
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

    float degrees_per_pixel_x = 40.0f / CAM_WIDTH;
    float degrees_per_pixel_y = 40.0f / CAM_HEIGHT;

    while (1) {
        face_coords_t coords;
        bool got_new_coords = false;

        // Drain the queue to always get the freshest coordinate
        while (xQueueReceive(servo_queue, &coords, 0)) {
            got_new_coords = true;
        }

        if (got_new_coords) {
            int err_x = coords.x - CAM_MID_X;
            int err_y = coords.y - CAM_MID_Y;
            
            // Apply a 15-pixel deadband so it doesn't vibrate when already centered
            if (err_x > 15 || err_x < -15) {
                target_angle_x = current_angle_x - err_x * degrees_per_pixel_x;
            }
            if (err_y > 15 || err_y < -15) {
                target_angle_y = current_angle_y + err_y * degrees_per_pixel_y;
            }
            
            if (target_angle_x < 0) target_angle_x = 0;
            if (target_angle_x > 180) target_angle_x = 180;
            if (target_angle_y < 60) target_angle_y = 60;
            if (target_angle_y > 120) target_angle_y = 120;

            ESP_LOGD(TAG, "Target: (%d, %d) | TgtAngle: (%.1f, %.1f)", 
                     coords.x, coords.y, target_angle_x, target_angle_y);
        }

        // Use dynamic smoothing based on tracking mode to prevent color tracking oscillations
        float smoothing = (current_mode == MODE_FACE) ? 0.13 : 0.04;

        current_angle_x = (current_angle_x * (1.0 - smoothing)) + (target_angle_x * smoothing);
        current_angle_y = (current_angle_y * (1.0 - smoothing)) + (target_angle_y * smoothing);

        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_X, angle_to_duty((int)current_angle_x)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_X));
        
        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_Y, angle_to_duty((int)current_angle_y)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_Y));

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void vision_task(void *arg)
{
    // Startup delay to let power stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Starting Vision Task on Xiao ESP32S3 Sense");

    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera");
        return;
    }

    ESP_LOGI(TAG, "Camera initialized successfully");

    HumanFaceDetect detector;
    
    face_coords_t coords;
    coords.x = -1;
    coords.y = -1;
    
    TrackingMode last_reported_mode = current_mode;

    while (1) {
        if (current_mode != last_reported_mode) {
            ESP_LOGI(TAG, "Switched to %s tracking mode", current_mode == MODE_FACE ? "FACE" : "COLOR");
            last_reported_mode = current_mode;
        }

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            continue;
        }

        if (current_mode == MODE_FACE) {
            dl::image::img_t img = {
                .data = fb->buf,
                .width = (uint16_t)fb->width,
                .height = (uint16_t)fb->height,
                .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565
            };

            std::list<dl::detect::result_t> results = detector.run(img);

            if (!results.empty()) {
                for (const auto &result : results) {
                    coords.x = (result.box[0] + result.box[2]) / 2;
                    coords.y = (result.box[1] + result.box[3]) / 2;
                    xQueueSend(servo_queue, &coords, 0);
                }
            }
        } else if (current_mode == MODE_COLOR) {
            if (find_color_blob(fb, &coords)) {
                ESP_LOGD(TAG, "Color Blob found at: (%d, %d)", coords.x, coords.y);
                xQueueSend(servo_queue, &coords, 0);
            }
        }

        esp_camera_fb_return(fb);
    }
}


extern "C" void app_main(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)BUTTON_PIN, button_isr_handler, NULL);

    servo_queue = xQueueCreate(1, sizeof(face_coords_t));

    xTaskCreatePinnedToCore(servo_task, "Servo", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(vision_task, "VisionTask", 8192, NULL, 5, NULL, 1);
}
