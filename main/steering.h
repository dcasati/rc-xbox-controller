// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (Servo PWM)

#ifndef STEERING_H
#define STEERING_H

#include <stdint.h>

// Initialize servo PWM
void steering_init(void);

// Set steering position from thumbstick X-axis value
// Range: -512 to 511 (left to right)
void steering_set_position(int32_t x_axis);

// Return to center
void steering_stop(void);

#endif // STEERING_H
