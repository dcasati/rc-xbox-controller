// SPDX-License-Identifier: MIT
// RC Xbox Controller — LED Headlight Control

#ifndef LED_H
#define LED_H

#include <stdbool.h>

// Initialize headlight GPIO
void led_init(void);

// Toggle headlight on/off
void led_toggle(void);

// Set headlight state explicitly
void led_set(bool on);

// Turn off headlight
void led_off(void);

#endif // LED_H
