#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
typedef enum { MODE_CALC, MODE_DIRECT } control_mode_t;
typedef enum { DIRECT_TON, DIRECT_DUTY } direct_mode_t;

/* Which test is active */
typedef enum {
   TEST_SINGLE = 0,
   TEST_R_FRAME,
   TEST_V_FRAME
} test_type_t;

/* A self-contained PWM parameter set (inrush or steady state) */
typedef struct {
   control_mode_t mode;
   direct_mode_t  direct_mode;
   /* CALC inputs */
   double I;
   double Rs;
   double Vpwm;
   double R3;
   double R5;
   /* DIRECT inputs */
   double Ton_direct;
   double Duty_direct;
   /* Frequency (common for both inrush & steady) */
   double Frequency;
   /* Calculated / used */
   double Gain;
   double Ts;
   double Ton;
   double Toff;
   double Duty_used;
} pwm_params_t;

typedef struct {
   bool           test_running;
   bool           params_valid;
   char           selected_phase;   /* Single test only: 'A','B','C' */
   test_type_t    active_test;
   /* ---- Single-phase test (legacy) ---- */
   pwm_params_t   single;           /* one parameter set */
   /* ---- R Frame / V Frame shared params ---- */
   pwm_params_t   inrush;
   pwm_params_t   steady;
   /* Runtime state for sequenced tests */
   int            seq_step;         /* which step of the sequence is active */
} system_state_t;

const system_state_t *get_system_state(void);

/* Single test – legacy interface */
void set_single_params_from_json(const char *json);
void select_phase(char phase);
bool start_single_test(void);

/* R Frame / V Frame */
void set_frame_params_from_json(const char *json);   /* sets inrush + steady */
bool start_r_frame_test(void);
bool start_v_frame_test(void);

/* Common stop */
void stop_test(void);