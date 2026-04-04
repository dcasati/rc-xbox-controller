// SPDX-License-Identifier: MIT
// RC Xbox Controller — Motor Control (TB6612FNG Channel A)

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>

// Initialize drive motor GPIOs and PWM
void motor_control_init(void);

// Set drive motor speed: positive = forward, negative = reverse, 0 = brake
// Range: -1023 to 1023
void motor_set_speed(int32_t speed);

// Emergency stop — brake immediately
void motor_stop(void);

#endif // MOTOR_CONTROL_H
