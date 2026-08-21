# Chapter 7: Testing and Results

---

## 7.1 Test Setup and Methodology

### 7.1.1 Hardware Configuration

All tests were performed on the fully assembled system described in Chapter 5. The system was powered from the 2S Li-Ion battery pack through the BMS and buck converter, operating at a regulated 5 V supply. The buck converter output voltage was verified with a multimeter to be within ±0.1 V of the nominal 5 V before each test session.

The pan-tilt arm was mounted on a stable flat surface at desk height. The camera axis was set to the horizontal center position (both servos at 90°) at the start of each test. No external lighting fixtures were used beyond standard indoor ambient illumination from overhead fluorescent/LED lighting.

> **[TODO: Figure 7.1 — Photo of the complete test setup: the assembled arm on a desk, battery pack connected, monitor or laptop in the background showing the ESP-IDF serial monitor output.]**

### 7.1.2 Measurement Instruments and Methods

**Frame rate measurement:** The ESP-IDF logging system (`ESP_LOGI`) was used to output timestamps at the start and end of each detection cycle. The face detection frame rate was derived by measuring the time between consecutive log entries from `vision_task` using `esp_timer_get_time()`. A series of 100 consecutive frames was timed and averaged.

**Latency measurement:** System latency — defined as the time elapsed between the moment a target displacement occurs and the moment the servo begins moving toward the new target — was estimated as the sum of the detection cycle time (1/FPS) and the servo response delay (one 20 ms control cycle). Direct measurement of end-to-end latency would require external high-speed camera capture synchronized with the servo, which was beyond the scope of this test setup.

**Memory measurement:** Free heap size was recorded using `esp_get_free_heap_size()` and `esp_get_free_internal_heap_size()` logged at system startup and during steady-state operation.

**Servo stability assessment:** Servo oscillation was assessed qualitatively by observing the physical motion of the arm during tracking. A stable system shows smooth, continuous motion without visible back-and-forth micro-vibration when the target is stationary or near the center of the frame.

---

## 7.2 Performance Metrics

Table 7.1 summarizes the quantitative performance metrics of the system as measured or determined from the implementation.

**Table 7.1: System performance metrics**

| Metric | Value | Method |
|--------|-------|--------|
| Face detection frame rate | **12.5 FPS** | Measured — serial log timestamps |
| Face detection cycle time | **80 ms** | Derived (1 / 12.5 FPS) |
| Color blob detection frame rate | **[TODO]** | To be measured |
| Servo control rate | **50 Hz** (20 ms period) | Fixed — LEDC timer configuration |
| Detection-to-servo-update latency | **~80–100 ms** | Estimated |
| Camera resolution | **240 × 240 px** | Configuration |
| Frame buffer size (PSRAM) | **115,200 bytes** (~113 KB) | Calculated (240 × 240 × 2) |
| Deadband radius | **±15 px** (~±2.5°) | Configuration |
| Smoothing factor α (face mode) | **0.13** | Configuration |
| Smoothing factor α (color mode) | **0.07** | Configuration |
| Y-axis operational range | **[60°, 120°]** | Mechanical constraint |
| X-axis operational range | **[0°, 180°]** | Full servo range |
| Free heap at runtime | **[TODO]** | To be measured |
| Battery runtime | **[TODO]** | To be measured |
| Mode switch latency (ISR) | **< 1 ms** | Inherent to ISR execution |

---

## 7.3 Face Tracking Results

### 7.3.1 Detection Performance

The face detection model (`HumanFaceDetect`) running on the ESP32S3's Core 1 achieved a consistent inference rate of **12.5 FPS** across all test conditions. This rate was stable regardless of whether a face was present in the frame or not, as the detector processes every captured frame regardless of the detection outcome.

The 12.5 FPS rate corresponds to a detection cycle time of 80 ms. This means the servo control algorithm operates on stale information for up to 80 ms between target updates. The adaptive exponential smoothing with α = 0.13 effectively bridges these update intervals: the servo continues to move smoothly toward the last known target position during the 80 ms gap, rather than abruptly jumping to the new position when the next detection arrives.

> **[TODO: Include here a screenshot or copy of the ESP-IDF serial monitor output showing several consecutive detection log entries with timestamps, demonstrating the consistent 80ms cycle time.]**

### 7.3.2 Tracking Stability — Stationary Target

When tracking a stationary face positioned within the camera's field of view, the system converges to the correct pointing angle within approximately 1–2 seconds from the initial detection. After convergence, the deadband of ±15 pixels prevents the servo from making further corrections as long as the detected centroid remains within this region, resulting in a fully stationary servo despite frame-to-frame centroid noise in the detection output.

Without the deadband, the servo exhibits continuous low-amplitude oscillation around the correct position — visible as a slight shaking of the camera arm — as it responds to pixel-level noise in the face detection bounding box coordinates. The effectiveness of the deadband in eliminating this behavior was verified by temporarily commenting out the deadband conditions and observing the resulting oscillation, then restoring them.

> **[TODO: Figure 7.2 — If possible, include a graph of servo angle over time for a stationary target: with deadband (flat line after convergence) vs. without deadband (continuous oscillation). Data can be collected by logging `current_angle_x` to the serial monitor and plotting in a spreadsheet.]**

### 7.3.3 Tracking Performance — Moving Target

When the tracked face moves laterally within the camera's field of view, the servo responds within one detection cycle (80 ms) by updating the target angle. The smoothing filter with α = 0.13 produces a gradual approach to the new target angle over approximately 33 control cycles (660 ms), sufficient for natural human movement at typical video conferencing distances.

At distances beyond approximately 1.5 m, the face occupies a smaller angular portion of the frame, and small lateral movements of the subject produce proportionally smaller pixel displacements in the detected centroid. The tracking response is consequently more subtle at greater distances, which is physically appropriate.

### 7.3.4 Behavior on Target Loss

When the tracked face leaves the camera's field of view — either by moving out of the frame or by occlusion — the face detector returns an empty list. No coordinate is sent to the servo queue, `got_new_coords` remains false, and the servo target angle is not updated. The servo continues executing the smoothing filter toward the last known target angle, eventually settling at that position.

This behavior is appropriate for the intended use case: when the target temporarily leaves the frame, the camera remains pointed at the last known position, increasing the probability that the target will re-enter the frame without requiring a full re-acquisition sweep.

---

## 7.4 Color Blob Tracking Results

### 7.4.1 Detection Performance

> **[TODO: Measure and insert: color blob detection frame rate (FPS), obtained by the same timestamp-logging method used for face detection. Expected to be significantly higher than 12.5 FPS due to the lower computational complexity of the HSV algorithm compared to the CNN.]**

### 7.4.2 Noise Rejection — Grid-Based Algorithm

The grid-based two-pass algorithm demonstrated effective rejection of background noise in all test conditions. The primary noise sources encountered were:

- **Specular highlights:** Small bright regions reflecting ambient light, which can exhibit high saturation and a blue-shifted hue when the light source has a cool color temperature. These produce isolated pixel clusters that are reliably rejected by the Pass 1 minimum threshold of 25 pixels per grid cell.
- **Blue clothing or background objects:** Objects with colors within the detection hue range but at a distance or of small angular size. The Pass 2 minimum threshold of 40 pixels in the 3×3 neighborhood rejects objects that subtend less than approximately 2–3° of the camera's field of view.

Compared to a naive full-frame scan that reports the centroid of all matching pixels in the image, the grid-based algorithm's use of the densest cell as the anchor for the centroid computation reliably selects the primary (largest or closest) blue object when multiple such objects are present in the scene.

### 7.4.3 Tracking Stability — Stationary Target

The behavior of the color tracking mode with a stationary target mirrors that of face tracking: the servo converges to the correct position and the deadband prevents oscillation. The lower smoothing factor (α = 0.07 vs. 0.13) produces a slower but more stable approach, which is appropriate given the higher frame-to-frame noise in the blob centroid compared to the face detection centroid. The reduced α effectively applies stronger low-pass filtering to the noisy centroid signal.

### 7.4.4 Sensitivity to Lighting Conditions

The HSV thresholds for blue detection were calibrated under standard indoor fluorescent lighting. The system was observed to remain functional under moderate variation in ambient illumination intensity, as the HSV Hue channel is relatively invariant to uniform changes in brightness. However, under strongly colored ambient light (e.g., incandescent warm-white lighting with a color temperature below approximately 3000 K), the effective hue of the tracked object shifts due to the color cast, potentially moving it outside the configured detection range. This is a known limitation of fixed-threshold HSV detection and is discussed further in the limitations section.

---

## 7.5 Mode Switching Behavior

Mode switching between face tracking and color blob tracking is triggered by a button press on GPIO 1. The ISR executes within microseconds of the falling edge of the GPIO signal, atomically updating the `current_mode` variable. The `vision_task` checks this variable at the beginning of each frame processing cycle, so the effective mode switch latency is at most one detection cycle (80 ms for face mode, shorter for color mode).

The servo state — both `current_angle` and `target_angle` — is not reset on mode switch. This means the servo holds its current physical position when the mode changes and begins tracking the new mode's target from that position. This behavior eliminates any abrupt servo jump at the moment of mode switch, providing a smooth transition.

> **[TODO: Figure 7.3 — Serial monitor screenshot showing mode switch event: the log line "Switched to COLOR tracking mode" (or FACE) appearing, followed by color blob detection log entries. Demonstrate that the transition is immediate and logging changes in the next frame cycle.]**

---

## 7.6 Memory Utilization

Table 7.2 presents the memory utilization of the system during steady-state operation.

**Table 7.2: Memory utilization during operation**

| Memory Region | Total | Used | Free |
|---------------|-------|------|------|
| Internal SRAM | 512 KB | [TODO] | [TODO] |
| PSRAM (external) | 8 MB | ~113 KB (frame buffer) | [TODO] |
| Flash (code + model) | 8 MB | [TODO] | [TODO] |

> **[TODO: Fill this table using `esp_get_free_heap_size()` and `esp_get_free_internal_heap_size()` logged at startup (before tasks run) and during steady-state operation. Also record flash usage from the build output: `idf.py size` or `idf.py size-components`.]**

The frame buffer accounts for the dominant PSRAM allocation: 115,200 bytes for one frame at 240×240 RGB565, multiplied by `fb_count = 2` for double buffering, totaling approximately 230,400 bytes (~225 KB) reserved in PSRAM for camera frame buffers. The remaining PSRAM is available for heap allocation by the application and the ESP-DL model runtime.

---

## 7.7 Discussion and Limitations

### 7.7.1 Detection Latency

The 80 ms detection cycle of the face detection model represents the primary latency in the tracking loop. For slow or predictable target motion — such as a person seated in front of the camera — this latency is imperceptible because the target moves less than one camera field of view width during the detection interval. For fast or unpredictable motion — such as a quickly turning head — the system will briefly lose tracking alignment before recovering after 1–2 detection cycles.

This limitation could be addressed by implementing a target motion prediction model (such as a constant-velocity predictor or a Kalman filter) that extrapolates the target position during the inter-frame interval based on the observed velocity of the target. This is identified as a direction for future work.

### 7.7.2 Color Threshold Sensitivity

The HSV thresholds for blue detection are fixed constants in the firmware. They were calibrated for a specific shade of blue under standard indoor illumination. A different shade of blue, a different lighting environment, or a change in camera sensor settings (gain, white balance) can push the target's HSV values outside the configured range, causing detection failure. Implementing an adaptive threshold calibration — for example, by analyzing the HSV histogram of a user-designated region of interest — would significantly improve robustness.

### 7.7.3 Y-Axis Range Constraint

The tilt axis is mechanically limited to [60°, 120°], providing only ±30° of vertical tracking range. Targets that move below or above this range cannot be followed. For the intended use case (a stationary person at a workstation), this range is generally sufficient. The range could be extended by redesigning the arm geometry to provide greater clearance for the camera bracket.

### 7.7.4 Open-Loop Servo Control

The MG90S servo motors implement an internal closed-loop position controller, but the ESP32S3 has no direct access to the measured shaft position — the servo feedback potentiometer is internal to the servo and not connected to any external pin. The system therefore has no way to verify that the servo actually reached the commanded angle, or to detect mechanical binding or excessive load. The system assumes that the servo faithfully follows the commanded PWM signal, which is valid under normal operating conditions but may not hold if the mechanism encounters an obstruction.

---

## References for Chapter 7

[1] Espressif Systems. *ESP-IDF System API — Heap Memory*. Available: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html

[2] Espressif Systems. *idf.py size command*. Available: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/tools/idf-py.html
