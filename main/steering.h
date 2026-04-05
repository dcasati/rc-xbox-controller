// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (DC motor + potentiometer feedback)

#ifndef STEERING_H
#define STEERING_H

#include <stdint.h>

// Initialize steering motor GPIOs, PWM, and ADC
void steering_init(void);

// Set steering position from thumbstick X-axis value
// Range: -512 to 511 (left to right)
void steering_set_position(int32_t x_axis);

// Return to center and stop
void steering_stop(void);

#endif // STEERING_H
