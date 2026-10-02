#pragma once
#include <stdbool.h>
typedef enum {
   PHASE_A = 0,
   PHASE_B,
   PHASE_C
} pwm_phase_t;
void pwm_driver_init(void);
/* Start / stop a single phase enable pin */
void pwm_driver_start(pwm_phase_t phase);
void pwm_driver_stop(void);          /* stops ALL phases */
/* Update frequency + duty for ONE specific channel independently.
  duty_percent is the LOGICAL duty (hardware outputs the INVERTED signal). */
void pwm_driver_update(pwm_phase_t phase,
                      double frequency_hz,
                      double duty_percent);