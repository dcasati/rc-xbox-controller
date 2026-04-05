# Functional Specification Document
## RC Xbox Controller — ESP32-WROOM

**Version:** 1.0  
**Date:** April 4, 2026  
**Repository:** https://github.com/dcasati/rc-xbox-controller

---

## 1. Overview

This project replaces the standard RC car controller with an Xbox Controller connected over BLE to an ESP32-WROOM. The ESP32 acts as the BLE host (via [Bluepad32](https://github.com/ricardoquesada/bluepad32)), interprets controller inputs, and drives the car's motors and headlights through a TB6612FNG H-Bridge motor driver.

---

## 2. System Architecture

```
[Xbox Controller]
      |
      | BLE (Bluepad32)
      |
[ESP32-WROOM]
      |
      |--- UART/USB -------> Serial Logging
      |--- Wi-Fi ----------> OTA Web Interface
      |
[TB6612FNG H-Bridge] (VM = 5V)
      |
      |--- Channel A (PWM/DIR) ---> Front + Rear DC Motors (propulsion, wired in parallel)
      |
      |--- Channel B (PWM/DIR) ---> Steering Motor (DC + potentiometer feedback)
      |
      |--- GPIO -------> Front LED (headlights)
```

---

## 3. Hardware Components

| Component | Description |
|---|---|
| ESP32-WROOM | Main microcontroller; BLE host, motor control |
| Xbox Controller | User input device; connects over BLE |
| TB6612FNG H-Bridge | Dual-channel motor driver; VM = 5V |
| Front DC Motor | Propulsion — forward / backward (wired in parallel with rear) |
| Rear DC Motor | Propulsion — forward / backward (wired in parallel with front) |
| Front Steering Servo | Steering — DC motor + potentiometer feedback, driven via TB6612FNG Channel B |
| Front LED | Headlights — on / off |
| USB Cable | Power + serial logging |

---

## 4. Functional Requirements

### 4.1 BLE & Controller Connectivity

| ID | Requirement |
|---|---|
| BLE-01 | The ESP32 shall initialize as a BLE host using the [Bluepad32](https://github.com/ricardoquesada/bluepad32) library. |
| BLE-02 | The ESP32 shall accept a pairing request from an Xbox Controller. |
| BLE-03 | The system shall reconnect automatically to the last paired controller on power-up. |
| BLE-04 | Controller disconnection shall immediately stop all motor outputs and turn off headlights. |

### 4.2 Propulsion (Front + Rear DC Motors)

The front and rear DC motors are wired in parallel on TB6612FNG Channel A. They always run at the same speed and direction. Motor supply voltage is 5V.

| ID | Requirement |
|---|---|
| MOT-01 | The left trigger (LT) shall control forward drive speed; the right trigger (RT) shall control reverse drive speed. |
| MOT-02 | Pressing LT shall drive the motors in the forward direction. |
| MOT-03 | Pressing RT shall drive the motors in reverse. |
| MOT-04 | Motor speed shall be proportional to trigger deflection (0–100% PWM). |
| MOT-05 | Releasing both triggers shall brake the motors (STBY or coast configurable). |

### 4.3 Steering (Front Servo)

| ID | Requirement |
|---|---|
| STR-01 | The left thumbstick X-axis shall control the front steering servo. |
| STR-02 | Deflecting left shall move the servo to the left steering position. |
| STR-03 | Deflecting right shall move the servo to the right steering position. |
| STR-04 | Returning the thumbstick to center shall return the servo to the center position. |
| STR-05 | Servo pulse range (1.0–2.0ms at 50Hz) shall be configurable to prevent mechanical over-travel. |

### 4.4 Headlights (Front LED)

| ID | Requirement |
|---|---|
| LED-01 | Pressing the **Y button** on the Xbox Controller shall toggle the front LED on and off. |
| LED-02 | The LED state shall persist across steering and speed changes. |
| LED-03 | On BLE disconnect, the LED shall turn off. |

### 4.5 OTA & Logging

| ID | Requirement |
|---|---|
| OTA-01 | The firmware shall support OTA updates over Wi-Fi using the ESP-IDF OTA partition scheme. |
| OTA-02 | A basic web interface (HTTP server on the ESP32) shall allow the user to upload a new firmware binary. |
| OTA-03 | The ESP32 shall connect to a configured Wi-Fi network on boot (SSID/password stored in NVS or sdkconfig). |
| OTA-04 | Serial logging shall be available over USB at 115200 baud. |
| OTA-05 | Log output shall include BLE connection state, controller input values, motor PWM duty cycles, and LED state. |

---

## 5. Xbox Controller Button Mapping

| Xbox Input | Function |
|---|---|
| Left Trigger (LT) | Drive forward (proportional) |
| Right Trigger (RT) | Drive backward (proportional) |
| Left Thumbstick X+ | Steer right |
| Left Thumbstick X− | Steer left |
| Y Button | Toggle headlights |

> Additional mappings (e.g., bumpers for turbo, Menu for OTA trigger) to be defined in Phase 2.

---

## 6. Pin Mapping (Verified)

| Signal | ESP32 Pin | ESP32-WROOM-32 Module Pin | TB6612FNG Pin | Function |
|---|---|---|---|---|
| AIN1 | GPIO 25 | IO25 (pin 10) | pin 14 | Drive motors direction A |
| AIN2 | GPIO 33 | IO33 (pin 9) | pin 15 | Drive motors direction B |
| PWMA | GPIO 32 | IO32 (pin 8) | pin 16 | Drive motors PWM |
| STBY | GPIO 26 | IO26 (pin 11) | — | H-Bridge standby (HIGH = enabled) |
| BIN1 | GPIO 27 | IO27 (pin 12) | pin 12 | Steering motor direction A |
| BIN2 | GPIO 14 | IO14 (pin 13) | pin 11 | Steering motor direction B |
| PWMB | GPIO 12 | IO12 (pin 14) | pin 10 | Steering motor PWM |
| POT | GPIO 34 | IO34 (pin 6) | — | Steering potentiometer wiper (ADC input) |
| LED | GPIO 13 | IO13 (pin 16) | — | Front LED headlight |
| VDD | 3.3V | VDD (pin 2) | VCC (pin 2) | Logic power |
| GND | GND | GND (pin 1) | GND (pins 3,8,9) | Common ground |
| VM | — | — | VM (pin 1) | Motor power: 5V (regulated from battery) |

### TB6612FNG Motor Outputs

| TB6612FNG Pin | Connected to | Notes |
|---|---|---|
| AO1 (pin 4) | Drive motor wire 1 | Front + rear DC motors wired in parallel |
| AO2 (pin 5) | Drive motor wire 2 | Swap AO1/AO2 if direction is reversed |
| BO1 (pin 7) | Steering servo RED wire | Motor + |
| BO2 (pin 6) | Steering servo BLACK wire | Motor − |

### 5-Wire Steering Servo

| Wire Color | Connect to | Function |
|---|---|---|
| Red | TB6612FNG BO1 (pin 7) | Motor + |
| Black | TB6612FNG BO2 (pin 6) | Motor − |
| White | ESP32 3.3V | Potentiometer VCC |
| Yellow | ESP32 GPIO 34 | Potentiometer wiper (ADC) |
| Brown | ESP32 GND | Potentiometer GND |

### Additional Components

| Component | Connection | Notes |
|---|---|---|
| 100nF ceramic cap | GPIO 34 ↔ GND | Required: filters motor EMI on ADC |
| 220Ω resistor | GPIO 13 → LED → GND | Current limiting for headlight LED |
| 7.4V Li-ion battery | VM + GND on TB6612FNG | 2S 18650, 1500mAh (regulated to 5V for VM) |

---

## 7. Software Architecture

### 7.1 Firmware Components

| Component | Responsibility |
|---|---|
| `main.c` | App entry point; task initialization |
| `ble_controller.c/.h` | Bluepad32 integration; BLE event handling |
| `motor_control.c/.h` | TB6612FNG PWM/direction control for both motors |
| `steering.c/.h` | DC motor + potentiometer closed-loop position control |
| `led.c/.h` | Headlight GPIO control |
| `ota.c/.h` | OTA update handler |
| `wifi.c/.h` | Wi-Fi station connection |
| `webserver.c/.h` | HTTP server for OTA upload page |

### 7.2 FreeRTOS Tasks

| Task | Priority | Description |
|---|---|---|
| `ble_task` | High | Polls Bluepad2 for controller events |
| `motor_task` | High | Applies PWM updates from BLE input queue |
| `ota_task` | Low | HTTP server for OTA firmware upload |
| `log_task` | Low | Periodic status logging over UART |

### 7.3 Framework & Libraries

| Tool / Library | Purpose |
|---|---|
| ESP-IDF v5.4.x | Base SDK, FreeRTOS, Wi-Fi, OTA, BLE |
| [Bluepad32](https://github.com/ricardoquesada/bluepad32) | Xbox Controller BLE host library |
| ESP32-Workbench | Flashing, testing, serial monitoring |

---

## 8. Implementation Phases

### Phase 1 — Motor Control & BLE Driving
- [ ] Initialize Bluepad2 and pair with Xbox Controller
- [ ] Implement rear DC motor forward/backward via TB6612FNG
- [ ] Implement front stepper motor left/right steering
- [ ] Map thumbstick axes to motor PWM
- [ ] Validate over BLE end-to-end

### Phase 2 — Headlights & Enhancements
- [ ] Implement LED toggle on Y button press
- [ ] Add BLE disconnect safety stop
- [ ] Add OTA update support
- [ ] Serial logging for all subsystems
- [ ] Tuning: dead-zone on thumbstick center, max steering angle

---

## 9. Non-Functional Requirements

| ID | Requirement |
|---|---|
| NFR-01 | BLE input-to-motor latency shall be < 50 ms. |
| NFR-02 | Firmware shall fit within 2 MB flash (single OTA partition scheme). |
| NFR-03 | System shall be stable for continuous operation > 1 hour without reboot. |
| NFR-04 | All source code shall be hosted at https://github.com/dcasati/rc-xbox-controller. |

---

## 10. Resolved Questions

| # | Question | Answer |
|---|---|---|
| 1 | Is the front motor a true stepper (step/dir) or a DC motor used for steering? | It is a servo. Direct PWM from ESP32 GPIO (no H-Bridge needed). Two DC motors (front + rear) for propulsion. |
| 2 | What is the supply voltage for the motors? | Measured 4.6–5V (within TB6612FNG range). |
| 3 | Should OTA be triggered automatically or via a dedicated button press? | Basic web interface served by the ESP32 over Wi-Fi. |
| 4 | Is Wi-Fi available in the RC car environment? | Yes. Wi-Fi OTA will be used. |
