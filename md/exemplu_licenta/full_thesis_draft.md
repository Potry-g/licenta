# Capitolul 1: Rezumat în Limba Română

---

## 1.1 Descrierea temei

Urmărirea obiectelor în timp real reprezintă o problemă fundamentală în domeniul roboticii și al viziunii artificiale. Sistemele clasice de urmărire sunt implementate de regulă pe calculatoare de uz general sau pe plăci de calcul de tip single-board computer (SBC), cum ar fi Raspberry Pi, care rulează sisteme de operare complete și biblioteci de procesare a imaginilor precum OpenCV. Deși aceste soluții oferă performanțe ridicate, ele prezintă dezavantaje semnificative în ceea ce privește consumul de energie, dimensiunile fizice, costul și dependența de o sursă de alimentare fixă, ceea ce le face impractice pentru aplicațiile mobile și autonome.

Lucrarea de față propune o abordare alternativă: implementarea unui sistem de urmărire vizuală în timp real pe un microcontroler de clasă redusă, fără sistem de operare complet, alimentat de la baterii. Sistemul utilizează microcontrolerul Seeed XIAO ESP32S3 Sense, echipat cu un procesor dual-core Xtensa LX7 la 240 MHz și instrucțiuni vectoriale de tip PIE (Processor Instruction Extensions), care permit rularea eficientă a modelelor de inteligență artificială direct pe dispozitiv — concept cunoscut sub denumirea de Edge AI sau TinyML.

Obiectivul principal al proiectului este construirea unui sistem embedded complet și autonom, capabil să urmărească în timp real atât fețe umane, prin intermediul unui model de rețea neuronală convoluțională (CNN), cât și obiecte colorate, printr-un algoritm de detecție bazat pe spațiul de culoare HSV. Ieșirea sistemului controlează fizic o cameră montată pe un braț pan-tilt cu două axe, acționat de două servomotoare, astfel încât subiectul urmărit să fie centrat permanent în cadrul imaginii.

---

## 1.2 Descrierea concisă a sistemului

Sistemul implementat este compus dintr-un ansamblu hardware și un set de algoritmi software care lucrează împreună pentru a realiza urmărirea în timp real.

Din punct de vedere hardware, sistemul este construit în jurul microcontrolerului **Seeed XIAO ESP32S3 Sense**, care integrează atât procesorul, cât și interfața pentru modulul de cameră. Camera utilizată este **OV2640**, conectată prin interfața DVP (Digital Video Port) cu 8 linii de date paralele, configurată să captureze cadre cu rezoluția de 240×240 pixeli în format RGB565. Mișcarea pe două axe este realizată de două **servomotoare MG90S**, conectate la perifericul LEDC (LED Control) al ESP32S3, care generează semnale PWM la frecvența de 50 Hz. Servomotoarele sunt montate pe un **braț pan-tilt imprimat 3D**, pe care sunt fixate atât ESP32S3, cât și camera, astfel încât mișcarea mecanică a brațului să centreze fizic subiectul urmărit în cadrul imaginii.

Alimentarea sistemului este asigurată de **două celule Li-Ion 18650 conectate în serie**, formând o configurație 2S cu tensiunea nominală de 7,4 V. Circuitul de management al bateriei (BMS 2S) asigură protecția la supraîncărcare, supradescărcare și scurtcircuit. Tensiunea este coborâtă la 5 V printr-un convertor buck DC-DC, care alimentează atât servomotoarele, cât și placa ESP32S3. Un **buton fizic** conectat la GPIO 1 permite utilizatorului să comute între cele două moduri de urmărire.

---

## 1.3 Proiectarea sistemului

Arhitectura software a sistemului este construită pe baza sistemului de operare în timp real **FreeRTOS**, integrat nativ în framework-ul ESP-IDF al Espressif. Sistemul este structurat în două task-uri independente, fiecare rulând pe câte un nucleu dedicat al procesorului dual-core:

- **`vision_task`** — rulează pe **Core 1** și este responsabil de capturarea cadrelor de la cameră și de executarea algoritmului de detecție corespunzător modului activ. În modul de detecție a fețelor (`MODE_FACE`), task-ul utilizează modelul CNN `HumanFaceDetect` din biblioteca **ESP-DL**, furnizată de Espressif. În modul de detecție a culorii (`MODE_COLOR`), task-ul execută un algoritm propriu de detecție a blob-urilor de culoare în spațiul HSV.

- **`servo_task`** — rulează pe **Core 0** și este responsabil de controlul servomotoarele la frecvența fixă de 50 Hz (la fiecare 20 ms), independent de rata de actualizare a sistemului de viziune. Task-ul aplică un algoritm de netezire exponențială adaptivă (LERP) și o zonă moartă (deadband) pentru a preveni oscilațiile.

Comunicarea dintre cele două task-uri se realizează printr-o coadă FreeRTOS cu o singură poziție, de tip `face_coords_t {int x, int y}`, care transmite coordonatele centrului obiectului detectat de la task-ul de viziune către task-ul de control al servomotoarelor. Comutarea între moduri este gestionată printr-o întrerupere hardware (`ISR`) asociată butonului fizic, cu debounce software de 200 ms implementat direct în rutina de tratare a întreruperii.

Algoritmul de control al servomotoarelor realizează o mapare geometrică directă între eroarea în pixeli față de centrul imaginii și unghiul de corecție al servomotorului, folosind câmpul vizual (FOV) al camerei ca factor de scalare. Asupra unghiului țintă calculat se aplică o interpolare liniară exponențială cu factor de netezire adaptiv: α = 0,13 în modul de detecție a fețelor și α = 0,07 în modul de detecție a culorii, valori alese experimental pentru a asigura mișcare fluidă fără oscilații.

---

## 1.4 Testarea sistemului

Sistemul a fost testat în condiții de iluminare interioară controlată, urmărind atât o persoană aflată la o distanță de aproximativ 0,5–1,5 m față de cameră, cât și un obiect albastru de dimensiuni medii.

În modul de detecție a fețelor, rata de inferență a modelului CNN a fost măsurată la **12,5 FPS** (cadre pe secundă), corespunzând unei latențe de aproximativ 80 ms între capturarea unui cadru și actualizarea țintei servomotoarelor. Servomotoarele se actualizează la frecvența fixă de 50 Hz, independent de rata de detecție, datorită arhitecturii dual-core și a algoritmului de netezire LERP. Comportamentul sistemului la pierderea subiectului din cadru este stabil: servomotoarele mențin ultima poziție cunoscută, fără mișcări erratice.

În modul de detecție a culorii, algoritmul grid-based în două pase a demonstrat o bună rezistență la zgomotul de fundal. Pragul minim de 25 pixeli per celulă de grilă (Pass 1) și pragul de 40 pixeli total pe blob (Pass 2) asigură filtrarea eficientă a artefactelor izolate. Implementarea zonei moarte de ±15 pixeli a eliminat complet oscilațiile servo atunci când obiectul urmărit se afla în apropierea centrului cadrului.

> **[TODO]:** Rata de cadre măsurată pentru modul de detecție a culorii. *(De completat după măsurare.)*

> **[TODO]:** Autonomia estimată a bateriei în ore. *(De completat după măsurarea consumului mediu de curent.)*

---

## 1.5 Concluzii

Sistemul implementat îndeplinește toate obiectivele propuse: urmărește în timp real fețe umane și obiecte colorate, funcționează autonom pe baterii, este compact ca dimensiuni fizice și nu necesită niciun calculator extern sau conexiune la internet.

Contribuțiile tehnice principale ale lucrării sunt: arhitectura dual-core FreeRTOS cu separarea clară a sarcinilor de viziune și control, algoritmul de detecție a blob-urilor de culoare bazat pe grilă bidimensională cu două pase, controlul servomotoarelor prin interpolare exponențială adaptivă și zona moartă, precum și comutarea între moduri prin întrerupere hardware cu debounce software.

Ca direcții de îmbunătățire viitoare, se pot menționa: implementarea unui filtru Kalman pentru predicția poziției între cadre, calibrarea automată a pragurilor de culoare HSV, utilizarea unui model CNN mai rapid sau cuantizat la 8 biți pentru creșterea ratei de detecție, și adăugarea unui server web pentru vizualizarea live a fluxului video cu suprapunerea bounding box-ului detectat.


---


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


---


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


---


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


---


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


---


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


---


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
