# Chapter 8: Conclusions and Future Work

---

## 8.1 Summary of Work

This thesis presented the design, implementation, and testing of a self-contained, battery-powered real-time object tracking system built on a microcontroller-class platform. The system uses the Seeed XIAO ESP32S3 Sense as its central processing unit, an OV2640 camera module for image acquisition, two MG90S servo motors for physical camera actuation, and a 3D-printed pan-tilt arm as the mechanical platform. Power is supplied by two Li-Ion 18650 cells in a 2S configuration, protected by a BMS and regulated to 5 V by a buck converter.

The system implements two distinct tracking modes, selectable at runtime by the user through a hardware push button:

- **Face tracking mode**, which uses the ESP-DL `HumanFaceDetect` convolutional neural network model to detect human faces in real time at 12.5 FPS, entirely on the microcontroller without cloud or external computation.
- **Color blob tracking mode**, which uses a custom two-pass grid-based HSV color segmentation algorithm to detect and track colored objects, operating at a higher frame rate due to its lower computational complexity compared to the CNN.

The software architecture is built on FreeRTOS running across both cores of the ESP32S3's dual-core processor. The vision processing task (`vision_task`) runs on Core 1 and executes the detection pipeline asynchronously at the maximum rate supported by the camera and the inference engine. The servo control task (`servo_task`) runs on Core 0 and updates the PWM output at a fixed 50 Hz rate, decoupled from the vision task through a depth-1 FreeRTOS queue. This architecture ensures smooth and continuous servo motion independent of the detection update rate.

The servo control algorithm combines three mechanisms to achieve stable, oscillation-free tracking: a geometric pixel-to-angle mapping derived from the camera's field of view, a ±15 pixel deadband that prevents micro-corrections in response to detection noise, and an adaptive exponential smoothing filter with mode-dependent smoothing factors (α = 0.13 for face tracking, α = 0.07 for color tracking). Mode switching is handled by a hardware GPIO interrupt with 200 ms software debouncing.

---

## 8.2 Contributions

The primary technical contributions of this thesis are:

1. **Dual-core FreeRTOS architecture for vision-servo decoupling.** The assignment of the vision pipeline to Core 1 and the servo control loop to Core 0, with asynchronous communication through a single-depth queue, is the architectural decision that enables real-time servo control at 50 Hz while simultaneously running a CNN inference engine at 12.5 FPS. This architecture eliminates the frame-rate bottleneck that would result from a single-threaded implementation in which the servo waits for each detection cycle to complete before updating its output.

2. **Two-pass grid-based HSV color blob detection.** The custom detection algorithm divides the frame into a 15×15 grid, identifies the cell with the highest density of target-color pixels in the first pass, and computes a precise centroid from the 3×3 neighborhood of that cell in the second pass. This approach is significantly more noise-resistant than a naive full-frame scan and executes efficiently on the ESP32S3 without dedicated hardware acceleration.

3. **Adaptive exponential smoothing servo control.** The use of an exponential low-pass filter applied at 50 Hz as the sole control mechanism — replacing a conventional PID controller — is well-suited to the low-update-rate visual feedback available from a 12.5 FPS detector. The adaptive selection of the smoothing factor based on tracking mode compensates for the different noise characteristics of the CNN and HSV detection outputs.

4. **Complete, self-contained battery-powered embedded tracking platform.** The integration of all system components — power management, mechanical actuation, camera sensing, neural network inference, and custom computer vision — into a single compact, battery-powered unit demonstrates the feasibility of edge AI tracking applications without reliance on external infrastructure.

---

## 8.3 Future Work

Several directions for improvement and extension of this system are identified below.

### 8.3.1 Target Motion Prediction

The current system has no mechanism to compensate for the 80 ms latency introduced by the face detection inference cycle. When the target moves significantly between detection frames, the servo lags behind by the detection cycle time. Implementing a constant-velocity predictor or a Kalman filter [1] that estimates the target's position between frames — based on the observed velocity computed from consecutive detection centroids — would reduce effective tracking latency and improve performance for faster-moving targets.

### 8.3.2 Adaptive HSV Threshold Calibration

The HSV color thresholds for the color blob detection mode are currently hardcoded in the firmware. An on-device calibration routine — for example, triggered by a long button press — could capture a frame in which the user holds the target object in front of the camera, compute the HSV histogram of a central region of the frame, and automatically set the detection thresholds to match the observed color distribution. This would make the color tracking mode robust to different target colors and lighting environments without requiring firmware modification.

### 8.3.3 Higher Frame Rate Face Detection

The 12.5 FPS face detection rate is limited by the computational complexity of the `HumanFaceDetect` model at 240×240 resolution. Two approaches could improve this:

- **Reduced resolution inference:** Running the face detector on a downscaled 160×160 or 120×120 frame would reduce the number of operations per inference cycle, potentially doubling the frame rate at the cost of reduced detection accuracy for small or distant faces.
- **Lighter model architecture:** Espressif's ESP-DL framework supports multiple model configurations with different accuracy-speed trade-offs. A lighter model quantized to INT8 with fewer convolutional layers could achieve higher throughput while retaining acceptable face detection accuracy.

### 8.3.4 Closed-Loop Servo Position Feedback

The current implementation uses the servo motors' internal closed-loop controllers and assumes they faithfully execute the commanded angle. Adding external position feedback — for example, using magnetic rotary encoders mounted on the servo output shafts — would allow the ESP32S3 to verify the actual servo position and compensate for mechanical errors, load-induced position drift, or gear backlash. This would improve absolute pointing accuracy and allow detection of mechanical faults.

### 8.3.5 Web Streaming with Live Annotation

The ESP32S3's integrated Wi-Fi radio could be used to stream the camera feed to a web browser, with the detected bounding box or blob centroid drawn as an overlay on each frame. This would allow remote monitoring of the system's detection output without requiring a physical serial connection. A basic implementation of this was partially explored in the `camera_webserver.cpp` file included in the project repository and could be fully integrated with the tracking system.

### 8.3.6 Expanded Mechanical Range

The current Y-axis (tilt) range is limited to ±30° by the geometry of the 3D-printed arm. Redesigning the arm to provide greater clearance between the camera bracket and the structural elements would extend the vertical tracking range, improving coverage for targets that move significantly in the vertical direction. A continuous rotation pan axis (replacing the 0°–180° servo with a 360° servo or a stepper motor) would also eliminate the current limitation on horizontal range.

---

## 8.4 Final Remarks

The results obtained in this project demonstrate that modern microcontrollers — when selected with attention to their AI acceleration capabilities and peripheral integration, and when programmed with an appropriate real-time software architecture — are capable of performing tasks that until recently required significantly more powerful and power-hungry hardware.

The ESP32S3, with its dual-core Xtensa LX7 processor, PIE vector extensions, and integrated camera interface, represents a class of device at which the boundary between traditional embedded systems and edge AI platforms is converging. The combination of the ESP-IDF framework, FreeRTOS, and the ESP-DL library provides a complete and accessible software stack for developing embedded vision applications on this platform.

The system presented in this thesis is fully functional, battery-powered, and operates without any external infrastructure. It is a compact demonstration that meaningful artificial intelligence workloads — specifically, real-time human face detection using a convolutional neural network — can be executed on a device that fits in the palm of a hand and runs for hours on two standard battery cells.

---

## References for Chapter 8

[1] R. E. Kalman, "A new approach to linear filtering and prediction problems," *Journal of Basic Engineering*, vol. 82, no. 1, pp. 35–45, 1960.

[2] Espressif Systems. *ESP-DL Model Zoo*. Available: https://github.com/espressif/esp-dl

[3] Espressif Systems. *ESP32-S3 Wi-Fi and Camera Web Server Example*. Available: https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server
