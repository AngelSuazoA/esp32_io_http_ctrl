#pragma once
#include <stdbool.h>

typedef enum {
    MODE_CALC,
    MODE_DIRECT
} control_mode_t;

typedef enum {
    DIRECT_TON,
    DIRECT_DUTY
} direct_mode_t;

typedef struct {
    bool test_running;
    bool params_valid;

    char selected_phase;     // 'A','B','C'
    control_mode_t mode;
    direct_mode_t direct_mode;

    /* Inputs (CALC mode) */
    double I;
    double Rs;
    double Vpwm;
    double R3;
    double R5;

    /* Common */
    double Frequency;

    /* Direct inputs */
    double Ton_direct;
    double Duty_direct;

    /* Calculated / used */
    double Gain;
    double Ts;
    double Ton;
    double Toff;
    double Duty_used;

} system_state_t;

const system_state_t *get_system_state(void);

void set_parameters_from_json(const char *json);
void select_phase(char phase);
bool start_test(void);
void stop_test(void);