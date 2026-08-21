# Chapter 3: State of the Art

---

## 3.1 Introduction

Real-time object tracking refers to the continuous process of detecting and following a target — such as a human face, a colored object, or any other region of interest — across successive frames of a video stream. The system must determine the position of the target in each new frame and, in the context of physical tracking systems, translate that position into a mechanical correction that keeps the target centered in the camera's field of view.

The challenge of real-time tracking lies in the tension between computational complexity and timing constraints. Accurate detection algorithms, particularly those based on deep neural networks, demand significant processing resources. At the same time, the control loop driving the physical actuators must operate at a consistent, high frequency to ensure smooth and stable motion. Traditionally, reconciling these two requirements has required powerful and power-hungry hardware.

Applications of real-time tracking systems span a broad range of fields. In surveillance and security, pan-tilt camera systems automatically follow persons of interest without human operator intervention [1]. In robotics, tracking enables a robot to maintain visual contact with a target object during manipulation or navigation tasks [2]. In consumer electronics, automatic framing cameras — such as those found in modern video conferencing systems — use face tracking to keep the speaker centered in the frame. In drone technology, ground-following and person-tracking capabilities rely entirely on real-time vision and control loops [3]. In assistive technology, tracking systems can control interface pointers or robotic arms using gaze or body tracking as input.

The core challenge addressed in this thesis is the implementation of such a system on a severely resource-constrained microcontroller platform, without a general-purpose operating system, without a dedicated GPU or neural processing unit, and powered entirely by a compact battery pack — while still achieving real-time performance.

---

## 3.2 Traditional Object Tracking Approaches

### 3.2.1 Classical Computer Vision Methods

Before the widespread adoption of deep learning, object tracking was performed using classical computer vision techniques. These methods typically operate by extracting low-level image features and using them to match or locate a target across frames.

Color-based tracking relies on the assumption that a target can be characterized by a distinctive color distribution. The image is transformed from the RGB color space into a more perceptually uniform space, such as HSV (Hue, Saturation, Value) or YCbCr, and pixels matching the target's color profile are segmented using thresholding. The centroid of the resulting binary mask is then used as the estimated target position. While computationally inexpensive, color-based tracking is sensitive to lighting variations and cannot distinguish between multiple objects of similar color [4].

Template matching methods maintain a reference patch (template) of the target's appearance and search for it in each new frame by computing a similarity metric — such as normalized cross-correlation — across all candidate positions. Methods such as MOSSE (Minimum Output Sum of Squared Error) [5] and KCF (Kernelized Correlation Filters) [6] extend this concept using frequency-domain operations to significantly accelerate the search. While effective for short-term tracking, these methods struggle with target deformation, occlusion, and appearance changes over time.

Optical flow methods, such as the Lucas-Kanade tracker, estimate the apparent motion of feature points between consecutive frames by analyzing local image gradients [7]. These methods are efficient and can track multiple points simultaneously, but require a good initial detection and degrade rapidly under fast motion or significant illumination changes.

### 3.2.2 PC and Cloud-Based Systems

The most prevalent approach to high-accuracy object tracking has historically been the use of general-purpose computers running full operating systems and software libraries. OpenCV, an open-source computer vision library originally developed by Intel, provides implementations of a wide range of tracking algorithms — from classical methods such as MeanShift and CamShift to modern deep-learning-based detectors [8]. Running on a desktop processor or a laptop GPU, these systems can achieve both high accuracy and high frame rates simultaneously.

However, PC-based systems are fundamentally unsuitable for mobile or embedded applications. A typical desktop workstation consumes between 100 W and 400 W of electrical power, requires a fixed power source, and is physically large. Even laptop-class hardware, while more compact, consumes tens of watts and requires active cooling. The cost of such systems also makes them impractical for disposable or low-budget applications.

Cloud-based tracking systems attempt to address some of these limitations by offloading computation to remote servers. The embedded device captures images and transmits them over a network connection; the server processes the images and returns the detected position. While this approach allows the use of arbitrarily powerful hardware for inference, it introduces network latency that is fundamentally incompatible with the real-time requirements of physical control loops. Round-trip latency of 50–300 ms is typical for cloud inference, far exceeding the 20 ms control period required for stable servo operation. Furthermore, cloud-based systems require a continuous and reliable network connection, which is unavailable in many deployment environments, and raise significant concerns regarding data privacy, as raw image data is transmitted to third-party servers.

### 3.2.3 Single-Board Computer (SBC) Based Systems

A widely adopted middle ground between full desktop systems and bare-metal microcontrollers is the single-board computer (SBC) running a full Linux-based operating system. The Raspberry Pi family of boards has become particularly popular for hobbyist and research tracking projects. A Raspberry Pi 4 Model B, for example, is equipped with a 1.8 GHz quad-core ARM Cortex-A72 processor and 4–8 GB of LPDDR4 RAM, and can run OpenCV-based tracking algorithms at useful frame rates.

However, the Raspberry Pi and similar SBCs present several drawbacks for applications requiring autonomy and portability. Their power consumption ranges from approximately 3 W (Raspberry Pi Zero 2 W) to over 7 W (Raspberry Pi 4 under load), which translates to short battery runtimes even with large battery packs. They require a boot process of tens of seconds before becoming operational, making them unsuitable for instant-on applications. The presence of a full operating system introduces non-deterministic task scheduling, making it difficult to guarantee consistent timing for real-time control loops without additional real-time extensions. Their physical size and the need for active cooling (heat sinks or fans) also make integration into compact mechanical designs challenging [9].

### 3.2.4 FPGA-Based Systems

Field-Programmable Gate Arrays (FPGAs) offer another alternative for real-time vision processing. By implementing image processing pipelines directly in hardware logic, FPGAs can achieve extremely low and deterministic latency, with frame processing times on the order of microseconds. Research systems implementing convolutional neural networks in FPGA fabric have demonstrated high throughput with low power consumption [10].

However, FPGA-based solutions are not accessible for most embedded applications due to their cost, the complexity of hardware description language (HDL) programming, long development cycles, and the need for specialized tools and expertise. They are therefore primarily found in industrial and military applications rather than in low-cost autonomous devices.

---

## 3.3 Edge AI and the TinyML Movement

### 3.3.1 Definition and Motivation

Edge AI refers to the execution of machine learning inference algorithms directly on end-user devices, without reliance on cloud servers or external computation resources. The subset of Edge AI concerned with extremely resource-constrained devices — microcontrollers with kilobytes of RAM and clock speeds of tens to hundreds of megahertz — is commonly referred to as TinyML [11].

The motivation for TinyML is driven by several converging trends. The proliferation of Internet of Things (IoT) devices has created demand for intelligent processing at the sensor level, where network connectivity may be unreliable or unavailable. Concerns about data privacy have increased pressure to process sensitive data (such as images of persons) locally rather than transmitting it to remote servers. The need for low-latency responses in physical systems — such as the real-time control loop in this project — makes cloud offloading impractical. Finally, advancements in model compression techniques, including quantization, pruning, and knowledge distillation, have made it possible to deploy capable neural networks in environments with as little as 256 KB of RAM and no floating-point hardware unit [12].

### 3.3.2 Model Compression Techniques

The deployment of neural networks on microcontrollers requires significant reduction of model size and computational complexity compared to their full-scale counterparts. The primary technique used for this purpose is quantization: replacing the 32-bit floating-point weights and activations used during training with lower-precision integer representations, typically 8-bit integers (INT8). This reduces model size by a factor of four and replaces slow floating-point operations with fast integer arithmetic that can be natively accelerated by vector instruction extensions. Post-training quantization and quantization-aware training are the two main approaches to this process [13].

Pruning removes weights or entire neurons from a trained network that contribute minimally to the output, reducing both model size and the number of operations required for inference. Knowledge distillation trains a smaller "student" model to reproduce the behavior of a larger "teacher" model, achieving competitive accuracy at a fraction of the computational cost [14].

### 3.3.3 Hardware Acceleration on Microcontrollers

Modern microcontrollers increasingly include hardware support for accelerated neural network inference. ARM's Cortex-M55 processor incorporates Helium (M-Profile Vector Extension), a set of SIMD (Single Instruction, Multiple Data) instructions that can process multiple data elements in parallel, significantly accelerating convolution and matrix multiplication operations used in CNNs [15].

Espressif's ESP32S3, used in this project, includes the PIE (Processor Instruction Extensions) instruction set, which provides 128-bit SIMD vector operations on the Xtensa LX7 core. The ESP-DL library, developed by Espressif, is specifically optimized to exploit these instructions for neural network inference, enabling the execution of face detection models at rates unachievable on earlier ESP32 variants without vector extensions [16].

Google's Edge TPU (used in the Coral platform) and similar dedicated neural processing units (NPUs) represent the high end of the edge inference hardware spectrum, offering throughput of several trillion operations per second (TOPS) in a small package. However, these components are more expensive and less integrated than microcontroller-class solutions, and are therefore more appropriate for applications requiring higher accuracy or throughput than what microcontrollers can provide.

---

## 3.4 Existing Embedded Tracking Solutions

### 3.4.1 OpenMV

OpenMV is a dedicated machine vision microcontroller platform designed specifically for embedded computer vision applications [17]. It combines a custom hardware module — based on ARM Cortex-M7 or H7 processors — with a firmware environment that exposes a Python API (MicroPython) for camera access and image processing. OpenMV provides built-in functions for color blob tracking, template matching, face detection using Haar cascades, and limited neural network inference.

Compared to the system implemented in this thesis, OpenMV is more accessible to beginners due to its Python programming model and extensive documentation. However, its performance for neural-network-based face detection is limited by the absence of dedicated vector instructions on the Cortex-M7, resulting in lower inference frame rates. The closed firmware architecture also limits the ability to implement custom real-time control architectures such as the FreeRTOS dual-core task model used in this project.

### 3.4.2 Pixy2

Pixy2 is a dedicated camera sensor designed specifically for object tracking by color signature [18]. It processes frames internally using a proprietary color-based segmentation algorithm and outputs the bounding box of detected objects via SPI, I2C, or UART to a connected microcontroller. Pixy2 can detect multiple color signatures simultaneously and operates at up to 60 frames per second.

While Pixy2 is well-suited for color-based tracking applications and offloads all vision processing from the host microcontroller, it does not support neural-network-based detection, cannot track faces or other semantic categories, and cannot be reprogrammed to implement custom detection algorithms. The tracking system presented in this thesis integrates both color blob detection and face detection within the same microcontroller, without requiring a separate dedicated vision processor.

### 3.4.3 ESP32 with Arduino and Color Tracking

A large body of hobbyist projects implements basic color-based tracking using the ESP32 microcontroller with the Arduino development framework and the ESP32-Camera library. These projects typically capture a JPEG frame, decode it on the CPU, and apply a color threshold to find the largest matching region. While functional for simple demonstrations, these implementations are characteristically limited: they operate in a single-threaded model with no separation between vision and control tasks, they do not support neural-network-based detection, and they suffer from tracking instability due to the lack of smoothing algorithms and deadband logic.

The system presented in this thesis advances significantly beyond this baseline by employing the ESP-IDF framework with FreeRTOS, a dual-core architecture that decouples vision processing from servo control, a custom two-pass grid-based color detection algorithm optimized for noise rejection, and an adaptive exponential smoothing control algorithm that produces stable, oscillation-free motion.

### 3.4.4 Commercial Pan-Tilt Tracking Cameras

Several commercial products implement pan-tilt tracking for specific use cases. Video conferencing cameras from manufacturers such as Logitech (Rally series), Huddly, and Meeting Owl use computer vision algorithms running on embedded processors to automatically frame conference participants. Security cameras from Hikvision and Dahua implement motion-triggered pan-tilt tracking using proprietary embedded software.

These products achieve high reliability and accuracy but are designed for fixed installation, powered from mains electricity, and are not configurable or reprogrammable. Their internal architectures are proprietary and not accessible for research or educational purposes. The system developed in this thesis is fully open, battery-powered, programmable, and serves as a platform for understanding the engineering principles underlying real-time embedded vision and control systems.

---

## 3.5 Servo Control in Robotics

### 3.5.1 Standard PWM Servo Control

Hobby servomotors are among the most widely used actuators in small robotics systems. A servo motor integrates a DC motor, a gearbox, a potentiometer for position feedback, and an internal control circuit into a single compact package. The desired position is communicated to the servo's internal controller by a PWM signal at 50 Hz: the pulse width, ranging from 500 µs (0°) to 2500 µs (180°), encodes the target angle. The internal control circuit implements a closed-loop proportional controller that drives the motor until the potentiometer reading matches the commanded position [19].

### 3.5.2 PID Control for Visual Tracking

The classical approach to visual servo control is the PID (Proportional-Integral-Derivative) controller. The pixel error between the detected target position and the image center is used directly as the control variable. The proportional term produces a servo correction proportional to the current error; the integral term accumulates past error to eliminate steady-state offset; the derivative term damps the response to prevent overshoot.

While PID control is well-suited for systems where the sensor delivers a continuous, high-frequency signal, it presents significant challenges in vision-based tracking systems where the detection rate is limited. When the camera delivers detections at only 12.5 FPS — one measurement every 80 ms — the derivative term is computed over a relatively long and variable time interval, leading to instability. Furthermore, the servo motor's internal proportional controller already constitutes one control loop; placing an outer PID loop on top of it creates a cascaded control problem that is prone to oscillation if the loop gains are not carefully coordinated [20].

### 3.5.3 Alternative Control Approaches

To address the limitations of PID control in low-frame-rate visual tracking, several alternative approaches have been proposed. Feedforward control with model-based target motion prediction uses an estimate of the target's velocity — derived from the difference between successive detected positions — to predict where the target will be at the next control step, allowing the servo to lead the target rather than lag behind it [21].

Exponential smoothing filters, sometimes referred to as first-order IIR (Infinite Impulse Response) filters, provide a simple mechanism for decoupling the update of a reference signal (the detected target position) from the continuous output of a control signal (the servo angle). The output is updated at a high, fixed rate as a weighted average of the previous output and the current target: `output = output × (1 − α) + target × α`. This produces smooth motion independent of the detection rate and can be made adaptive by varying the smoothing factor α based on operating conditions — the approach implemented in this thesis.

Deadband logic prevents unnecessary actuator movement when the tracking error is smaller than a defined threshold, eliminating the mechanical oscillation, noise, and wear that result from continuous micro-corrections in response to sensor noise. This technique is standard practice in industrial servo control and is particularly important in visual tracking systems where detection noise is inherent [22].

---

## References for Chapter 3

*(The following reference numbers are placeholders — verify and complete with actual publication details before submission.)*

[1] — Reference on pan-tilt camera surveillance systems (search: "pan-tilt unit visual tracking survey")

[2] — Reference on robot visual servoing (search: "visual servoing robotics review")

[3] — Reference on drone person tracking (search: "UAV person tracking embedded")

[4] — Reference on color-based tracking limitations (search: "color tracking HSV illumination robustness")

[5] D. S. Bolme, J. R. Beveridge, B. A. Draper, and Y. M. Lui, "Visual object tracking using adaptive correlation filters," in *Proc. IEEE CVPR*, 2010.

[6] J. F. Henriques, R. Caseiro, P. Martins, and J. Batista, "High-speed tracking with kernelized correlation filters," *IEEE Trans. Pattern Anal. Mach. Intell.*, vol. 37, no. 3, pp. 583–596, 2015.

[7] B. D. Lucas and T. Kanade, "An iterative image registration technique with an application to stereo vision," in *Proc. IJCAI*, 1981.

[8] G. Bradski and A. Kaehler, *Learning OpenCV: Computer Vision with the OpenCV Library*. O'Reilly Media, 2008.

[9] — Reference on Raspberry Pi power consumption (search: "Raspberry Pi 4 power consumption measurement")

[10] — Reference on FPGA CNN inference (search: "FPGA convolutional neural network inference real-time")

[11] P. Warden and D. Situnayake, *TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers*. O'Reilly Media, 2019.

[12] — Reference on model quantization for microcontrollers (search: "INT8 quantization microcontroller neural network")

[13] B. Jacob et al., "Quantization and training of neural networks for efficient integer-arithmetic-only inference," in *Proc. IEEE CVPR*, 2018.

[14] G. Hinton, O. Vinyals, and J. Dean, "Distilling the knowledge in a neural network," *arXiv preprint arXiv:1503.02531*, 2015.

[15] — ARM Cortex-M55 with Helium SIMD: ARM Ltd., *Cortex-M55 Processor Technical Reference Manual*.

[16] Espressif Systems. *ESP-DL: Espressif Deep Learning Library*. Available: https://github.com/espressif/esp-dl

[17] OpenMV LLC. *OpenMV Documentation*. Available: https://docs.openmv.io

[18] Charmed Labs. *Pixy2 User Guide*. Available: https://docs.pixycam.com

[19] — Reference on hobby servo PWM control principle (any robotics textbook or servo datasheet)

[20] — Reference on cascaded PID control challenges in visual servoing

[21] — Reference on feedforward prediction in visual tracking

[22] — Reference on deadband in servo control systems (search: "deadband servo control oscillation prevention")
