# Smart Campus Navigator 🧭

**An IoT Edge-to-Cloud Wayfinding Device** *Built for EEC 172 at UC Davis*

The Smart Campus Navigator is a custom hardware and cloud-integrated IoT device designed to guide users to predefined locations across the UC Davis campus. It bridges a mobile web frontend, AWS serverless cloud infrastructure, and a TI CC3200 microcontroller to provide real-time, orientation-aware directional guidance on an OLED display.

---

## 🌟 Features
* **Cloud-Synced GPS:** Fetches the user's live GPS coordinates from an AWS IoT Device Shadow, updated dynamically via a mobile web app and AWS Lambda.
* **Real-Time Orientation:** Utilizes a BNO055 9-DOF IMU polling at 10 Hz to determine the user's physical magnetic heading.
* **Dynamic Wayfinding:** Calculates real-time distance (in miles) and relative bearing to the target destination using flat-earth approximation mathematics.
* **Anti-Flicker UI:** Features a custom selective-redraw algorithm on the SSD1351 OLED display for smooth 10 Hz compass needle updates.
* **IR Remote Menu:** Allows users to select from 10 predefined campus destinations (e.g., the ARC, Kemper Hall, MU) using standard NEC infrared remote codes.

---

## 🛠️ Hardware Requirements
* **Microcontroller:** Texas Instruments CC3200 LaunchPad
* **Display:** Adafruit 1.5" Color OLED (SSD1351)
* **Compass:** Adafruit BNO055 9-DOF Absolute Orientation IMU
* **Input:** 38kHz IR Receiver Diode & Handheld IR Remote
* **Misc:** Breadboard, jumper wires, micro-USB cable

## 💻 Software & Cloud Stack
* **Firmware:** C (Texas Instruments Code Composer Studio / CC3200 SDK)
* **Libraries:** `driverlib`, `simplelink`, custom Adafruit_GFX port
* **Cloud Compute:** AWS Lambda (Python 3, Boto3)
* **Cloud State:** AWS IoT Core (Device Shadows)
* **Frontend:** HTML/JS (Mobile Geolocation API)

---

## 🏗️ System Architecture

1. **Frontend:** A user opens the HTML web app on their phone, which grabs their current GPS coordinates and sends an HTTP POST request.
2. **AWS Lambda:** The Lambda function parses the coordinates and updates the `desired` state of the CC3200's AWS IoT Device Shadow.
3. **CC3200 (Edge):** Every 5 seconds, the microcontroller makes an HTTP GET request to AWS over a secure TLS socket to fetch the updated coordinates. 
4. **Navigation Math:** The board calculates the $\Delta x$ and $\Delta y$ to the target destination, computing the absolute bearing and distance.
5. **Sensor Fusion:** The BNO055 is read over I2C. The absolute target bearing is offset by the user's current physical heading.
6. **OLED Output:** The screen draws the compass interface, pointing the arrow exactly toward the destination relative to how the user is holding the board.

---

## 🔌 Pin Mapping & Wiring Guide

**⚠️ Important:** Ensure your OLED Chip Select (CS) is wired to Pin 58 to prevent SPI/Interrupt collisions with the IR Receiver!

| Component | CC3200 Pin | Function / Notes |
| :--- | :--- | :--- |
| **OLED Display** | | |
| MOSI | Pin 07 | SPI Data Out |
| CLK | Pin 05 | SPI Clock |
| CS (Chip Select)| Pin 58 (GPIO 3) | **MUST be Pin 58** to avoid IR conflict |
| DC (Data/Cmd) | Pin 62 (GPIO 7) | |
| RST (Reset) | Pin 18 (GPIO 28)| |
| **BNO055 IMU** | | |
| SDA | I2C SDA Pin | Standard I2C Data |
| SCL | I2C SCL Pin | Standard I2C Clock |
| **IR Receiver** | | |
| OUT / DATA | Pin 61 (GPIO 6) | Triggers SysTick Interrupt |

---

## 🚀 Setup & Installation

### 1. Cloud Setup (AWS)
1. Create a "Thing" in AWS IoT Core named `Sukhraj_920857647_CC3200`.
2. Deploy the provided Python Lambda function (in the `/cloud` folder) via AWS API Gateway to handle incoming POST requests.
3. Download your AWS root CA, private key, and client certificates.

### 2. Firmware Flashing (UniFlash)
1. Open TI UniFlash and format the CC3200 (1MB capacity).
2. Flash your AWS certificates:
   * `/cert/rootCA.der`
   * `/cert/private.der`
   * `/cert/client.der`
3. Flash the compiled `/sys/mcuimg.bin` (Ensure max size is set to at least 128,000 bytes).

### 3. Usage
1. Power on the CC3200. The OLED will display the boot sequence.
2. Wait for the board to connect to Wi-Fi and the AWS server.
3. **Calibration:** When prompted, wave the board in a "Figure 8" motion to calibrate the magnetometer.
4. **Select Destination:** Point the IR remote at the receiver and press a number key (1-9, 0) to select a campus destination. Press `DEL` for pure compass mode.
5. **Navigate:** Follow the on-screen arrow! Press `ENTER` on the remote to return to the menu at any time.

---

## 👥 Authors
* **Tobias Albano** ([tjalbano@ucdavis.edu](mailto:tjalbano@ucdavis.edu))
* **Sukhraj Johal** ([ssjohal@ucdavis.edu](mailto:ssjohal@ucdavis.edu))

*Developed for EEC 172 at the University of California, Davis (Winter 2026).*
