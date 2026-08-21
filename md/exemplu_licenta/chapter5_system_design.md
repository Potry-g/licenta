# Chapter 5: System Design

---

## 5.1 System Architecture Overview

The tracking system is designed as a fully self-contained, battery-powered unit in which all computation, actuation, sensing, and power management are integrated onto a single compact mechanical platform. The system comprises three functional subsystems that interact continuously during operation: the **power subsystem**, which converts stored battery energy into regulated voltages for all loads; the **vision and processing subsystem**, which captures images and executes the detection and control algorithms; and the **actuation subsystem**, which converts computed control signals into physical camera motion.

The overall system architecture is illustrated in Figure 5.1.

> **[TODO: Figure 5.1 — System block diagram. Draw a block diagram showing: 2× Li-Ion 18650 → 2S BMS → 5V Buck Converter → (Servo X on GPIO4) + (Servo Y on GPIO3) + (XIAO ESP32S3 Sense → OV2640 Camera + Push Button on GPIO1). Add arrows showing data and power flow directions.]**

The distinguishing architectural feature of this system is the physical mounting of the camera and the processing unit directly on the servo-actuated arm. Unlike systems that process a fixed camera image and apply digital pan-tilt through image cropping or electronic image stabilization, this system achieves tracking by **physically re-orienting the camera** so that the detected target is centered in the optical axis. This approach eliminates the field-of-view reduction and image quality degradation associated with digital zoom and cropping, and directly maps the servo angle to the pointing direction of the camera without any software coordinate transformation beyond the pixel-to-angle conversion described in Chapter 6.

---

## 5.2 Mechanical Design — 3D Printed Pan-Tilt Arm

### 5.2.1 Pan-Tilt Concept

A pan-tilt mechanism provides two degrees of freedom in camera orientation: **pan** (rotation around the vertical axis, also called yaw) and **tilt** (rotation around the horizontal axis, also called pitch). Together, these two axes allow the camera to point at any target within the angular range of the mechanism.

In the system implemented in this thesis, the pan axis is driven by the X-axis MG90S servo (Servo X), which is mounted at the base of the arm and rotates the entire upper assembly. The tilt axis is driven by the Y-axis MG90S servo (Servo Y), which is mounted on the rotating platform of Servo X and tilts only the camera bracket. This configuration is illustrated in Figure 5.2.

> **[TODO: Figure 5.2 — Labeled diagram or photo of the pan-tilt arm showing: Servo X at the base (pan axis, rotation around vertical), Servo Y above it (tilt axis, rotation around horizontal), camera and ESP32S3 board mounted on the tilt bracket. Label axes X and Y clearly.]**

### 5.2.2 Arm Geometry and Component Mounting

The arm structure was designed and fabricated using fused deposition modeling (FDM) 3D printing in PLA material. The design prioritizes mechanical simplicity, rigidity at the servo mounting points, and clearance for cable routing.

The base of the arm houses Servo X in a fixed pocket, with the servo output shaft aligned to the vertical rotation axis. A platform attached to the servo horn carries the second servo (Servo Y) and constitutes the rotating upper assembly. The camera bracket is attached to the output horn of Servo Y and holds the XIAO ESP32S3 Sense board, which in turn connects to the OV2640 camera module via the integrated flexible flat cable connector. The push button is mounted on the arm structure in a position accessible to the user without interfering with the servo range of motion.

This compact arrangement means that all electronic components — the microcontroller, camera, and user interface button — move as a single rigid body with the servo outputs. The only external connections to the moving assembly are the power supply cable (5 V and GND) and the two servo PWM signal wires, all of which are routed with sufficient slack to accommodate the full angular range of motion without tension.

> **[TODO: Figure 5.3 — Photo of the physical prototype from the front/side, showing the assembled arm with camera visible, button accessible, and wiring.]**

### 5.2.3 Angular Range and Mechanical Constraints

The physical geometry of the arm imposes limits on the usable range of the Y-axis (tilt). The camera bracket can tilt freely within a range of ±30° from the center position (90°), giving an operational range of [60°, 120°]. Beyond this range, the bracket makes contact with the structural elements of the arm, creating a mechanical hard stop. The software enforces these limits by clamping the computed target angle for the Y axis to [60°, 120°] before applying the smoothing filter, as described in Chapter 6.

The X-axis (pan) servo operates over its full rated range of [0°, 180°], as no mechanical obstruction is present along the pan rotation.

---

## 5.3 Electrical Design

### 5.3.1 Power Rail Architecture

The electrical design of the system follows a hierarchical power distribution scheme, as shown in Figure 5.4.

> **[TODO: Figure 5.4 — Power rail schematic: two Li-Ion cells in series (labeled 3.7V each, series = 7.4V nominal) → BMS 2S (with labels for charge port, discharge port, balancing) → Buck Converter (input 6V–8.4V, output 5V regulated) → 5V rail → Servo X (MG90S) + Servo Y (MG90S) + ESP32S3 VIN pin. Include GND common rail. Show decoupling capacitors on the 5V rail near servo connectors.]**

The two Li-Ion 18650 cells are connected in series and routed through the 2S BMS. The BMS output feeds the input of the buck converter. The regulated 5 V output of the buck converter constitutes the system power rail, from which both MG90S servos and the XIAO ESP32S3 Sense module are powered. The ESP32S3's integrated on-board LDO regulator steps the 5 V down to 3.3 V for the microcontroller core and IO.

Decoupling capacitors (minimum 100 µF electrolytic in parallel with 100 nF ceramic) are placed close to the servo power connector pads on the 5 V rail to absorb the transient current spikes generated when the servo motors accelerate or stall. This prevents the voltage dips from propagating to the ESP32S3 supply line and triggering a brownout reset.

### 5.3.2 Signal Connections

All signal connections between the ESP32S3 and the peripheral components are listed in Table 5.1.

**Table 5.1: Complete signal connection table**

| Signal | Direction | ESP32S3 GPIO | Connected to | Notes |
|--------|-----------|-------------|--------------|-------|
| Camera D0 | Input | GPIO 15 | OV2640 Y2 | DVP data bit 0 |
| Camera D1 | Input | GPIO 17 | OV2640 Y3 | DVP data bit 1 |
| Camera D2 | Input | GPIO 18 | OV2640 Y4 | DVP data bit 2 |
| Camera D3 | Input | GPIO 16 | OV2640 Y5 | DVP data bit 3 |
| Camera D4 | Input | GPIO 14 | OV2640 Y6 | DVP data bit 4 |
| Camera D5 | Input | GPIO 12 | OV2640 Y7 | DVP data bit 5 |
| Camera D6 | Input | GPIO 11 | OV2640 Y8 | DVP data bit 6 |
| Camera D7 | Input | GPIO 48 | OV2640 Y9 | DVP data bit 7 |
| VSYNC | Input | GPIO 38 | OV2640 VSYNC | Frame sync |
| HREF | Input | GPIO 47 | OV2640 HREF | Line sync |
| PCLK | Input | GPIO 13 | OV2640 PCLK | Pixel clock |
| XCLK | Output | GPIO 10 | OV2640 XCLK | Master clock (10 MHz) |
| SIOD | Bidir | GPIO 40 | OV2640 SDA | SCCB data |
| SIOC | Output | GPIO 39 | OV2640 SCL | SCCB clock |
| Servo Y PWM | Output | GPIO 3 | MG90S (Y/Tilt) signal | 50 Hz PWM, LEDC CH1 |
| Servo X PWM | Output | GPIO 4 | MG90S (X/Pan) signal | 50 Hz PWM, LEDC CH2 |
| Button | Input | GPIO 1 | Push button | Pull-up, NEGEDGE ISR |
| 5V | Power | VIN | Buck converter output | System 5V rail |
| GND | Power | GND | Common ground | |

The camera module is connected via the integrated FFC (Flexible Flat Cable) connector on the XIAO ESP32S3 Sense board, which routes all DVP and SCCB signals internally. No additional wiring is required between the camera sensor and the microcontroller.

The servo motor signal lines carry only the PWM control signal (logic-level output, 3.3 V), which is interpreted correctly by the MG90S control circuit. The servo power (5 V and GND) is supplied directly from the system power rail, not from the microcontroller GPIO pins. Each servo connector provides three pins: signal (PWM), power (5 V), and ground.

### 5.3.3 Button Circuit

The push button is connected between GPIO 1 and GND. GPIO 1 is configured with the internal pull-up resistor enabled, so the pin reads logic HIGH (3.3 V) when the button is not pressed and transitions to logic LOW (0 V) when pressed. The falling edge (HIGH → LOW, NEGEDGE) triggers the GPIO interrupt. No external pull-up resistor or RC debounce filter is required, as debouncing is implemented in software within the ISR using timestamp comparison.

> **[TODO: Figure 5.5 — Simple schematic of the button circuit: GPIO1 with internal pull-up shown as a resistor to 3.3V, button connecting GPIO1 to GND. Optional: add a 10kΩ external pull-up resistor for clarity in the schematic, even if the internal one is used.]**

---

## 5.4 System Integration

### 5.4.1 Assembly Overview

The integration of hardware components follows this sequence:

1. The base servo (Servo X) is secured in the arm base pocket. The servo horn is attached and oriented so that the center position (90°, 1500 µs pulse) aligns the arm's longitudinal axis with the forward-facing direction.
2. The upper platform is attached to the Servo X horn. Servo Y is mounted in the upper platform pocket, oriented so that the center position aligns the camera to the horizontal.
3. The camera bracket is attached to the Servo Y horn. The XIAO ESP32S3 Sense board is mounted on the bracket with the camera connector accessible.
4. The OV2640 camera module is connected to the board via the FFC connector.
5. The push button is installed in its designated pocket on the arm structure.
6. Signal wires (PWM for Servo X and Servo Y, button) are routed through the arm's cable channel and connected to the appropriate GPIO pins.
7. The power cable (5 V rail and GND) is connected to the arm's power inlet and distributed to the servo connectors and the ESP32S3 VIN pin.
8. The BMS and buck converter are mounted on or adjacent to the base structure. The battery pack is connected to the BMS input.

### 5.4.2 Boot Sequence

Upon power-on, the following initialization sequence occurs:

1. The ESP32S3 boot ROM executes and loads the application from flash memory.
2. `app_main()` configures GPIO 1 as an interrupt input, installs the ISR service, and registers `button_isr_handler`.
3. The FreeRTOS queue (`servo_queue`) is created with a depth of 1.
4. `servo_task` is created on Core 0 at priority 5. It immediately initializes the LEDC timers and channels, sets both servos to the center position (90°), and begins the 50 Hz control loop.
5. `vision_task` is created on Core 1 at priority 5. After a 1-second startup delay (to allow power supply stabilization), it initializes the OV2640 camera driver and begins the capture-detect-send loop.
6. The system is operational. The default tracking mode is `MODE_FACE`.

The 1-second startup delay in `vision_task` is a deliberate design choice: the camera module requires a settling time after power-on before the image sensor produces stable output. Attempting to capture frames immediately after power-on can result in incorrectly initialized sensor state.

> **[TODO: Figure 5.6 — Boot sequence flowchart: Power ON → app_main() → GPIO config → Queue create → Create servo_task (Core 0) → Create vision_task (Core 1, with 1s delay) → Camera init → Main loop (two parallel paths: servo loop at 50Hz and vision loop at ~12.5 FPS)]**

---

## References for Chapter 5

*(Verify and complete with actual publication details before submission.)*

[1] Espressif Systems. *ESP32-S3 Technical Reference Manual*. Available: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf

[2] Seeed Studio. *XIAO ESP32S3 Sense Schematic*. Available: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

[3] Tower Pro. *MG90S Servo Motor Datasheet*. Available: https://www.towerpro.com.tw/product/mg90s-3/

[4] OmniVision Technologies. *OV2640 Camera Module Datasheet*.
