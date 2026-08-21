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
