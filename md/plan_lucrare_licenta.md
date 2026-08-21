# Plan Lucrare de Licență
## „Sistem Embedded de Urmărire în Timp Real bazat pe Edge AI"

> **Notă:** Acest plan este adaptat exact pe proiectul tău — ESP32S3 Sense (Xiao), camera OV2640, 2 servomotoare pe LEDC, FreeRTOS dual-core, detecție față (ESP-DL) și detecție blob de culoare HSV.

---

## Structura Generală a Lucrării

| Nr. | Capitol | Lungime estimată |
|-----|---------|-----------------|
| — | Pagina de titlu, declarație, cuprins, lista figurilor | 5–8 pagini |
| — | Rezumat (RO + EN) | 1 pagină |
| 1 | Introducere și stadiul artei | 8–12 pagini |
| 2 | Arhitectura hardware | 10–14 pagini |
| 3 | Arhitectura software și RTOS | 12–16 pagini |
| 4 | Computer Vision și Edge AI | 10–14 pagini |
| 5 | Algoritmii de control | 10–14 pagini |
| 6 | Rezultate experimentale și analiză | 8–12 pagini |
| 7 | Concluzii și direcții viitoare | 4–6 pagini |
| — | Bibliografie | 2–4 pagini |
| — | Anexe (cod sursă relevant) | opțional |
| **TOTAL** | | **~65–90 pagini** |

---

## Pagini de Început (obligatorii)

- **Pagina de titlu** — titlul lucrării, numele tău, coordonatorul, universitatea, facultatea, anul
- **Declarație de autenticitate** — că lucrarea îți aparține, semnată
- **Cuprins** — generat automat în Word
- **Lista figurilor și tabelelor**
- **Lista abrevierilor** — ESP32, RTOS, FreeRTOS, LEDC, HSV, RGB565, FOV, PWM, ISR, DL, CNN, IPC, PSRAM...

---

## Rezumat (Abstract)

**~250 cuvinte, în română ȘI engleză.**

Să conțină:
1. **Problema:** Urmărirea în timp real a obiectelor necesită putere de calcul mare — dificil pe microcontrolere
2. **Soluția propusă:** Sistem embedded pe ESP32S3 cu FreeRTOS, dual-core, cu două moduri de urmărire (față și blob de culoare)
3. **Contribuțiile tehnice:** algoritm de mapare geometrică, deadband, smoothing adaptiv, ISR cu debounce
4. **Rezultatele:** sistem funcțional care urmărește în timp real la ~12 FPS, stabil, fără oscilații

---

## Capitol 1: Introducere și Stadiul Artei

### 1.1 Motivația lucrării
- De ce tracking local (edge) este mai bun decât cloud?
  - Latență: local sub 100ms vs. cloud 300–500ms
  - Confidențialitate: imaginile nu pleacă pe internet
  - Cost: nu ai nevoie de server sau conexiune internet
- Aplicații reale: roboti de supraveghere, sisteme de asistare, drone, camere de securitate smart

### 1.2 Stadiul Artei (State of the Art)
- **Sisteme tradiționale:** Raspberry Pi + OpenCV (mai putere, mai consum)
- **TinyML și Edge AI:** tendința de a muta inferența AI pe dispozitive mici
- **Microcontrolere cu acceleratoare vectoriale:** ESP32S3 cu instrucțiuni SIMD (PIE/DSP)
- **Biblioteci Edge AI:** ESP-DL vs. TensorFlow Lite Micro vs. Edge Impulse
- Citează 2–3 lucrări academice despre TinyML sau tracking pe embedded

### 1.3 Obiectivele lucrării
- Construirea unui sistem de urmărire 2D (pan + tilt) în timp real
- Folosirea unui microcontroler de clasă low-end (fără OS, fără Linux)
- Implementarea a două moduri de urmărire: față (AI) și culoare (computer vision clasic)
- Comutare prin întrerupere hardware (buton fizic)
- Mișcare fluidă a servomotoarelor prin interpolare liniară (lerp) cu smoothing adaptiv

---

## Capitol 2: Arhitectura Hardware

### 2.1 Microcontrolerul ESP32S3 (Seeed Xiao ESP32S3 Sense)
- **Procesor:** Xtensa LX7 dual-core, 240 MHz
- **Memorie:** 512KB SRAM intern + 8MB PSRAM extern (crucial pentru frame buffers!)
- **Instrucțiuni vectoriale:** PIE (Processor Instruction Extensions) — cum le folosește ESP-DL
- **Periferie relevantă:** LEDC (PWM), GPIO cu ISR, I2C/SPI, DVP camera interface
- Include un tabel cu specificațiile tehnice cheie
- Explică de ce ai ales acest modul față de alternativele (Arduino Nano, STM32, Raspberry Pi Zero)

### 2.2 Modulul Camera (OV2640)
- Interfața DVP (Digital Video Port) cu 8 linii de date paralele (Y2–Y9)
- Semnale de sincronizare: VSYNC, HREF, PCLK, XCLK
- Formatul de ieșire ales: **RGB565** — de ce? (nativ pentru ESP-DL, nu necesită conversie)
- Rezoluția de lucru: **240×240 pixeli** — compromis între viteză și acuratețe
- Frame buffer în PSRAM (`CAMERA_FB_IN_PSRAM`) — de ce e necesar (mărimea unui frame: 240×240×2 = 115200 bytes)
- `GRAB_MODE: CAMERA_GRAB_WHEN_EMPTY` — explică ce face și de ce previne buffering-ul vechi

### 2.3 Servomotoarele
- Tip: servomotor hobby standard (180°), control prin semnal PWM la 50Hz
- **Formula impuls:** `pulse_width = 500µs + (angle/180) * 2000µs`
- **Formula duty cycle LEDC:** `duty = (pulse_width * (2^13 - 1)) / 20000`
- Pinii de control: GPIO 3 (axa Y/tilt) și GPIO 4 (axa X/pan)
- Limitele fizice ale sistemului: axa Y limitată la [60°, 120°] pentru a nu deteriora mecanismul
- Include un diagrama de conectare (poți face o schiță simplă)

### 2.4 Interfața utilizator (Buton fizic)
- Buton pe GPIO 1, cu pull-up intern
- Comutare mod prin **întrerupere hardware** pe front descendent (NEGEDGE)
- Software debounce în ISR: `last_isr_time` cu fereastră de 200ms

### 2.5 Schema de alimentare
- Separarea alimentării servomotoarelor față de ESP32 (curenți de vârf de 600mA pot cauza brownout)
- Decuplaj capacitor pe liniile de alimentare

---

## Capitol 3: Arhitectura Software și Proiectarea RTOS

### 3.1 ESP-IDF și FreeRTOS
- Ecosistemul ESP-IDF: de ce față de Arduino? (Control mai fin, acces direct la periferice, suport oficial Espressif)
- FreeRTOS: scurtă prezentare — scheduler preemptiv, priorități, task-uri
- **Core Affinity (afinitate core):** `xTaskCreatePinnedToCore` — de ce e critic
  - Core 0: `servo_task` — sensibil la latență, trebuie să ruleze la exact 50Hz
  - Core 1: `vision_task` — computațional intensiv, rulează inferența AI

### 3.2 Arhitectura dual-core

```
┌─────────────────────────────────────────────────────┐
│                   ESP32S3                           │
│                                                     │
│  ┌──────────────────┐    ┌──────────────────────┐  │
│  │     CORE 0       │    │       CORE 1         │  │
│  │   servo_task     │    │    vision_task        │  │
│  │   50Hz (20ms)    │◄───│   ~12 FPS (~80ms)    │  │
│  │   LEDC PWM       │    │   Camera + AI/CV      │  │
│  └──────────────────┘    └──────────────────────┘  │
│          ▲                        │                 │
│          │    FreeRTOS Queue      │                 │
│          └─────── (1 slot) ───────┘                 │
│                face_coords_t                        │
└─────────────────────────────────────────────────────┘
```

### 3.3 Comunicarea Inter-Core (IPC) prin FreeRTOS Queue
- `xQueueCreate(1, sizeof(face_coords_t))` — de ce o singură poziție? (vrem mereu cel mai proaspăt cadru)
- `xQueueSend(servo_queue, &coords, 0)` — non-blocking (dacă coada e plină, aruncă datele vechi)
- `xQueueReceive(servo_queue, &coords, 0)` — drenarea cozii pentru a prelua ultimele coordonate
- Explică cum se evită **race conditions** (coada FreeRTOS este thread-safe)
- Variabila `current_mode` este `volatile` — de ce? (ISR rulează pe alt context decât task-urile)

### 3.4 Întreruperea Hardware (ISR)
- `IRAM_ATTR` — de ce codul ISR trebuie să fie în RAM intern, nu în Flash
- Debounce software în ISR cu timer (nu delay-uri!)
- Comutarea atomică a modului de urmărire

### 3.5 Diagrama fluxului de date

```
[Camera] → [vision_task C1] → [Queue] → [servo_task C0] → [PWM Servos]
                ↑                                ↑
           [ESP-DL Model]              [smoothing + deadband]
           [HSV Blob Detect]
                ↑
           [Button ISR] ← [GPIO 1]
```

---

## Capitol 4: Computer Vision și Edge AI

### 4.1 Detecția facială cu ESP-DL
- **HumanFaceDetect** — modelul CNN din biblioteca esp-who
- Rulează pe instrucțiunile vectoriale ale ESP32S3
- Input: frame RGB565, 240×240
- Output: listă de `dl::detect::result_t` cu bounding box `[x1, y1, x2, y2]`
- Calculul centrului feței: `cx = (box[0] + box[2]) / 2`, `cy = (box[1] + box[3]) / 2`
- Frecvența de inferență: ~12 FPS (80ms per frame)

### 4.2 Detecția Blob de Culoare (HSV)
Acesta este algoritmul tău propriu — descrie-l în detaliu!

#### 4.2.1 Conversia RGB565 → RGB → HSV
- Extragerea canalelor din pixel RGB565 (big-endian):
  ```
  pixel[i]   = byte MSB
  pixel[i+1] = byte LSB
  R = (pixel >> 8) & 0xF8
  G = (pixel >> 3) & 0xFC
  B = (pixel << 3) & 0xF8
  ```
- Conversia RGB → HSV (formulele complete cu H, S, V)
- Pragurile pentru albastru: `H ∈ (200°, 255°)`, `S > 0.4`, `V > 0.2`

#### 4.2.2 Algoritmul Grid-Based (2 Pase)
- **Motivație:** scanarea pixel cu pixel ar fi lentă și sensibilă la zgomot
- **Pasul 1 — Grid 15×15 cu celule de 16×16 pixeli:**
  - Numără pixelii de culoare per celulă
  - Găsește celula cu concentrația maximă
  - Prag minim: ≥ 25 pixeli per celulă (filtrează artefacte)
- **Pasul 2 — Calculul precis al centrului:**
  - Caută doar în fereastra 3×3 de celule în jurul celulei maxime
  - Calculează centrodul ponderat (media aritmetică a coordonatelor)
  - Prag secundar: ≥ 40 pixeli total (blob valid)

#### 4.2.3 Comparație față-culoare
| Criteriu | Detecție față (CNN) | Detecție culoare (HSV) |
|---------|--------------------|-----------------------|
| FPS | ~12 | ~25 |
| Consum CPU | ridicat | redus |
| Robustețe | ridicată (iluminare variabilă) | medie (sensibilă la lumină) |
| Flexibilitate | orice față | un singur interval HSV |

---

## Capitol 5: Algoritmii de Control

> Acesta este **cel mai important capitol tehnic** din lucrare!

### 5.1 De ce nu PID standard?
- Controlerele PID clasice presupun un semnal de eroare **continuu** la frecvență mare
- Camera livrează date la ~12 FPS (una la 80ms), servomotorul rulează la 50Hz (una la 20ms)
- **Problema:** un PID direct ar provoca oscilații — primește 4 update-uri servo la 0 eroare, apoi un salt brusc
- Soluția: decuplarea frecvenței de urmărire de cea de control

### 5.2 Maparea Geometrică Directă (Pixel → Unghi)
- **Baza:** camera are un FOV cunoscut → fiecare pixel corespunde unui unghi fix
- `degrees_per_pixel_x = FOV_H / CAM_WIDTH = 40° / 240 = 0.167°/pixel`
- `degrees_per_pixel_y = FOV_V / CAM_HEIGHT = 40° / 240 = 0.167°/pixel`
- **Formula de calcul unghi țintă:**
  ```
  err_x = target_x - CAM_MID_X   (eroarea în pixeli față de centru)
  target_angle_x = current_angle_x - err_x * degrees_per_pixel_x
  target_angle_y = current_angle_y + err_y * degrees_per_pixel_y
  ```
- Notă: semnul minus pe X și plus pe Y — explică de ce (oglindire/orientare fizică)

### 5.3 Smoothing Adaptiv prin Interpolare Exponențială (LERP)
- Decuplarea mișcării servo de frecvența de detecție
- **Formula:**
  ```
  current_angle = current_angle * (1 - α) + target_angle * α
  ```
  unde `α` este factorul de smoothing
- **Smoothing adaptiv bazat pe mod:**
  - `MODE_FACE: α = 0.13` — mișcare mai rapidă, feței i se permite mai multă variație
  - `MODE_COLOR: α = 0.07` — mișcare mai lentă, blob-ul de culoare e mai zgomotos
- Execuție la **50Hz** (la fiecare 20ms) indiferent de actualizările de viziune
- Grafic: comportamentul smoothing cu α mic vs. α mare (opțional, dar util)

### 5.4 Deadband — Prevenirea Oscilațiilor
- **Problema:** când obiectul este centrat, există o eroare mică dar nenulă din cauza zgomotului
- Fără deadband: servo-ul vibrează continuu în jurul centrului → consum, uzură, zgomot
- **Implementarea deadband-ului:**
  ```cpp
  if (err_x > 15 || err_x < -15)    // ±15 pixeli deadband
      target_angle_x = ...;
  ```
- Alegerea valorii de 15 pixeli: justificare experimentală
- Efectul: servo-ul „blochează" poziția când obiectul e aproape de centru

### 5.5 Limitele Fizice de Siguranță (Clipping)
- Axa X: `[0°, 180°]` — gama completă a servomotorului
- Axa Y: `[60°, 120°]` — mecanism limitat fizic (previne coliziunea cu structura)
- Implementat prin clipping simplu după calculul unghiului țintă

---

## Capitol 6: Rezultate Experimentale și Analiză

### 6.1 Mediul de testare
- Descrierea setup-ului fizic: suportul pan-tilt, camera, cablajul
- Condițiile de iluminare pentru teste
- Subiectul de urmărit (față umană, obiect albastru)

### 6.2 Metrici de performanță

#### Tabel de performanță
| Metric | Valoare |
|--------|---------|
| Frecvența de inferență AI (față) | ~12 FPS |
| Frecvența de control servo | 50 Hz |
| Latența sistem (detectare → mișcare) | ~80–100 ms |
| Rezoluția camerei | 240×240 px |
| Consum memorie frame buffer | 115 KB (PSRAM) |
| Priorități task-uri FreeRTOS | 5 (ambele) |

#### 6.2.1 Utilizarea memoriei
- Heap liber, PSRAM ocupat, stack-ul task-urilor
- Captură din monitorul serial ESP-IDF

#### 6.2.2 Latența de urmărire
- Cum ai măsurat? (marcare temporală cu `esp_timer_get_time()`)
- Grafic timp de răspuns al servomotorului

### 6.3 Comportamentul sistemului

#### 6.3.1 Urmărire față — analiză calitativă
- Stabilitate în condiții de iluminare normală
- Comportament la pierderea țintei (servo rămâne pe ultima poziție)
- Comportament la ținte multiple (preia prima față detectată)

#### 6.3.2 Urmărire culoare — analiză calitativă
- Robustețea la variații de iluminare
- Comparație cu/fără deadband (oscilații eliminate)
- Comparație cu/fără grid-based detection (zgomot redus)

#### 6.3.3 Comutarea modurilor
- Timpul de comutare după apăsarea butonului
- Comportamentul servo la tranziție

### 6.4 Discuții și limitări
- Performanța degradată la iluminare slabă
- Sensibilitatea algoritmului HSV la umbrele albastre din decor
- Limitarea mecanică pe axa Y (±30° față de centru)

---

## Capitol 7: Concluzii și Direcții Viitoare

### 7.1 Rezumatul contribuțiilor
- Ai implementat un sistem embedded complet de urmărire 2D în timp real
- Arhitectura dual-core cu FreeRTOS pentru decuplarea viziunii de control
- Doi algoritmi de urmărire: CNN (ESP-DL) și Computer Vision clasic (HSV + grid)
- Algoritm de control cu smoothing adaptiv și deadband, fără PID clasic
- Comutare între moduri prin întrerupere hardware cu debounce software

### 7.2 Direcții viitoare
- **Model AI mai rapid:** explorarea unor modele mai ușoare sau cuantizate (INT8)
- **Calibrare automată a culorii:** ajustarea pragurilor HSV în timp real prin buton sau interfață web
- **Feedback din encoder:** adăugarea de encodere pe servo pentru control în buclă închisă adevărată
- **Cameră mai bună:** OV5640 pentru rezoluție mai mare și FOV mai precis
- **Tracking cu predicție (Kalman Filter):** estimarea poziției viitoare pentru latențe mai mici
- **Interfață web:** streaming video cu overlay al bounding box-ului

---

## Bibliografie (sugestii)

### Documentație oficială
1. Espressif Systems. *ESP-IDF Programming Guide* (versiunea utilizată în proiect). https://docs.espressif.com/projects/esp-idf
2. Espressif Systems. *ESP-DL User Guide*. https://github.com/espressif/esp-dl
3. Espressif Systems. *ESP-WHO Vision AI Demos*. https://github.com/espressif/esp-who
4. FreeRTOS. *FreeRTOS Reference Manual*. https://www.freertos.org/Documentation/RTOS_book.html
5. Seeed Studio. *XIAO ESP32S3 Sense Wiki*. https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

### Lucrări academice (caută pe Google Scholar)
6. Warden, P., & Situnayake, D. (2019). *TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers*. O'Reilly Media.
7. Catuneanu, A. et al. — articole despre Edge AI / TinyML pe microcontrolere (caută „edge inference microcontroller")
8. Lucrări despre algoritmi de blob detection și tracking în viziune artificială

### Datasheet-uri
9. OmniVision Technologies. *OV2640 Camera Module Datasheet*
10. Espressif Systems. *ESP32-S3 Technical Reference Manual*

---

## Anexe (opțional, dar util)

### Anexa A: Codul sursă complet `main.cpp`
- Prezintă codul cu comentarii
- Evidențiază secțiunile cheie cu referințe la capitolele din lucrare

### Anexa B: Schema electrică
- Conectarea camerei la ESP32S3 (pinout complet)
- Conectarea servomotoarelor (GPIO 3, 4)
- Butonul pe GPIO 1

### Anexa C: Configurarea proiectului ESP-IDF
- `sdkconfig.defaults` — setările importante
- `partitions.csv` — schema de partiționare flash
- `idf_component.yml` — dependențele proiectului

---

## Sfaturi Generale pentru Redactare

### Formatare Word (standard academic Romanian)
- Font: **Times New Roman 12pt** sau **Arial 11pt**
- Spațiere: **1.5** sau **2 rânduri**
- Margini: 2.5cm stânga, 2cm dreapta/sus/jos (sau conform cerințelor facultății)
- Numerotare pagini: din pagina 1 a introducerii (paginile de titlu cu cifre romane sau fără)
- Capitolele încep pe pagină nouă

### Figuri și Diagrame
- Fiecare figură trebuie să aibă **număr și titlu** sub ea: `Figura 3.1: Arhitectura dual-core`
- Referință în text **înainte** de figură: „...după cum se vede în Figura 3.1..."
- Fă capturi de ecran din monitorul serial cu log-urile ESP
- Fă o fotografie a hardware-ului fizic pentru Capitolul 2

### Cod sursă în lucrare
- Folosește **font monospațiat** (Courier New 10pt sau Consolas 10pt)
- Nu pune tot codul în text — doar fragmentele **relevante și explicate**
- Codul complet merge în Anexe

### Citări
- Stil **APA** sau **IEEE** — verifică ce cere facultatea ta
- Cite-ază **fiecare** cifră, afirmație tehnică și comparație care nu e contribuția ta
