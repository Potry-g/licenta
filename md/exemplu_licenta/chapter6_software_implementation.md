# Chapter 6: Software Implementation

---

## 6.1 Project Structure

The software for this project is developed using the ESP-IDF framework, version 5.x, with a CMake-based build system. The project directory structure is organized as follows:

```
licenta/
├── main/
│   ├── main.cpp              # Application entry point and all logic
│   ├── CMakeLists.txt        # Main component build definition
│   └── idf_component.yml     # Component dependency declarations
├── components/
│   ├── bsp/                  # Board support package (XIAO ESP32S3 Sense)
│   └── esp-who/              # ESP-DL face detection model and wrapper
├── CMakeLists.txt            # Top-level project build file
├── sdkconfig.defaults        # Default build configuration overrides
└── partitions.csv            # Custom flash partition table
```

All application logic resides in a single source file, `main.cpp`, which implements the camera initialization, the two FreeRTOS tasks, the hardware interrupt service routine, the HSV color blob detection algorithm, and the servo control algorithm. The `esp-who` component provides the `HumanFaceDetect` class and all associated model weights. The `bsp` component provides the pin definitions for the XIAO ESP32S3 Sense board.

---

## 6.2 FreeRTOS Task Architecture

### 6.2.1 System Overview

The application is structured around two concurrent FreeRTOS tasks that run on separate CPU cores, communicating via a shared queue. This architecture is the central design decision of the software, and its rationale is described in this section.

The fundamental challenge in this system is the mismatch between the frequency at which the vision subsystem produces new target coordinates (~12.5 FPS, one update every 80 ms) and the frequency at which the servo subsystem must be updated (50 Hz, one update every 20 ms). If servo updates were driven only by vision updates, the servo would receive a new target four times less often than its control period, resulting in jerky, stair-step motion. Conversely, if the vision task were blocked waiting for the servo update cycle, the frame capture rate would be limited by the servo timing rather than by the camera and inference engine.

The dual-core FreeRTOS architecture solves this by running the two subsystems independently and asynchronously:

- **`vision_task` on Core 1:** Runs as fast as the camera and detection algorithm permit. It is never blocked by the servo cycle and always captures the most recent frame.
- **`servo_task` on Core 0:** Runs at a precise 50 Hz rate. It reads the latest available target coordinates from the queue and interpolates toward the target each cycle, producing smooth continuous motion even when target updates are infrequent.

The two tasks are created in `app_main()` using `xTaskCreatePinnedToCore()` with equal priority (5) on their respective cores:

```cpp
xTaskCreatePinnedToCore(servo_task,  "Servo",     4096, NULL, 5, NULL, 0);
xTaskCreatePinnedToCore(vision_task, "VisionTask", 8192, NULL, 5, NULL, 1);
```

The larger stack size allocated to `vision_task` (8192 bytes vs. 4096 for `servo_task`) accommodates the deeper call stack of the ESP-DL inference pipeline and the local arrays used by the HSV detection algorithm.

### 6.2.2 Inter-Task Communication via FreeRTOS Queue

The data channel between `vision_task` and `servo_task` is a FreeRTOS queue of depth 1, carrying elements of type `face_coords_t`:

```cpp
typedef struct {
    int x;
    int y;
} face_coords_t;

QueueHandle_t servo_queue = xQueueCreate(1, sizeof(face_coords_t));
```

The depth-1 design is deliberate. When `vision_task` sends a new coordinate pair, if the queue already holds an unread coordinate from the previous detection cycle, the send fails silently (non-blocking call with timeout = 0). The old coordinate is effectively discarded. This ensures that `servo_task` always acts on the **most recently detected position**, never on stale data from several frames ago.

In `servo_task`, the queue is drained at the start of each 20 ms control cycle:

```cpp
face_coords_t coords;
bool got_new_coords = false;
while (xQueueReceive(servo_queue, &coords, 0)) {
    got_new_coords = true;
}
```

The loop consumes all available items in the queue (though at most one can be present given the depth-1 design) and sets `got_new_coords = true` only if at least one coordinate was received. If no new coordinate is available — because the vision task has not yet completed a new detection cycle — `got_new_coords` remains false and the servo task continues interpolating toward the previously set target. This is the mechanism by which smooth motion is maintained between detection updates.

### 6.2.3 Mode Switching via Hardware Interrupt

The tracking mode is stored in a `volatile` global variable of the enumeration type `TrackingMode`:

```cpp
enum TrackingMode { MODE_FACE, MODE_COLOR };
volatile TrackingMode current_mode = MODE_FACE;
```

The `volatile` qualifier instructs the compiler not to cache the variable's value in a register across function calls or loop iterations. Without this qualifier, the compiler could legally optimize the mode-check condition in `vision_task` into a single load at task startup, causing the task to miss all subsequent changes written by the ISR.

The ISR is registered for the falling edge of GPIO 1 (button press):

```cpp
gpio_config_t io_conf = {};
io_conf.intr_type    = GPIO_INTR_NEGEDGE;
io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
io_conf.mode         = GPIO_MODE_INPUT;
io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
gpio_config(&io_conf);
gpio_install_isr_service(0);
gpio_isr_handler_add((gpio_num_t)BUTTON_PIN, button_isr_handler, NULL);
```

The ISR function is marked `IRAM_ATTR` to ensure it resides in internal RAM:

```cpp
static void IRAM_ATTR button_isr_handler(void* arg) {
    static uint32_t last_isr_time = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (current_time - last_isr_time > 200) {
        current_mode = (current_mode == MODE_FACE) ? MODE_COLOR : MODE_FACE;
        last_isr_time = current_time;
    }
}
```

The debounce window of 200 ms prevents mechanical contact bounce — the rapid electrical transitions that occur in the first few milliseconds after a button press due to the elastic deformation of the contact surfaces — from registering as multiple mode toggles.

---

## 6.3 Camera Initialization

The OV2640 camera is initialized in `init_camera()` using the `esp_camera_init()` API of the ESP-IDF camera driver. The key configuration parameters are:

```cpp
config.pixel_format = PIXFORMAT_RGB565;
config.frame_size   = FRAMESIZE_240X240;
config.fb_count     = 2;
config.fb_location  = CAMERA_FB_IN_PSRAM;
config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
config.xclk_freq_hz = 10000000;  // 10 MHz master clock
```

The choice of `FRAMESIZE_240X240` produces square frames that are natively compatible with the ESP-DL model input. The `CAMERA_FB_IN_PSRAM` location is mandatory since the 115,200-byte frame buffer cannot be allocated from the 512 KB internal SRAM alongside the FreeRTOS stacks, ESP-DL model weights, and application heap. The `CAMERA_GRAB_WHEN_EMPTY` grab mode ensures that a new frame is captured only when the driver has no pending frame to deliver, preventing the accumulation of stale frames.

---

## 6.4 Face Detection Implementation

In `MODE_FACE`, the `vision_task` passes each captured frame to the `HumanFaceDetect` detector:

```cpp
HumanFaceDetect detector;

// Inside the task loop:
camera_fb_t *fb = esp_camera_fb_get();

dl::image::img_t img = {
    .data     = fb->buf,
    .width    = (uint16_t)fb->width,
    .height   = (uint16_t)fb->height,
    .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565
};

std::list<dl::detect::result_t> results = detector.run(img);

if (!results.empty()) {
    face_coords_t coords;
    coords.x = (results.front().box[0] + results.front().box[2]) / 2;
    coords.y = (results.front().box[1] + results.front().box[3]) / 2;
    xQueueSend(servo_queue, &coords, 0);
}

esp_camera_fb_return(fb);
```

The `img_t` descriptor points directly into the frame buffer in PSRAM, avoiding any pixel data copy. The detection result contains a bounding box with four coordinates: `box[0]` and `box[1]` are the top-left corner (x₁, y₁), while `box[2]` and `box[3]` are the bottom-right corner (x₂, y₂). The centroid is computed as the arithmetic mean of opposite corners. Only the first detected face is used; in scenes with multiple faces, the face returned first by the detector (which is determined internally by the model) is tracked.

If no face is detected in a frame, nothing is sent to the queue and the servo task continues toward the last known target position.

---

## 6.5 HSV Color Blob Detection

In `MODE_COLOR`, the `vision_task` calls the `find_color_blob()` function, which implements a custom two-pass grid-based detection algorithm designed for robustness against noise and isolated background artifacts.

### 6.5.1 Pass 1 — Grid Density Map

The frame is divided into a 15×15 grid of cells, each covering 16×16 pixels (15 × 16 = 240, matching the frame dimensions exactly). For each pixel in the frame, the RGB565 value is decoded and converted to HSV. If the resulting HSV values satisfy the blue color threshold, the counter of the containing grid cell is incremented:

```cpp
const int CELL_SIZE = 16;
const int GRID_W = 15, GRID_H = 15;
int grid[GRID_H][GRID_W] = {0};

for (int y = 0; y < fb->height; y++) {
    for (int x = 0; x < fb->width; x++) {
        // Decode RGB565 pixel
        int i = (y * fb->width + x) * 2;
        uint16_t pixel = ((uint16_t)fb->buf[i] << 8) | fb->buf[i + 1];
        uint8_t r = (pixel >> 8) & 0xF8;
        uint8_t g = (pixel >> 3) & 0xFC;
        uint8_t b = (pixel << 3) & 0xF8;

        float h, s, v;
        rgb_to_hsv(r, g, b, h, s, v);

        if (h > 200.0f && h < 255.0f && s > 0.4f && v > 0.2f) {
            grid[y / CELL_SIZE][x / CELL_SIZE]++;
        }
    }
}
```

After scanning the entire frame, the grid cell with the maximum pixel count is identified. If this maximum count is below the threshold of **25 pixels**, the function returns `false` — no valid blob was detected. This threshold rejects isolated noise pixels and small background artifacts that would otherwise generate spurious tracking targets.

### 6.5.2 Pass 2 — Precise Centroid Estimation

Rather than computing the centroid of all matching pixels in the entire frame — which would be slow and susceptible to interference from any blue object anywhere in the scene — the second pass restricts the search to a 3×3 neighborhood of grid cells centered on the densest cell found in Pass 1:

```cpp
int start_y = std::max(0, (max_gy - 1) * CELL_SIZE);
int end_y   = std::min((int)fb->height, (max_gy + 2) * CELL_SIZE);
int start_x = std::max(0, (max_gx - 1) * CELL_SIZE);
int end_x   = std::min((int)fb->width,  (max_gx + 2) * CELL_SIZE);

int total_x = 0, total_y = 0, final_count = 0;

for (int y = start_y; y < end_y; y++) {
    for (int x = start_x; x < end_x; x++) {
        // Decode and threshold pixel (same as Pass 1)
        if (/* pixel is blue */) {
            total_x += x;
            total_y += y;
            final_count++;
        }
    }
}

if (final_count > 40) {
    coords->x = total_x / final_count;
    coords->y = total_y / final_count;
    return true;
}
return false;
```

The centroid coordinates are the arithmetic means of the x and y pixel indices of all qualifying pixels within the search window. The second threshold of **40 pixels** ensures that the blob has sufficient spatial extent to be considered a real object rather than a cluster of noise pixels that happened to pass the Pass 1 threshold. This two-threshold approach was validated experimentally to reliably reject noise while maintaining detection sensitivity for the target blue object at distances of 0.3–1.0 m.

The rationale for focusing the centroid computation on only the 3×3 cell neighborhood — rather than all matching pixels in the frame — is to prevent distant blue objects or blue portions of the background from biasing the centroid estimate away from the primary target.

---

## 6.6 Servo Control Algorithm

The servo control algorithm runs in `servo_task` at a fixed rate of 50 Hz, determined by the `vTaskDelay(pdMS_TO_TICKS(20))` call at the end of each control cycle. The algorithm consists of four sequential stages: coordinate-to-angle mapping, deadband filtering, safety clamping, and exponential smoothing.

### 6.6.1 Geometric Pixel-to-Angle Mapping

The pixel-to-angle conversion factor is derived from the camera's field of view and the frame resolution:

**degrees_per_pixel = FOV / resolution = 40° / 240 ≈ 0.1667°/pixel**  *(1)*

This scalar is computed once at startup and stored as `degrees_per_pixel_x` and `degrees_per_pixel_y` (both equal in this square-frame configuration).

When a new coordinate is received from the queue, the pixel error relative to the image center is computed:

**err\_x = detected\_x − CAM\_MID\_X = detected\_x − 120**  *(2)*

**err\_y = detected\_y − CAM\_MID\_Y = detected\_y − 120**  *(3)*

The new target servo angle is then:

**target\_angle\_x = current\_angle\_x − err\_x × degrees\_per\_pixel\_x**  *(4)*

**target\_angle\_y = current\_angle\_y + err\_y × degrees\_per\_pixel\_y**  *(5)*

The sign convention reflects the physical orientation of the system: a target displaced to the right (positive err\_x) requires the pan servo to rotate left (decreasing angle) to center it. A target displaced downward (positive err\_y, since image Y increases downward) requires the tilt servo to increase its angle.

### 6.6.2 Deadband

Before computing the new target angle, a deadband of ±15 pixels is applied:

```cpp
if (err_x > 15 || err_x < -15)
    target_angle_x = current_angle_x - err_x * degrees_per_pixel_x;
// else: target_angle_x unchanged

if (err_y > 15 || err_y < -15)
    target_angle_y = current_angle_y + err_y * degrees_per_pixel_y;
// else: target_angle_y unchanged
```

When the detected target is within 15 pixels of the image center in either axis, the corresponding servo target angle is not updated. This prevents continuous micro-corrections in response to the inherent noise in the detection output — random frame-to-frame variations in the reported centroid position that do not reflect actual target motion. A 15-pixel deadband corresponds to a spatial region of ±2.5° (15 × 0.167°/px) around the center of the camera's field of view, within which the servo is considered sufficiently aligned and no correction is applied.

### 6.6.3 Safety Clamping

After computing the new target angles, the values are clamped to the physically safe operating ranges of the pan-tilt mechanism:

```cpp
target_angle_x = constrain(target_angle_x, 0.0f, 180.0f);
target_angle_y = constrain(target_angle_y, 60.0f, 120.0f);
```

This prevents the computed target from exceeding the mechanical limits of the arm, which could damage the servo gears or the 3D-printed structure.

### 6.6.4 Adaptive Exponential Smoothing

The final stage of the control algorithm is the exponential smoothing filter applied to both servo axes at every 50 Hz cycle, regardless of whether a new target coordinate was received:

**current\_angle = current\_angle × (1 − α) + target\_angle × α**  *(6)*

The smoothing factor α is selected based on the active tracking mode:

```cpp
float smoothing = (current_mode == MODE_FACE) ? 0.13f : 0.07f;
current_angle_x = current_angle_x * (1.0f - smoothing) + target_angle_x * smoothing;
current_angle_y = current_angle_y * (1.0f - smoothing) + target_angle_y * smoothing;
```

With α = 0.13 (face mode), the servo moves approximately 13% of the remaining distance to the target in each 20 ms cycle, reaching within 1% of the target in approximately 33 cycles (660 ms). With α = 0.07 (color mode), the convergence is slower but more stable, reaching within 1% of the target in approximately 64 cycles (1.28 s).

The lower α value in color mode reflects the higher noise level of the HSV blob centroid compared to the CNN bounding box centroid: color blob detection is more sensitive to background interference, lighting changes, and motion blur, all of which introduce larger frame-to-frame variations in the reported centroid. A lower smoothing factor effectively increases the cut-off frequency of the low-pass filter applied to the target position signal, rejecting more of this high-frequency noise at the cost of a slower tracking response.

### 6.6.5 PWM Output

The smoothed current angles are converted to LEDC duty cycle values using the `angle_to_duty()` function and written to the LEDC peripheral:

```cpp
static uint32_t angle_to_duty(int angle) {
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;
    uint32_t pulse_width = MIN_PULSE_WIDTH +
        (((MAX_PULSE_WIDTH - MIN_PULSE_WIDTH) * angle) / 180);
    return (pulse_width * ((1 << 13) - 1)) / 20000;
}

ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_X, angle_to_duty((int)current_angle_x));
ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_X);
ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_Y, angle_to_duty((int)current_angle_y));
ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_Y);
```

The `ledc_update_duty()` call is required to latch the newly written duty value into the LEDC shadow register and apply it at the start of the next PWM period. Without this call, the duty cycle update would not take effect.

---

## 6.7 Control Flow Summary

Figure 6.1 illustrates the complete data flow through the software system from camera capture to servo output.

> **[TODO: Figure 6.1 — Data flow diagram. Show two parallel vertical lanes (Core 0 and Core 1). Core 1: Camera capture → RGB565 frame → (MODE_FACE: HumanFaceDetect.run() → bounding box → centroid) or (MODE_COLOR: find_color_blob() → grid → centroid) → xQueueSend(). Core 0: 50Hz tick → xQueueReceive() → (if new: err_x/y → deadband → clamp → new target) → LERP smoothing → angle_to_duty() → ledc_set_duty(). Connect via Queue arrow between cores. Also show ISR on GPIO1 modifying current_mode.]**

---

## References for Chapter 6

[1] Espressif Systems. *ESP-IDF FreeRTOS Documentation*. Available: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html

[2] Espressif Systems. *LEDC PWM Controller*. Available: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/ledc.html

[3] Espressif Systems. *ESP32 Camera Driver*. Available: https://github.com/espressif/esp32-camera

[4] Espressif Systems. *ESP-WHO: Human Face Detection*. Available: https://github.com/espressif/esp-who
