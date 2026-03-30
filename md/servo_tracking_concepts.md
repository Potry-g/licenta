# ESP32S3 Servo Tracking Control Algorithms

When running AI workloads like ESP-DL Face Detection on a microcontroller like the ESP32S3, the framerate is inherently low (~10-15 FPS or updates every 80-100ms). This creates challenges for standard servo control. Here are the core concepts and algorithms for achieving smooth tracking.

## 1. Why PID is Not Suitable
Standard hobby RC servos (like MG90S) already contain an internal microcontroller running its own PID position-control loop. 
- When you send a PWM signal, you are commanding an **absolute position**, not motor voltage or speed. 
- Running an external PID loop on your ESP32 to command position "fights" the servo's internal loop (Cascaded Control Conflict). 
- Attempting to smoothly interpolate target angles between 12.5 FPS frames causes the servo to sluggishly track "old" data.

## 2. Direct Geometric Mapping (The Baseline)
Because the camera physically moves *with* the servo, you map the pixel offset directly to a real-world angle offset.

**How it works:**
1. Determine the camera's horizontal Field of View (FOV). Example: 60 degrees.
2. Determine pixels per degree: `60 degrees / 240 pixels = 0.25 degrees/pixel`.
3. Stop looping blindly. Block the servo task until a new face coordinate arrives.
4. Calculate the error: `pixels_off_center = center_x - face_x`.
5. Convert to an angle difference: `degree_offset = pixels_off_center * 0.25`.
6. Instantly add `degree_offset` to the current servo angle and command the servo to jump there immediately.

## 3. Frame Prediction (Constant-Velocity Model)
To compensate for the 80ms latency between frames, you can anticipate where the face will be *between* AI inferences.

**How it works:**
1. Store the previous frame's face location and the timestamps for both frames.
2. Calculate the velocity in the image plane: 
   `velocity (pixels/ms) = (current_x - previous_x) / time_delta_ms`
3. Between camera frames, let the servo task update every 20ms and guess the new position:
   `predicted_x = current_x + (velocity * elapsed_ms_since_last_frame)`
4. Command the servo to track the continuously updating `predicted_x` instead, allowing the servo to "coast" smoothly along the predicted path.

**The Danger of Prediction: Noise**
- Because pixel bounding boxes jitter slightly even when stationary, calculating velocity on raw pixels is extremely noisy.
- This noise gets amplified over time, causing the servo to aggressively snap sideways and jitter.
- **Solution:** A low-pass filter (like Exponential Moving Average) or a threshold must be applied to the calculated velocity before using it to predict movement.

## 4. Deadbands
To prevent endless micro-jittering when a face is perfectly tracked in the center:
- Define a "safe zone" in the center of the camera (e.g., +/- 15 pixels). 
- If the absolute pixel error is less than 15, command the servo to hold its current position. Only move when the face leaves the deadband.

## 5. ESP32S3 Vision Performance Estimates
Different tracking use-cases dictate different algorithms due to computational overhead. Below are approximate FPS limits for the ESP32S3 running at QVGA (320x240) using PSRAM:

| Detection Type | Approximate FPS | Processing Method | Why it performs this way |
| :--- | :---: | :--- | :--- |
| **Color/Blob Tracking** | **25 - 40+ FPS** | Traditional CV (Thresholding) | The CPU just loops through pixels in memory checking RGB ranges. Extremely fast, mostly limited by DMA bus speeds from the camera. |
| **Motion Detection** | **20 - 30+ FPS** | Traditional CV (Pixel Diff) | Compares current frame against a stored background frame. Fast array subtraction. |
| **QR Code / Barcode** | **10 - 20 FPS** | Traditional CV (Pattern Matching) | Requires finding contrast patterns (e.g., QR corners) and decoding matrices. |
| **Face Detect (ESP-DL)** | **10 - 15 FPS** | AI (CNN / Deep Learning) | Math-heavy vector instructions, but Espressif has heavily optimized this specific model for this architecture. |
| **AprilTag Tracking**| **5 - 10 FPS** | Traditional CV (Complex Math) | Requires heavy mathematical rotations to find the 3D pose, angles, and skew of fiducial markers. |
| **Object Classifiers** | **2 - 5 FPS** | AI (CNN / Deep Learning) | E.g. MobileNet (classifying entirely distinct objects). Running the whole image through an unoptimized classification network is extremely slow. |

If you switch from face detection to low-latency color/blob tracking, traditional Proportional (P) control loops *will* start to work well because the update latency drops well below the mechanical response time of the servo.
