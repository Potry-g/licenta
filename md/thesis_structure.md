# Bachelor's Thesis Structure: Embedded Low-Latency Tracking System using Edge AI

This document outlines a recommended chapter-by-chapter structure for a **Lucrare de Licență** (Bachelor's Thesis) based on the ESP32S3 Vision Tracking project. This structure specifically focuses on highlighting your engineering decisions, control algorithms, and RTOS architecture.

## Abstract
A half-page summary of the problem (low-latency tracking is difficult on microcontrollers), the proposed solution (ESP32S3 with ESP-DL and custom tracking algorithms), and the results (successful tracking at 12.5 FPS using direct geometric mapping).

---

## Chapter 1: Introduction and State of the Art
*   **1.1 Motivation:** Why is local, edge-based AI important? (Privacy, latency, no internet required).
*   **1.2 State of the Art:** How tracking is traditionally done (Cloud processing, Raspberry Pi). Explain the shift towards TinyML and vector-accelerated microcontrollers like the ESP32S3.
*   **1.3 Thesis Objectives:** What are you trying to build? (A standalone, real-time, 2-axis tracking system using an RTOS).

---

## Chapter 2: Hardware Architecture
*   **2.1 The ESP32S3 Microcontroller:** Discuss the dual-core architecture, vector instructions (crucial for ESP-DL), and the importance of PSRAM.
*   **2.2 Camera Module (e.g., OV2640):** Interfacing via DMA and I2S/DVP. Mention the memory constraints of image buffers.
*   **2.3 Actuators (PWM Servos):** Explain how hobby servos operate (PWM signals, internal control loops).
*   **2.4 Power Management:** Voltage regulation and isolation. Explain why keeping servo noise/current spikes away from the microcontroller is critical to prevent brownouts.

---

## Chapter 3: Software Architecture and RTOS Design
*   **3.1 FreeRTOS Overview:** Explain task scheduling, priorities, and core affinity (pinning tasks to specific cores).
*   **3.2 Dual-Core Implementation:** 
    *   **Core 0:** Servo Control Task (handling PWM hardware timers natively).
    *   **Core 1:** Vision AI Task (running the ESP-DL model).
*   **3.3 Inter-Process Communication (IPC):** Explain how to safely pass data between cores using FreeRTOS Queues (`xQueueReceive` and `xQueueSend`), avoiding race conditions between the AI core and the servo core.

---

## Chapter 4: Edge AI and Computer Vision
*   **4.1 ESP-DL Framework:** How the face detection model runs efficiently on the ESP32S3 using optimized vector instructions.
*   **4.2 Performance Trade-offs:** Comparison of AI models (CNNs) vs. Traditional Computer Vision (Color Blob, QR codes) regarding framerates (FPS) and CPU load. Why you chose the ESP-DL face model.

---

## Chapter 5: Control Algorithms (The Core Engineering Contribution)
*This is the most critical chapter for a high grade. It proves you engineered a solution rather than just copying a tutorial.*
*   **5.1 Limitations of Standard PID Control:** Why continuous PID interpolation fails for low-FPS target updates (Cascaded Control Conflict with the servo's internal loop).
*   **5.2 Direct Geometric Mapping:** Show the mathematical formula for calculating real-world angle offsets from pixel errors based on the camera's Field of View (FOV).
*   **5.3 Frame Prediction (Constant-Velocity Model):** Detail how to compensate for the 80ms AI latency by predicting the target's position between frames. Include why low-pass filtering of noisy bounding boxes is necessary.
*   **5.4 Deadband Implementation:** Explain how to prevent mechanical jitter and overheating when the subject is centered.

---

## Chapter 6: Practical Application and Analysis
*   **6.1 System Tuning:** Tuning the tracking parameters, such as `degrees_per_pixel` and the prediction coefficients.
*   **6.2 Performance Metrics:** Graphs showing CPU usage, memory consumption, latency measurements, and tracking accuracy.
*   **6.3 End Application:** Showcase a practical use-case (e.g., A smart fan that tracks the user, a robotic lamp mount, or an attention-monitor for a workstation).

---

## Chapter 7: Conclusions and Future Work
*   **7.1 Summary of Contributions:** What did you achieve? Explain that you successfully married real-time operating systems with edge AI.
*   **7.2 Future Improvements:** Faster AI models, better camera sensors, or 3D depth tracking with stereo cameras.

---

## References
*   Espressif ESP-IDF Documentation
*   FreeRTOS Manual
*   Academic papers on TinyML and Edge Computing
*   Datasheets for the ESP32S3 and servo motors
