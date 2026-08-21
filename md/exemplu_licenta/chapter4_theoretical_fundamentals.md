# Chapter 4: Theoretical Fundamentals

---

## 4.1 Power Management

### 4.1.1 Lithium-Ion Batteries

The tracking system presented in this thesis is powered by two lithium-ion (Li-Ion) 18650 cylindrical cells. The 18650 designation refers to the physical dimensions of the cell: 18 mm in diameter and 65 mm in length. Li-Ion cells of this format are among the most widely used rechargeable energy storage elements in portable electronics and small robotic systems, owing to their high energy density, low self-discharge rate, and well-established charging infrastructure [1].

A single Li-Ion 18650 cell has a nominal voltage of 3.6–3.7 V, a fully charged voltage of 4.2 V, and a minimum discharge cutoff voltage of 2.5–3.0 V, below which irreversible capacity loss occurs. The two cells in this system are connected in series, forming a 2S configuration with a nominal pack voltage of 7.2–7.4 V and a maximum charged voltage of 8.4 V. Connecting cells in series increases the pack voltage while maintaining the capacity (in ampere-hours, Ah) of a single cell.

The energy stored in the battery pack is given by:

**E = V_nom × C**  *(1)*

where *V_nom* is the nominal pack voltage and *C* is the cell capacity in ampere-hours. The runtime of the system depends on the average current drawn by all loads, primarily the two MG90S servo motors and the ESP32S3 microcontroller.

Li-Ion cells are sensitive to overcharging, over-discharging, and excessive discharge current. Overcharging above 4.2 V per cell can cause electrolyte decomposition, gas generation, and in severe cases thermal runaway — an uncontrolled exothermic reaction that can result in fire or explosion. Over-discharging below the cutoff voltage causes copper dissolution at the anode, permanently reducing cell capacity. These risks make a dedicated protection circuit mandatory for any multi-cell Li-Ion configuration [2].

### 4.1.2 Battery Management System (BMS) — 2S Configuration

The Battery Management System (BMS) is a protection circuit that sits between the battery cells and the external load. In the 2S configuration used in this project, the BMS performs the following critical functions:

**Overcharge protection:** When either cell reaches 4.2 V during charging, the BMS disconnects the charging path, preventing further voltage increase.

**Over-discharge protection:** When either cell drops below the minimum cutoff voltage (typically 2.5–3.0 V, depending on the BMS threshold), the BMS disconnects the discharge path, protecting the cells from deep discharge damage.

**Short-circuit protection:** In the event of a short circuit on the output terminals, the BMS detects the resulting overcurrent and interrupts the discharge path within microseconds, protecting both the cells and the connected circuitry.

**Cell balancing:** In a series-connected pack, manufacturing tolerances cause individual cells to have slightly different capacities and self-discharge rates. Without balancing, the weakest cell will reach the cutoff voltage first, leaving capacity unused in the stronger cell. The BMS implements passive balancing by dissipating excess energy from the higher-voltage cell through a bleed resistor during charging, equalizing the cell voltages over time [3].

### 4.1.3 Buck Converter (Step-Down DC-DC Converter)

The 2S battery pack delivers an output voltage ranging from approximately 6.0 V (near-depleted) to 8.4 V (fully charged). The servo motors and the ESP32S3 microcontroller both operate at 5 V. A voltage conversion stage is therefore required between the battery pack and the system loads.

A linear voltage regulator (LDO, Low Dropout) achieves this by dissipating the excess voltage as heat. The power dissipated in an LDO is:

**P_loss = (V_in − V_out) × I_load**  *(2)*

At a battery voltage of 7.4 V, output of 5 V, and a total load current of approximately 600 mA (two servos under moderate load plus the ESP32S3), the LDO would dissipate approximately 1.44 W as heat. This corresponds to an efficiency of only 68%, meaning that 32% of the battery energy is wasted as heat. The LDO would also require a heatsink to prevent thermal shutdown.

A buck converter (switching step-down regulator) avoids this problem by using a high-frequency switching element (typically a MOSFET) to transfer energy in discrete packets through an inductor and capacitor filter network. The output voltage is controlled by the duty cycle of the switching signal. Modern synchronous buck converters achieve efficiencies of 85–95% over a wide range of input voltages and load currents, regardless of the voltage difference between input and output [4]. This higher efficiency directly translates to longer battery runtime and eliminates the need for a heatsink.

A critical consideration in this system is the current draw of the MG90S servo motors under stall conditions. At 5 V, a single MG90S can draw up to 500–700 mA when its output shaft is mechanically blocked. With two servos potentially stalling simultaneously, the instantaneous current demand can approach 1.4 A. These short-duration current spikes must not propagate to the ESP32S3 supply rail, as voltage dips below approximately 3.0 V on the microcontroller's supply can cause a brownout reset. Adequate output capacitance on the buck converter's output, combined with a common ground plane, is essential to absorb these transient loads and maintain supply stability.

---

## 4.2 The Microcontroller — Seeed XIAO ESP32S3 Sense

### 4.2.1 ESP32S3 Architecture

The Seeed XIAO ESP32S3 Sense is a compact development module built around Espressif's ESP32-S3 System-on-Chip (SoC). The ESP32-S3 integrates two Xtensa LX7 processor cores operating at up to 240 MHz, 512 KB of internal SRAM, 8 MB of external PSRAM (Pseudo-Static RAM), and 8 MB of flash memory, along with a rich set of peripherals and wireless connectivity (Wi-Fi 802.11 b/g/n and Bluetooth 5.0 LE) [5].

The Xtensa LX7 core represents a significant advancement over the LX6 core used in earlier ESP32 variants. The most relevant improvement for this project is the introduction of the **PIE (Processor Instruction Extensions)** instruction set — a set of 128-bit SIMD (Single Instruction, Multiple Data) vector operations that allow the processor to perform the same arithmetic operation on multiple data elements simultaneously. This is the hardware mechanism exploited by the ESP-DL library to accelerate convolutional neural network inference: the multiply-accumulate operations at the heart of convolution can be performed on vectors of 8-bit integer operands, achieving a throughput several times higher than scalar computation alone [6].

The external **8 MB PSRAM** is a critical resource for this application. A single camera frame at 240×240 pixels in RGB565 format occupies 115,200 bytes (~113 KB) — far exceeding the 512 KB of internal SRAM when combined with the stack space and heap requirements of FreeRTOS, the ESP-DL model weights, and the application code. Allocating the frame buffer in PSRAM frees internal SRAM for time-critical data structures and stack space, enabling stable system operation.

### 4.2.2 Peripherals Used

**LEDC (LED Control) Peripheral:** Despite its name, the LEDC peripheral is a general-purpose hardware PWM generator used in this project for servo motor control. The ESP32S3 provides 8 LEDC channels with 4 associated timers, configurable to resolutions of up to 14 bits. In this project, two independent LEDC timers (TIMER_1 and TIMER_2) are configured at 50 Hz with 13-bit resolution, each driving one servo channel: TIMER_1 drives CHANNEL_1 on GPIO 3 (Y axis), and TIMER_2 drives CHANNEL_2 on GPIO 4 (X axis). Using separate timers for the two servos ensures that their PWM periods are completely independent — updating one servo's duty cycle does not affect the other's signal timing.

**GPIO with Interrupt:** GPIO 1 is configured as an input with internal pull-up enabled and an interrupt on the falling edge (NEGEDGE), triggered when the user presses the push button. The GPIO interrupt service routine is registered using `gpio_isr_handler_add()` and handles mode switching with software debouncing.

**DVP (Digital Video Port) Camera Interface:** The OV2640 camera module is connected via an 8-bit parallel DVP interface using GPIOs 15, 17, 18, 16, 14, 12, 11, and 48 for the data lines (Y2–Y9), GPIO 38 for VSYNC, GPIO 47 for HREF, GPIO 13 for PCLK, and GPIO 10 for XCLK. Camera configuration registers are accessed via the SCCB protocol (I2C-compatible) on GPIOs 40 (SDA) and 39 (SCL).

### 4.2.3 Electrical Considerations

The ESP32S3 operates at a core supply voltage of 3.3 V, regulated on-board from the 5 V input by the XIAO module's integrated LDO. Each GPIO pin can source or sink a maximum of 40 mA, with a recommended operating current of 20 mA. The servo motor signal lines (PWM outputs) do not draw significant current from the GPIO pins, as the servo's internal electronics only need to sense the logic level of the signal. The servo motors themselves draw their operating current directly from the 5 V supply rail, not from the GPIO pins.

### 4.2.4 Comparison with Alternative Microcontrollers

The choice of the ESP32S3 over alternative platforms is justified by the combination of features required for this specific application. Table 4.1 presents a comparative overview of candidate platforms.

**Table 4.1: Comparison of candidate microcontroller platforms**

| Feature | XIAO ESP32S3 | Arduino Nano (ATmega328P) | STM32F411 (Nucleo) | Raspberry Pi Zero 2W |
|---|---|---|---|---|
| CPU | Xtensa LX7 × 2, 240 MHz | AVR, 16 MHz | Cortex-M4, 100 MHz | Cortex-A53 × 4, 1 GHz |
| AI vector instructions | PIE (128-bit SIMD) | None | None | None (no NPU) |
| Integrated camera I/F | DVP (built-in) | No | No | No (CSI via adapter) |
| External RAM | 8 MB PSRAM | None | None | 512 MB LPDDR2 |
| OS requirement | None (FreeRTOS) | None | None | Linux required |
| Typical power | ~240 mW | ~150 mW | ~200 mW | ~1.2 W (idle) |
| Face detection support | ESP-DL (native) | Not feasible | Limited | OpenCV (Linux) |
| Cost (approx.) | Low | Very low | Medium | Low |

The Arduino Nano lacks the memory, processing power, and camera interface required for any form of real-time image processing. The STM32F411 offers sufficient processing power for classical computer vision but lacks native camera interface hardware and is not supported by any readily available embedded face detection library. The Raspberry Pi Zero 2W could run OpenCV-based face detection under Linux, but its power consumption, boot time, non-deterministic OS scheduling, and physical size make it unsuitable for a compact battery-powered design.

---

## 4.3 Camera Module — OV2640

### 4.3.1 DVP Interface

The OV2640 is a CMOS image sensor manufactured by OmniVision Technologies, widely used in embedded camera applications due to its low cost, small package, and compatibility with the DVP parallel interface supported by ESP32-series microcontrollers [7].

The DVP (Digital Video Port) interface transmits pixel data as an 8-bit parallel word synchronized to a pixel clock (PCLK). The horizontal and vertical timing is controlled by the HREF and VSYNC signals, respectively. The XCLK (external clock) signal is provided by the ESP32S3 at 10 MHz as the master clock for the camera's internal PLL, from which the camera derives its operating frequencies. Camera configuration — including resolution, pixel format, brightness, contrast, and frame rate — is performed through the SCCB protocol, which is electrically and functionally equivalent to I2C, using register writes to the OV2640's internal register file.

### 4.3.2 Pixel Format — RGB565

The pixel format selected for this application is **RGB565**, in which each pixel is encoded in 16 bits: 5 bits for the red channel, 6 bits for the green channel (the human eye is most sensitive to green), and 5 bits for the blue channel. This format provides a good balance between color fidelity and memory efficiency: at 240×240 pixels, a single frame requires exactly 115,200 bytes, compared to 172,800 bytes for 24-bit RGB888.

RGB565 is the native input format of the ESP-DL face detection model, eliminating any pixel format conversion step between camera capture and neural network inference. For the custom HSV color blob detection algorithm, the RGB565 pixel values are decoded and converted to HSV in software, as described in Chapter 6.

The RGB565 pixel data is stored in memory in big-endian byte order as output by the OV2640: the most significant byte is stored first, followed by the least significant byte. Decoding a pixel at buffer index *i* is performed as:

**pixel = (buf[i] << 8) | buf[i+1]**  *(3)*

**R = (pixel >> 8) & 0xF8**  *(4)*

**G = (pixel >> 3) & 0xFC**  *(5)*

**B = (pixel << 3) & 0xF8**  *(6)*

The masking operations (0xF8 for R and B, 0xFC for G) extract the 5-bit and 6-bit channel values and scale them to the 8-bit range by left-shifting to the most significant bits.

### 4.3.3 Frame Buffer Configuration

The ESP32 camera driver allocates the frame buffer in external PSRAM (`CAMERA_FB_IN_PSRAM`), as the 115,200-byte frame size far exceeds any practical allocation from the 512 KB internal SRAM when accounting for other system memory requirements. The driver is configured with `fb_count = 2`, enabling double buffering: while the camera DMA engine fills one buffer with a new frame, the application task processes the previously captured frame in the other buffer. This eliminates stalls caused by waiting for frame capture to complete.

The grab mode is set to `CAMERA_GRAB_WHEN_EMPTY`, which instructs the driver to capture a new frame only when the application has released the previous frame buffer. This ensures that the vision task always processes the most recently captured frame, avoiding the accumulation of stale frames in a queue.

### 4.3.4 Camera Field of View

The field of view (FOV) of the camera system is a fundamental parameter for the tracking control algorithm, as it defines the angular extent of the scene captured in each frame and therefore the conversion factor between pixel displacement and angular correction.

The OV2640 sensor combined with the lens assembly on the XIAO ESP32S3 Sense module has a diagonal FOV of approximately 65°. For a square frame (240×240 pixels, aspect ratio 1:1), the horizontal and vertical fields of view are equal. The FOV in each axis can be derived from the diagonal FOV using:

**FOV_H = FOV_V = 2 × arctan(tan(FOV_diag / 2) / √2)**  *(7)*

For a diagonal FOV of 65°, this gives an approximate horizontal and vertical FOV of **~49°**. In the implemented system, a value of 40° was used for both axes based on empirical calibration of the tracking response — this conservative value accounts for the effective FOV reduction caused by the lens distortion and the specific mounting of the camera. The sensitivity of the tracking algorithm to this parameter is discussed in Chapter 6.

---

## 4.4 Servo Motors — MG90S

### 4.4.1 Operating Principle

The MG90S is a micro servo motor manufactured by Tower Pro, widely used in small robotics applications. It integrates a brushed DC motor, a metal gearbox with a reduction ratio of approximately 1:100, a potentiometer mechanically coupled to the output shaft for position sensing, and an internal analog control circuit that implements a proportional position controller [8].

The desired output shaft angle is communicated to the servo by a PWM signal at a fixed frequency of **50 Hz** (period = 20 ms). The pulse width within each period encodes the target position:

**pulse_width_µs = 500 + (angle / 180) × 2000**  *(8)*

This gives:
- 500 µs → 0°
- 1500 µs → 90° (center position)
- 2500 µs → 180°

The internal control circuit measures the actual shaft angle through the potentiometer and drives the motor in the direction that reduces the error between the measured position and the commanded position. This constitutes a hardware-implemented closed-loop proportional controller internal to the servo.

The MG90S is rated for a stall torque of 1.8 kg·cm at 4.8 V and 2.2 kg·cm at 6 V, with a no-load operating current of approximately 170 mA and a stall current of up to 700 mA at 5 V. The metal gearbox provides better durability and higher torque than the plastic gearbox of the otherwise similar SG90 servo, making it more suitable for continuous use in a pan-tilt mechanism.

### 4.4.2 PWM Generation Using the LEDC Peripheral

On the ESP32S3, PWM signals for servo control are generated by the LEDC (LED Control) peripheral. The peripheral is configured with a 13-bit timer resolution (`LEDC_TIMER_13_BIT`), providing 2¹³ = 8192 discrete duty cycle steps. At 50 Hz, the period of the PWM signal is 20 ms = 20,000 µs. The duty cycle register value corresponding to a given pulse width is:

**duty = (pulse_width_µs × 8191) / 20000**  *(9)*

This mapping gives:
- 500 µs → duty = 204
- 1500 µs → duty = 614
- 2500 µs → duty = 1023

The angular resolution achievable with this configuration is:

**Δangle = 180° / (1023 − 204) ≈ 0.22°/step**  *(10)*

This resolution is sufficient for the tracking application, where the minimum angular correction determined by the camera FOV and deadband is approximately 2.5° (15 pixels × 0.167°/pixel).

Two independent LEDC timers are used — TIMER_1 for the Y-axis servo on GPIO 3, and TIMER_2 for the X-axis servo on GPIO 4 — so that updating the duty cycle of one channel does not introduce any phase or timing disturbance in the other.

### 4.4.3 Pan-Tilt Mechanism and Physical Angle Limits

The two MG90S servos are mounted in a pan-tilt configuration on a 3D-printed arm. The X-axis servo (pan) is mounted at the base of the arm and rotates the entire upper assembly, including the Y-axis servo, the camera, and the ESP32S3 module, around the vertical axis. The Y-axis servo (tilt) is mounted on the rotating platform and tilts the camera bracket around the horizontal axis.

This configuration means that the camera is always rigidly attached to the servo output shaft — any servo movement directly translates to a change in camera pointing direction, and therefore a shift of the scene in the camera frame. Tracking is achieved by physically re-orienting the camera so that the target is centered in the frame, rather than by digital image manipulation.

The physical geometry of the 3D-printed arm imposes a constraint on the Y-axis range of motion: the tilt axis is limited to the angular range [60°, 120°] — a symmetric ±30° range around the center position of 90° — to prevent mechanical interference between the camera bracket and the arm structure. The X-axis servo operates over its full range of [0°, 180°]. These constraints are enforced in software by clamping the computed target angles before applying the smoothing filter.

---

## 4.5 Software Framework — ESP-IDF and FreeRTOS

### 4.5.1 ESP-IDF

The ESP-IDF (Espressif IoT Development Framework) is the official software development kit for the ESP32 family of microcontrollers, maintained by Espressif Systems. It provides a CMake-based build system, a component manager for dependency management, drivers for all on-chip peripherals, the lwIP TCP/IP stack, and full integration with the FreeRTOS real-time operating system [9].

The choice of ESP-IDF over the Arduino framework for this project is motivated by several factors. ESP-IDF provides direct access to all peripheral configuration registers and driver APIs, enabling fine-grained control over the LEDC timer configuration and camera driver settings that are abstracted away in the Arduino environment. The component manager allows seamless integration of the `esp-who` library containing the ESP-DL face detection model. The build system supports deterministic, reproducible builds with explicit version pinning of all dependencies.

### 4.5.2 FreeRTOS

FreeRTOS is a widely used open-source real-time operating system kernel designed for embedded systems. On the ESP32S3, FreeRTOS is integrated directly into the ESP-IDF and runs on both processor cores simultaneously — each core runs its own FreeRTOS scheduler instance, with shared inter-core communication primitives [10].

The core scheduling mechanism of FreeRTOS is preemptive priority-based scheduling: at each tick, the scheduler selects the highest-priority task that is in the Ready state and assigns the processor to it. Tasks can be placed in the Blocked state — relinquishing the processor — while waiting for time delays (`vTaskDelay`), queue events (`xQueueReceive`), or other synchronization primitives, allowing other tasks or the idle task to run.

The key FreeRTOS constructs used in this project are:

**Tasks:** Independent units of execution, each with its own stack. Created using `xTaskCreatePinnedToCore()`, which assigns the task to a specific CPU core and sets its priority.

**Queues:** Thread-safe FIFO buffers for passing data between tasks. Created using `xQueueCreate(depth, itemSize)`. Items are sent with `xQueueSend()` and received with `xQueueReceive()`. Queue operations are atomic and safe to use from both tasks and ISRs.

**Interrupt Service Routines (ISR):** Hardware-triggered callback functions registered via `gpio_isr_handler_add()`. ISR code must be located in internal RAM (annotated with `IRAM_ATTR`) to avoid cache miss stalls during Flash access at interrupt time. ISRs may use special ISR-safe variants of FreeRTOS APIs (e.g., `xQueueSendFromISR()`).

---

## 4.6 Edge AI — ESP-DL and the HumanFaceDetect Model

ESP-DL is Espressif's proprietary deep learning inference library, optimized for the ESP32S3 and the earlier ESP32S2. It provides implementations of common neural network layer types — convolution, depthwise convolution, batch normalization, ReLU, pooling, and fully connected layers — compiled to use the PIE vector instructions of the Xtensa LX7 core for maximum throughput [11].

The `HumanFaceDetect` class, provided as part of the `esp-who` application framework, implements a multi-stage face detection pipeline. The first stage applies a lightweight convolutional network to the full input frame to identify candidate face regions. A second stage applies a more accurate network to each candidate region to refine the bounding box and reject false positives. The model weights are stored in flash memory and are quantized to 8-bit integers, reducing the memory footprint and replacing floating-point operations with integer arithmetic that maps efficiently to the PIE vector instructions.

The model accepts an input image descriptor of type `dl::image::img_t`, specifying the data pointer, width, height, and pixel type. For this project, the pixel type is `DL_IMAGE_PIX_TYPE_RGB565`, meaning no pixel format conversion is required between the camera frame buffer and the model input. The model returns a `std::list<dl::detect::result_t>`, where each element contains a bounding box `box[4]` = `{x1, y1, x2, y2}` in pixel coordinates. The centroid of the first detected face is computed as:

**cx = (box[0] + box[2]) / 2**  *(11)*

**cy = (box[1] + box[3]) / 2**  *(12)*

At the configured resolution of 240×240 pixels, the face detection inference runs at approximately **12.5 FPS** (80 ms per frame), with the computation pinned to Core 1 of the ESP32S3.

---

## 4.7 Computer Vision — HSV Color Space and Blob Detection

### 4.7.1 The HSV Color Model

The RGB (Red, Green, Blue) color model represents colors as additive combinations of three primary light components. While convenient for display hardware, RGB is poorly suited for color-based object segmentation in natural scenes, because the RGB values of a surface change substantially with varying illumination intensity. A blue object illuminated by a bright light source will appear much lighter (higher R, G, and B values) than the same object in shadow, making it difficult to define a fixed RGB threshold that reliably identifies the object across different lighting conditions.

The HSV (Hue, Saturation, Value) color model separates the chromatic information (color identity) from the luminance (brightness). The Hue channel encodes the dominant wavelength of the color as an angle on a color wheel (0°–360°). The Saturation channel describes the purity of the color (0 = grey, 1 = fully saturated). The Value channel describes the overall brightness (0 = black, 1 = maximum brightness).

This separation is advantageous for color-based tracking because the Hue of a surface remains relatively stable across moderate variations in illumination intensity, while only the Value channel changes significantly. A well-chosen Hue range can therefore identify a specific color under varying lighting conditions more robustly than any fixed RGB threshold [12].

### 4.7.2 RGB to HSV Conversion

The conversion from normalized RGB (r, g, b ∈ [0, 1]) to HSV is defined as follows. Let:

**C_max = max(r, g, b)**  *(13)*

**C_min = min(r, g, b)**  *(14)*

**Δ = C_max − C_min**  *(15)*

The Value component is:

**V = C_max**  *(16)*

The Saturation component is:

**S = Δ / C_max**, if C_max > 0; **S = 0** otherwise  *(17)*

The Hue component is computed as:

If Δ = 0: **H = 0** (undefined, achromatic)  *(18)*

If C_max = r: **H = 60° × (g − b) / Δ**, adjusted to [0°, 360°)  *(19)*

If C_max = g: **H = 60° × (2 + (b − r) / Δ)**  *(20)*

If C_max = b: **H = 60° × (4 + (r − g) / Δ)**  *(21)*

For the target color in this project — a blue object — the Hue value falls in the range [200°, 255°], corresponding to the blue-to-indigo segment of the color wheel. The detection thresholds applied are H ∈ (200°, 255°), S > 0.4, and V > 0.2, filtering out achromatic pixels (low saturation) and very dark pixels (low value) that might coincidentally fall within the Hue range.

---

## References for Chapter 4

*(Verify and complete with actual publication details before submission.)*

[1] — Reference on Li-Ion 18650 cell characteristics (search: "lithium ion 18650 battery characteristics review")

[2] — Reference on Li-Ion safety and thermal runaway (search: "lithium ion battery thermal runaway safety")

[3] — Reference on BMS cell balancing (search: "battery management system cell balancing techniques")

[4] — Reference on buck converter efficiency (search: "synchronous buck converter efficiency analysis")

[5] Espressif Systems. *ESP32-S3 Technical Reference Manual*. Available: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf

[6] Espressif Systems. *ESP-DL: Espressif Deep Learning Library*. Available: https://github.com/espressif/esp-dl

[7] OmniVision Technologies. *OV2640 Camera Module Datasheet*.

[8] Tower Pro. *MG90S Servo Motor Datasheet*. Available: https://www.towerpro.com.tw/product/mg90s-3/

[9] Espressif Systems. *ESP-IDF Programming Guide*. Available: https://docs.espressif.com/projects/esp-idf

[10] Barry, R. *Mastering the FreeRTOS Real Time Kernel — A Hands-On Tutorial Guide*. Available: https://www.freertos.org/Documentation/RTOS_book.html

[11] Espressif Systems. *ESP-WHO: Face Detection and Recognition*. Available: https://github.com/espressif/esp-who

[12] A. R. Smith, "Color gamut transform pairs," *ACM SIGGRAPH Computer Graphics*, vol. 12, no. 3, pp. 12–19, 1978.
