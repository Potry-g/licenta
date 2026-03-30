#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "servo_example";

// Configuration
#define SERVO_PIN 3 // D0 (GPIO 3)
#define SERVO_MODE          LEDC_LOW_SPEED_MODE
#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL       LEDC_CHANNEL_0
#define SERVO_DUTY_RES      LEDC_TIMER_13_BIT // 13-bit resolution (0-8191)
#define SERVO_FREQUENCY     50      // 50Hz for standard servos

// Pulse width limits for MG90S (in microseconds)
// These might need slight tuning for your specific servo
#define MIN_PULSE_WIDTH     500     // 0 degrees
#define MAX_PULSE_WIDTH     2500    // 180 degrees

// Helper to convert angle to duty cycle
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

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Servo Example Started");

    // 1. Setup Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_MODE,
        .duty_resolution  = SERVO_DUTY_RES,
        .timer_num        = SERVO_TIMER,
        .freq_hz          = SERVO_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Setup Channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_MODE,
        .channel        = SERVO_CHANNEL,
        .timer_sel      = SERVO_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN,
        .duty           = 0, // Start at 0
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    while (1) {
        ESP_LOGI(TAG, "Moving to 0 degrees");
        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(0)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Moving to 90 degrees");
        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(90)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Moving to 180 degrees");
        ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(180)));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
