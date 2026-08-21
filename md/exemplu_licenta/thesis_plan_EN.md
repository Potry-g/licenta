# Bachelor's Thesis Plan
## "Embedded Real-Time Object Tracking System Using Edge AI"

> **Note:** Standard Romanian university thesis format.

---

## Overall Structure

| # | Chapter | Est. Pages |
|---|---------|-----------|
| — | Title page, authenticity declaration | 2 |
| — | Project synthesis (RO + EN) — "Sinteza Proiectului" | 2 |
| — | Table of Contents, List of Figures, List of Tables | 3–4 |
| 1 | Summary in Romanian | 6–8 |
| 2 | Work Planning (Gantt diagram) | 1–2 |
| 3 | State of the Art | 8–12 |
| 4 | Theoretical Fundamentals | 14–20 |
| 5 | System Design | 12–16 |
| 6 | Software Implementation | 10–14 |
| 7 | Testing and Results | 8–12 |
| 8 | Conclusions and Future Work | 3–5 |
| — | References | 2–3 |
| — | Appendices | optional |
| **TOTAL** | | **~70–90 pages** |

---

## Title Page
- University name, Faculty name
- "LUCRARE DE LICENȚĂ" / "BACHELOR'S THESIS"
- **Title:** "Embedded Real-Time Object Tracking System Using Edge AI"
- Your name, supervisor name and title (e.g., Conf. dr. ing. ...)
- City, Year

---

## Authenticity Declaration (Declarație de Autenticitate)
Standard university form — signed, dated.

---

## Project Synthesis — "Sinteza Proiectului de Diplomă" (2 pages)

**This is a 2-page bilingual summary** — Romanian first, then English.
Must include:
- What the system does (1 paragraph)
- Key components used (camera, microcontroller, actuators, power system)
- Software approach
- Results summary (does it work? performance?)
- Signed by supervisor ("Avizul conducătorului")

**Draft EN version:**
> This thesis presents the design and implementation of a self-contained, battery-powered 2-axis object tracking system based on the Seeed XIAO ESP32S3 Sense microcontroller. The system uses an OV2640 camera module to capture real-time video frames and processes them using two distinct computer vision algorithms: a CNN-based face detection model provided by the ESP-DL framework, and a custom HSV color blob detection algorithm. The tracking output drives two hobby servo motors mounted on a 3D-printed pan-tilt arm via the LEDC PWM peripheral. A FreeRTOS dual-core architecture separates the vision pipeline from the servo control loop, enabling smooth 50Hz servo updates independent of the ~12 FPS AI inference rate. The user can switch between tracking modes using a hardware push button connected via GPIO interrupt. Power is supplied by two Li-Ion 18650 cells in series with a 2S BMS and a 5V buck converter. The system successfully tracks human faces and colored objects in real-time with stable, oscillation-free servo motion.

---

## Chapter 1: Summary in Romanian — "Rezumat în Limba Română"

This chapter is entirely in Romanian and covers:

### 1.1 Descrierea temei (Topic Description)
- Why real-time tracking on embedded systems is relevant
- The gap: most tracking systems use cloud/PC — this is fully standalone
- The goal: build a complete, battery-powered tracking robot

### 1.2 Descrierea concisă a sistemului (System Description)
- Hardware components: ESP32S3, OV2640, 2 servos, 3D-printed arm, 2S Li-Ion + BMS + buck
- Two tracking modes: față (face) și culoare (color blob)
- Comutare prin buton fizic cu întrerupere hardware

### 1.3 Proiectarea sistemului (System Design)
- Block diagram (RO version)
- Power system: baterii → BMS → buck 5V → ESP + servouri
- Software: FreeRTOS dual-core, coadă IPC, LEDC PWM
- Algoritmii: CNN (ESP-DL) și HSV grid-based

### 1.4 Testarea sistemului (System Testing)
- FPS măsurat, latență, comportament la pierderea țintei
- Comparație față tracking vs. color tracking

### 1.5 Concluzii
- Sistemul funcționează și îndeplinește obiectivele
- Direcții de îmbunătățire

---

## Chapter 2: Work Planning

**One page with a Gantt diagram** — a visual timeline of the project phases.

Suggested phases:
| Phase | Description | Timeframe |
|-------|-------------|-----------|
| 1 | Literature review & component selection | Month 1 |
| 2 | Hardware assembly & wiring | Month 1–2 |
| 3 | Camera driver & ESP-IDF setup | Month 2 |
| 4 | Face detection integration (ESP-DL) | Month 2–3 |
| 5 | Servo control algorithm development | Month 3 |
| 6 | HSV color tracking implementation | Month 3–4 |
| 7 | System integration & tuning | Month 4 |
| 8 | Testing & documentation | Month 4–5 |

---

## Chapter 3: State of the Art

### 3.1 Introduction
- What is real-time object tracking?
- Applications: robotics, surveillance, assistive devices, drones, smart cameras
- Challenge: balancing compute requirements with power and cost constraints

### 3.2 Traditional Tracking Systems
- **PC/Cloud-based:** OpenCV + Python on a desktop. High accuracy, high power, not portable
- **Raspberry Pi based:** common in hobby robotics, runs Linux + OpenCV, but heavy (~5W), expensive
- **FPGA-based systems:** real-time but expensive and complex to program

### 3.3 Edge AI and TinyML
- Definition: running machine learning inference directly on low-power microcontrollers
- Trend: pushed by companies like Google (TensorFlow Lite Micro), ARM (Cortex-M55 + Ethos NPU), Espressif (ESP-DL)
- Key constraint: memory (KB, not GB), no OS, no floating point GPU

### 3.4 Existing Embedded Tracking Solutions
- **OpenMV:** dedicated camera microcontroller for machine vision, runs MicroPython — simpler but less powerful
- **Pixy2:** dedicated object tracking camera sensor, good for color blobs but limited AI
- **ESP32 + Arduino + color tracking:** common tutorial projects, but open-loop, no face AI
- **This project's position:** combines CNN face detection AND HSV color tracking on a single MCU with FreeRTOS, in a battery-powered self-contained unit

### 3.5 Servo Control in Robotics
- Standard PWM servo control (50Hz signal, 500–2500µs pulse)
- PID controllers: standard approach, limitations for low-FPS vision input
- Proportional control with smoothing: simpler and more appropriate for this use case

---

## Chapter 4: Theoretical Fundamentals

*Deep dive into every component used. Each section should include: what it is, how it works, why you chose it, key specifications from the datasheet.*

### 4.1 Power Management

#### 4.1.1 Lithium-Ion Batteries
- Li-Ion 18650 chemistry: nominal 3.6–3.7V, fully charged 4.2V, cutoff 2.5–3.0V
- Two cells in series (2S): 7.2–8.4V nominal/max
- Energy density advantages over NiMH, alkaline
- Safety requirements: must not overcharge, over-discharge, or short-circuit

#### 4.1.2 Battery Management System (BMS) — 2S Configuration
- Functions: cell balancing, over-charge protection (>4.2V/cell), over-discharge protection (<2.5V/cell), short-circuit protection
- Why a BMS is mandatory for Li-Ion (thermal runaway risk without protection)
- Include a simple schematic of BMS in the circuit

#### 4.1.3 Buck Converter (Step-Down DC-DC)
- Converts 7.4–8.4V battery voltage → regulated 5V
- Why not a linear regulator (LDO)? — efficiency: buck ~90% vs. LDO ~60% at these voltages → less heat, more battery life
- Switching frequency, output ripple, capacitor decoupling
- Servo current spikes (up to 600mA per servo stall) and why decoupling prevents ESP32 brownouts

### 4.2 The Microcontroller — Seeed XIAO ESP32S3 Sense

#### 4.2.1 ESP32S3 Architecture
- Xtensa LX7 dual-core, 240 MHz
- 512 KB internal SRAM + 8 MB external PSRAM
- PIE (Processor Instruction Extensions) — SIMD vector instructions used by ESP-DL for CNN acceleration
- Compare LX7 vs. LX6 (previous ESP32): PIE instructions = key advantage for AI workloads

#### 4.2.2 Functionality and Peripherals Used
- **LEDC (LED Control) peripheral:** hardware PWM generator used for servo control
  - 8 independent channels, configurable timer, up to 14-bit resolution
  - Two independent timers (TIMER_1, TIMER_2) for simultaneous X and Y servo control
- **GPIO with interrupt:** NEGEDGE interrupt on GPIO 1 for button
- **DVP Camera Interface:** 8-bit parallel data bus for OV2640

#### 4.2.3 Electrical Considerations
- Supply voltage: 3.3V (USB 5V regulated on-board)
- Maximum IO current per pin: 40 mA
- Total IO current limit: important when driving multiple peripherals

#### 4.2.4 Comparison with Alternative Microcontrollers
| Feature | XIAO ESP32S3 | Arduino Nano | STM32F4 | Raspberry Pi Zero |
|---------|-------------|-------------|---------|-----------------|
| CPU speed | 240 MHz | 16 MHz | 168 MHz | 1 GHz |
| AI acceleration | PIE (SIMD) | None | None | None |
| Camera interface | DVP built-in | No | No | Yes (via CSI) |
| PSRAM | 8 MB | None | None | 512 MB |
| Power consumption | ~240 mW | ~150 mW | ~200 mW | ~1.2 W |
| OS required | No (FreeRTOS) | No | No | Yes (Linux) |
| Cost | Low | Very low | Medium | Low |

### 4.3 Camera Module — OV2640

#### 4.3.1 DVP Interface
- Digital Video Port (DVP): 8 parallel data lines (Y2–Y9), VSYNC, HREF, PCLK, XCLK
- XCLK: master clock provided by ESP32 (10 MHz configured)
- SCCB (I2C-compatible) for camera register configuration (SIOD, SIOC)

#### 4.3.2 Pixel Formats and Resolution
- Pixel format used: **RGB565** — 16 bits per pixel, 5 bits R, 6 bits G, 5 bits B
  - Native format for ESP-DL → no conversion overhead
- Resolution: **240×240 pixels**
  - Frame size: 240 × 240 × 2 bytes = **115,200 bytes** (~113 KB) stored in PSRAM
  - Trade-off: larger resolution = more compute per frame = lower FPS

#### 4.3.3 Frame Buffer Configuration
- `CAMERA_FB_IN_PSRAM`: frame buffer allocated in external PSRAM (too large for internal SRAM)
- `CAMERA_GRAB_WHEN_EMPTY`: always grab a fresh frame — prevents processing stale images
- `fb_count = 2`: double buffering — one buffer for capture, one for processing

#### 4.3.4 Field of View
- OV2640 diagonal FOV ≈ 65°
- At 240×240 (1:1 aspect ratio): horizontal FOV ≈ vertical FOV ≈ **~40°**
- This FOV value is used directly in the tracking algorithm (Section 6.3)

### 4.4 Servo Motors

#### 4.4.1 Operating Principle
- Hobby servo: motor + gearbox + potentiometer + internal control circuit
- Controlled by PWM signal at **50 Hz** (period = 20 ms)
- Pulse width determines angle:
  - 500 µs → 0°
  - 1500 µs → 90° (center)
  - 2500 µs → 180°

#### 4.4.2 LEDC PWM Generation on ESP32S3
- 13-bit duty cycle resolution chosen (`LEDC_TIMER_13_BIT` → 8191 steps)
- Duty cycle formula:
  ```
  pulse_width_µs = 500 + (angle / 180) × 2000
  duty = (pulse_width_µs × 8191) / 20000
  ```
- Two servos on separate LEDC timers (TIMER_1 for Y, TIMER_2 for X) for fully independent control

#### 4.4.3 Physical Constraints
- Axis X (pan): full range [0°, 180°]
- Axis Y (tilt): limited to [60°, 120°] by the 3D-printed arm geometry to prevent collision

### 4.5 Software Framework — ESP-IDF and FreeRTOS

#### 4.5.1 ESP-IDF
- Espressif IoT Development Framework: the official SDK for ESP32
- Why ESP-IDF over Arduino: full FreeRTOS integration, direct hardware peripheral access, component manager, better performance
- Build system: CMake + `idf.py` toolchain

#### 4.5.2 FreeRTOS
- Real-Time Operating System preemptive scheduler
- Key concepts used: tasks, priorities, core affinity, queues
- `xTaskCreatePinnedToCore()`: pins a task to a specific CPU core
- Thread-safe inter-task communication via `xQueueCreate / xQueueSend / xQueueReceive`

### 4.6 Edge AI — ESP-DL Framework
- ESP-DL: Espressif's deep learning inference library optimized for ESP32S3
- Uses PIE vector instructions for accelerated convolution operations
- Model: `HumanFaceDetect` from the `esp-who` library
- Input: RGB565 image tensor; Output: list of bounding boxes with confidence scores
- Quantized to 8-bit integers for speed and memory efficiency

### 4.7 Computer Vision — HSV Color Space
- HSV (Hue, Saturation, Value) is more robust to lighting changes than RGB for color detection
- Hue: color identity (0–360°), Saturation: color purity, Value: brightness
- RGB565 → RGB → HSV conversion formula (include all 3 equations numbered)
- Why HSV over RGB for color thresholding: a blue object looks different under different lighting in RGB, but Hue stays consistent

---

## Chapter 5: System Design

### 5.1 System Architecture Overview
Full block diagram:
```
[2× Li-Ion 18650] → [2S BMS] → [5V Buck Converter]
                                        │
              ┌─────────────────────────┼──────────────────────┐
              │                         │                      │
       [Servo X — GPIO4]        [Servo Y — GPIO3]     [XIAO ESP32S3]
              │                         │                      │
              └──────────[3D Printed Pan-Tilt Arm]─────────────┘
                                        │
                                [OV2640 Camera]
                                [Push Button — GPIO1]
```

### 5.2 Mechanical Design — 3D Printed Pan-Tilt Arm
- Pan-tilt concept: Servo X rotates the entire arm (yaw/pan), Servo Y tilts the camera bracket (pitch/tilt)
- Camera and ESP32S3 board are mounted on the arm — they move together with the servos
- This means the tracking is achieved by **physically centering** the subject in the camera frame, not by digital cropping
- Include: photo of the physical prototype + labeled diagram
- Materials: PLA or PETG, M2/M3 screws, servo horns
- Note the mechanical angle limits that drive the software constraints on axis Y

### 5.3 Electrical Schematic
- Full connection table:

| Signal | ESP32S3 GPIO | Connected to |
|--------|-------------|-------------|
| Camera Y2–Y9 | 15,17,18,16,14,12,11,48 | OV2640 D0–D7 |
| VSYNC | 38 | OV2640 VSYNC |
| HREF | 47 | OV2640 HREF |
| PCLK | 13 | OV2640 PCLK |
| XCLK | 10 | OV2640 XCLK |
| SIOD | 40 | OV2640 SDA |
| SIOC | 39 | OV2640 SCL |
| Servo X PWM | 4 | Servo X signal |
| Servo Y PWM | 3 | Servo Y signal |
| Button | 1 | Push button (pull-up) |
| 5V | VIN | Buck converter output |
| GND | GND | Common ground |

- Power rail schematic: batteries in series → BMS → buck → 5V rail → servos + ESP32S3 VIN

### 5.4 3D Printed Enclosure / Arm
- Description of the arm geometry
- How the ESP32S3 and camera are mounted
- How the button is accessible to the user
- Cable management within the arm

---

## Chapter 6: Software Implementation

### 6.1 Project Structure
```
licenta/
├── main/
│   └── main.cpp          # All application logic
├── components/
│   ├── bsp/              # Board support package
│   └── esp-who/          # ESP-DL face detection model
├── CMakeLists.txt
├── sdkconfig.defaults
└── partitions.csv
```

### 6.2 FreeRTOS Task Architecture

#### 6.2.1 Dual-Core Task Assignment
- **`servo_task` → Core 0:** Runs at 50 Hz (every 20 ms). Reads target coordinates from queue, applies smoothing and deadband, updates LEDC PWM duty cycle
- **`vision_task` → Core 1:** Continuously captures camera frames, runs face detection (MODE_FACE) or HSV blob detection (MODE_COLOR), sends result to queue

#### 6.2.2 Inter-Task Communication
- FreeRTOS queue of depth 1, element type `face_coords_t {int x, int y}`
- Non-blocking send (timeout = `0` / `portDONT_BLOCK`): if queue full, the send is discarded — always the freshest coordinate wins
- Queue draining pattern in `servo_task`: receive in a loop until empty, keeping only the last received coordinate

#### 6.2.3 Mode Switching via Hardware Interrupt
- ISR registered with `gpio_isr_handler_add()`, attribute `IRAM_ATTR`
- `IRAM_ATTR`: forces ISR code into internal RAM. If ISR were in Flash, a cache miss during Flash access could stall the ISR and crash the system
- Debounce: compare current timestamp (`esp_timer_get_time() / 1000`) against `last_isr_time`; require >200 ms between triggers
- `volatile TrackingMode current_mode`: `volatile` prevents compiler from caching the value in a register — ensures ISR write is visible to tasks

### 6.3 Face Detection Implementation
- Initialize `HumanFaceDetect detector` object
- Build `dl::image::img_t` descriptor pointing to frame buffer: width, height, `DL_IMAGE_PIX_TYPE_RGB565`
- Call `detector.run(img)` → returns `std::list<dl::detect::result_t>`
- Extract centroid: `cx = (box[0] + box[2]) / 2`, `cy = (box[1] + box[3]) / 2`
- Send centroid to servo queue

### 6.4 HSV Color Blob Detection

#### RGB565 Pixel Decoding
```cpp
uint16_t pixel = ((uint16_t)buf[i] << 8) | buf[i+1];   // big-endian
uint8_t R = (pixel >> 8) & 0xF8;
uint8_t G = (pixel >> 3) & 0xFC;
uint8_t B = (pixel << 3) & 0xF8;
```

#### RGB to HSV Conversion
Full standard formula (include equations numbered (1)(2)(3)).

#### Two-Pass Grid Algorithm
**Pass 1 — 15×15 grid, 16×16 px cells:**
Count pixels matching blue HSV threshold (`H ∈ (200°, 255°)`, `S > 0.4`, `V > 0.2`) per cell.
Find cell with maximum count. Reject if count < 25.

**Pass 2 — Precise centroid:**
Search 3×3 neighborhood of the max cell. Compute centroid. Reject if total < 40 pixels.

### 6.5 Servo Control Algorithm

#### Geometric Pixel-to-Angle Mapping
```
err_x = detected_x - CAM_MID_X
err_y = detected_y - CAM_MID_Y
degrees_per_pixel = FOV / resolution = 40° / 240 ≈ 0.167°/px
target_angle_x = current_angle_x - err_x × dpp_x
target_angle_y = current_angle_y + err_y × dpp_y
```
Include as numbered equations.

#### Deadband
```
if (|err_x| > 15): update target_angle_x
else: keep target_angle_x unchanged
```
Prevents micro-oscillation when subject is near-centered.

#### Safety Clipping
```
target_x = clamp(target_x, 0°, 180°)
target_y = clamp(target_y, 60°, 120°)
```

#### Adaptive Exponential Smoothing
```
α = 0.13  (MODE_FACE)
α = 0.07  (MODE_COLOR)
current = current × (1 − α) + target × α
```
Applied at 50 Hz. Decouples servo update rate from detection rate (~12 FPS).

---

## Chapter 7: Testing and Results

### 7.1 Test Setup
- Environment: indoor, controlled lighting
- Face tracking subject: person seated at 0.5–1.5 m distance
- Color tracking subject: solid blue object at 0.3–1.0 m
- Measurement tools: serial monitor (ESP-IDF log output), stopwatch for mode switch timing

### 7.2 Performance Measurements

| Metric | Measured Value |
|--------|---------------|
| Face detection rate | ~12 FPS |
| Color blob detection rate | ~25 FPS |
| Servo control rate | 50 Hz (fixed) |
| Detection-to-movement latency | ~80–100 ms |
| Frame buffer size | 115,200 bytes (113 KB) |
| Mode switch response time | <1 ms (ISR) |
| Battery runtime estimate | ~X hours (measure and fill) |
| Free heap (runtime) | measure with `esp_get_free_heap_size()` |

### 7.3 Face Tracking Results
- Tracking behavior under normal indoor lighting (describe qualitatively)
- Behavior when face leaves frame (servo holds last position)
- Behavior with multiple faces (first detected used)
- Comparison: with smoothing α=0.13 vs. without smoothing (show oscillation difference)
- Comparison: with deadband vs. without deadband

### 7.4 Color Tracking Results
- Effective range and accuracy for blue object
- Behavior with similar-colored background elements
- Comparison: grid-based (final) vs. naive pixel scan (show noise reduction)
- Sensitivity to lighting changes

### 7.5 Mode Switching Behavior
- Latency of mode switch after button press
- Servo behavior at transition point (no sudden jump due to smoothing)

### 7.6 Discussion and Limitations
- HSV thresholds are hardcoded → sensitive to lighting conditions
- Face detection limited to ~12 FPS → up to 80 ms tracking lag during fast motion
- Y-axis mechanically limited to ±30° from center
- Open-loop servo control (no position feedback encoder)

---

## Chapter 8: Conclusions and Future Work

### 8.1 Summary of Contributions
- Successfully designed and built a fully self-contained battery-powered tracking robot
- FreeRTOS dual-core architecture effectively separates vision and control concerns
- Two functional tracking modes: CNN face detection (ESP-DL) and custom HSV grid blob detection
- Stable, oscillation-free control using adaptive smoothing and deadband without classical PID
- Hardware interrupt-driven mode switching with software debounce

### 8.2 Future Work
- **Adaptive HSV calibration:** dynamically tune color thresholds via web interface or on-device calibration routine
- **Faster AI models:** quantized INT8 face models for higher FPS
- **Kalman filter prediction:** predict target position between frames to compensate AI latency
- **Closed-loop servo control:** add encoders for true position feedback
- **Better camera:** OV5640 for higher resolution and more accurate FOV
- **IoT integration:** Wi-Fi streaming with live bounding box overlay via ESP32 web server

---

## References (examples — add your own)

[1] Espressif Systems. *ESP-IDF Programming Guide*. Available: https://docs.espressif.com/projects/esp-idf

[2] Espressif Systems. *ESP-DL: Espressif Deep Learning Library*. Available: https://github.com/espressif/esp-dl

[3] Espressif Systems. *ESP-WHO: Face Detection and Recognition*. Available: https://github.com/espressif/esp-who

[4] Barry, R. *FreeRTOS Reference Manual*. Available: https://www.freertos.org/Documentation/RTOS_book.html

[5] Seeed Studio. *XIAO ESP32S3 Sense Getting Started*. Available: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

[6] Warden, P. & Situnayake, D. (2019). *TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers*. O'Reilly Media.

[7] OmniVision Technologies. *OV2640 Camera Module Datasheet*.

[8] Espressif Systems. *ESP32-S3 Technical Reference Manual*.

[9] *(Add 2–3 academic papers on embedded tracking / TinyML from Google Scholar)*

---

## Appendices

### Appendix A — Full Source Code (`main.cpp`)
Complete annotated source code.

### Appendix B — Electrical Schematic
Full wiring diagram with all GPIO connections, power rail, BMS, and buck converter.

### Appendix C — ESP-IDF Project Configuration
- `sdkconfig.defaults`
- `partitions.csv`
- `idf_component.yml`

---

## Word Formatting Guidelines

| Setting | Value |
|---------|-------|
| Font | Times New Roman 12pt |
| Line spacing | 1.5 |
| Alignment | Justified |
| Margins | Standard A4 Romanian university format |
| Page numbers | Centered bottom, from first chapter |
| Chapter start | New page |
| Code font | Courier New 10pt or Consolas 10pt |
| Figure caption | **Below** figure: `Figure 5.1: System block diagram` |
| Table caption | **Above** table: `Table 4.1: Microcontroller comparison` |
| Equation numbers | Right-aligned: `(1)` |
| Citation style | Numbered references in brackets `[1]` — IEEE style |
| Figure reference in text | "In Figure 5.1, ..." or "...as shown in Figure 5.1." |

> **Important:** Every claim, specification, or number that is not your own work must have a citation `[N]`.
> Include the supervisor's name exactly as they sign ("Conf. dr. ing. ...") on the title and synthesis pages.
