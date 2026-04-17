#pragma once
#include <stdbool.h>

typedef enum {
    PHASE_A = 0,
    PHASE_B,
    PHASE_C
} pwm_phase_t;

void pwm_driver_init(void);
void pwm_driver_start(pwm_phase_t phase);
void pwm_driver_stop(void);

/* Can be called before start, after start, or while running */
void pwm_driver_update(double frequency_hz, double duty_percent);