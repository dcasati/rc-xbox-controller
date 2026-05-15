// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (TB6612FNG Channel B + potentiometer)

#ifndef STEERING_H
#define STEERING_H

#include <stdint.h>

// Initialize TB6612FNG Channel B GPIOs, LEDC PWM, and ADC for the potentiometer.
// NOTE: motor_control_init() must be called first — it holds STBY (GPIO 26) HIGH.
void steering_init(void);

// Drive steering motor toward the position encoded by thumbstick X-axis.
// Range: -512 (full left) … 0 (center) … +511 (full right).
// Uses a P-controller with potentiometer feedback; brakes at target.
void steering_set_position(int32_t x_axis);

// Brake the steering motor immediately (e.g., on BLE disconnect).
void steering_stop(void);

#endif // STEERING_H
