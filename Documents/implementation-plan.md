# Implementation Plan
## RC Xbox Controller — ESP32-WROOM

Based on [FSD.md](FSD.md) v1.0

---

## Project Structure

```
rc-xbox-controller/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv                  # OTA partition table
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                      # App entry, task init
│   ├── Kconfig.projbuild           # Wi-Fi SSID/password menu
│   ├── ble_controller.c/.h         # Bluepad32 BLE host
│   ├── motor_control.c/.h          # TB6612FNG Channel A (drive)
│   ├── steering.c/.h               # TB6612FNG Channel B (stepper)
│   ├── led.c/.h                    # Headlight GPIO
│   ├── ota.c/.h                    # OTA update handler
│   ├── wifi.c/.h                   # Wi-Fi STA connection
│   └── webserver.c/.h              # HTTP server for OTA upload
├── components/
│   └── bluepad32/                  # Bluepad32 (git submodule)
└── Documents/
    ├── FSD.md
    └── rc-xbox-controller-project-idea.md
```

---

## Phase 1 — Motor Control & BLE Driving

### Task 1.1: Project Scaffolding
- [ ] Create ESP-IDF project structure (`CMakeLists.txt`, `main/`)
- [ ] Add Bluepad32 as a component (git submodule under `components/`)
- [ ] Configure `sdkconfig.defaults` for ESP32-WROOM (BLE enabled, flash 4MB)
- [ ] Set target to `esp32`, verify blank project builds
- **Depends on:** nothing

### Task 1.2: BLE Controller Integration
- [ ] Implement `ble_controller.c/.h` — init Bluepad32, register callbacks
- [ ] Handle `onConnectedGamepad` / `onDisconnectedGamepad` events
- [ ] Parse Xbox Controller inputs: LT, RT, left thumbstick X-axis, Y button
- [ ] Log raw input values over serial for validation
- **Depends on:** 1.1
- **Validates:** BLE-01, BLE-02, BLE-03

### Task 1.3: Drive Motor Control (TB6612FNG Channel A)
- [ ] Implement `motor_control.c/.h`
- [ ] Configure LEDC PWM on GPIO 27 (PWMA), GPIOs 25/26 (AIN1/AIN2)
- [ ] Configure GPIO 12 (STBY) — pull HIGH on init
- [ ] `motor_set_speed(int16_t speed)` — positive = forward, negative = reverse, 0 = brake
- [ ] Map LT (0–1023) → forward PWM, RT (0–1023) → reverse PWM
- [ ] Add dead-zone threshold (~5% of trigger range) to avoid motor hum at rest
- **Depends on:** 1.1
- **Validates:** MOT-01 through MOT-05

### Task 1.4: Steering Control (TB6612FNG Channel B)
- [ ] Implement `steering.c/.h`
- [ ] Configure LEDC PWM on GPIO 14 (PWMB), GPIOs 32/33 (BIN1/BIN2)
- [ ] `steering_set_position(int16_t x_axis)` — map thumbstick X to stepper direction/speed
- [ ] Add configurable max step count constant to prevent mechanical over-travel
- [ ] Center position (thumbstick = 0) → stop stepper
- **Depends on:** 1.1
- **Validates:** STR-01 through STR-05

### Task 1.5: End-to-End Integration
- [ ] Wire BLE callbacks → motor + steering functions in `main.c`
- [ ] Create `ble_task` (FreeRTOS, priority high) — poll Bluepad32
- [ ] Create `motor_task` (FreeRTOS, priority high) — consume input queue, apply PWM
- [ ] Add BLE disconnect safety: stop motors + stepper on disconnect
- [ ] Flash to ESP32, test with Xbox Controller + motors on bench
- **Depends on:** 1.2, 1.3, 1.4
- **Validates:** BLE-04, full Phase 1

### Phase 1 Milestone
> Xbox Controller connected over BLE drives front+rear DC motors via triggers and steers via thumbstick. Disconnect stops all outputs.

---

## Phase 2 — Headlights & Enhancements

### Task 2.1: Headlight LED Control
- [ ] Implement `led.c/.h`
- [ ] Configure GPIO 4 as output
- [ ] `led_toggle()`, `led_set(bool on)`, `led_off()`
- [ ] Wire Y button press (with debounce) → `led_toggle()`
- [ ] Call `led_off()` on BLE disconnect
- **Depends on:** Phase 1
- **Validates:** LED-01, LED-02, LED-03

### Task 2.2: Wi-Fi Station Connection
- [ ] Implement `wifi.c/.h`
- [ ] Add `Kconfig.projbuild` for SSID/password configuration
- [ ] Connect to Wi-Fi on boot using ESP-IDF `esp_wifi` API
- [ ] Log IP address on successful connection
- **Depends on:** Phase 1
- **Validates:** OTA-03

### Task 2.3: OTA Web Interface
- [ ] Implement `webserver.c/.h` — start `httpd_server` on port 80
- [ ] Serve HTML upload form at `GET /`
- [ ] Handle firmware binary upload at `POST /ota`
- [ ] Implement `ota.c/.h` — write to OTA partition, validate, reboot
- [ ] Create OTA partition table (`partitions.csv`: factory + ota_0 + ota_1)
- **Depends on:** 2.2
- **Validates:** OTA-01, OTA-02

### Task 2.4: Logging
- [ ] Implement structured serial logging for all subsystems
- [ ] Log: BLE state, trigger values, PWM duty, thumbstick X, LED state
- [ ] Use ESP-IDF `ESP_LOG` macros with per-module tags
- **Depends on:** Phase 1
- **Validates:** OTA-04, OTA-05

### Task 2.5: Tuning & Polish
- [ ] Tune dead-zone for trigger center (~5%) and thumbstick center (~10%)
- [ ] Tune max steering angle / step count
- [ ] Verify latency < 50 ms (BLE input → motor output)
- [ ] Stress test: 1+ hour continuous operation
- **Depends on:** 2.1, 2.3, 2.4
- **Validates:** NFR-01, NFR-02, NFR-03

### Phase 2 Milestone
> Full system operational: BLE driving, headlights, Wi-Fi OTA, serial logging. Ready for in-car testing.

---

## GPIO Summary

| GPIO | Function | Direction |
|------|----------|-----------|
| 4    | Front LED | Output |
| 12   | TB6612FNG STBY | Output |
| 14   | PWMB (stepper) | PWM Output |
| 25   | AIN1 (drive dir A) | Output |
| 26   | AIN2 (drive dir B) | Output |
| 27   | PWMA (drive PWM) | PWM Output |
| 32   | BIN1 (steer dir A) | Output |
| 33   | BIN2 (steer dir B) | Output |

---

## Dependencies

| Dependency | Version | Source |
|------------|---------|--------|
| ESP-IDF | v5.4.1 | `~/esp/esp-idf` (installed) |
| Bluepad32 | latest | `components/bluepad32` (git submodule) |

---

## Build & Flash

```bash
get_idf                              # activate ESP-IDF environment
cd /Volumes/Extreme\ SSD/src/rc-xbox-controller
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-* flash monitor
```
