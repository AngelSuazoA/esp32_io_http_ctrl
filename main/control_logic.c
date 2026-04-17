#include "control_logic.h"
#include "cJSON.h"
#include <string.h>
#include <esp_log.h>
#include "pwm_driver.h"

static system_state_t state = {
    .test_running = false,
    .params_valid = false,
    .selected_phase = 'A',
    .mode = MODE_CALC,
    .direct_mode = DIRECT_TON
};

const system_state_t *get_system_state(void)
{
    return &state;
}

/* ================= PARAMETERS ================= */
void set_parameters_from_json(const char *json)
{
    if (state.test_running) return;

    cJSON *r = cJSON_Parse(json);
    if (!r) return;

    const cJSON *mode_item = cJSON_GetObjectItem(r, "mode");
    const cJSON *freq_item = cJSON_GetObjectItem(r, "Frequency");

    if (!cJSON_IsString(mode_item) || !cJSON_IsNumber(freq_item)) {
        cJSON_Delete(r);
        return;
    }

    state.Frequency = freq_item->valuedouble;
    state.Ts = (state.Frequency > 0.0) ? 1.0 / state.Frequency : 0.0;

    /* ================= CALC MODE ================= */
    if (strcmp(mode_item->valuestring, "calc") == 0) {

        const cJSON *i   = cJSON_GetObjectItem(r,"I");
        const cJSON *rs  = cJSON_GetObjectItem(r,"Rs");
        const cJSON *vp  = cJSON_GetObjectItem(r,"Vpwm");
        const cJSON *r3  = cJSON_GetObjectItem(r,"R3");
        const cJSON *r5  = cJSON_GetObjectItem(r,"R5");

        if (!cJSON_IsNumber(i) || !cJSON_IsNumber(rs) ||
            !cJSON_IsNumber(vp) || !cJSON_IsNumber(r3) ||
            !cJSON_IsNumber(r5)) {
            cJSON_Delete(r);
            return;
        }

        state.mode = MODE_CALC;

        state.I    = i->valuedouble;
        state.Rs   = rs->valuedouble;
        state.Vpwm = vp->valuedouble;
        state.R3   = r3->valuedouble;
        state.R5   = r5->valuedouble;

        state.Gain = (state.R3 > 0.0) ? (state.R5 / state.R3) : 0.0;

        /* ===== Ton formula implementation ===== */
        if (state.Gain > 0.0 && state.Vpwm > 0.0 && state.Ts > 0.0) {

            double numerator =
                (3.0 * state.Rs * state.I)
                + 1.4
                - 2.5 * (1.0 + state.Gain);

            double denominator = state.Gain * state.Vpwm;

            state.Ton = ((numerator / denominator) + 1.0) * state.Ts;

            /* Clamp Ton */
            if (state.Ton < 0.0) state.Ton = 0.0;
            if (state.Ton > state.Ts) state.Ton = state.Ts;
        } else {
            state.Ton = 0.0;
        }

        state.Duty_used = (state.Ts > 0.0)
            ? (state.Ton / state.Ts) * 100.0
            : 0.0;

        state.Ton_direct = 0.0;
        state.Duty_direct = 0.0;
    }
    /* ================= DIRECT MODE ================= */
    else {
        state.mode = MODE_DIRECT;

        const cJSON *dm = cJSON_GetObjectItem(r, "direct_mode");
        if (!cJSON_IsString(dm)) {
            cJSON_Delete(r);
            return;
        }

        if (strcmp(dm->valuestring, "ton") == 0) {
            const cJSON *ton_item = cJSON_GetObjectItem(r, "Ton");
            if (!cJSON_IsNumber(ton_item)) {
                cJSON_Delete(r);
                return;
            }

            state.direct_mode = DIRECT_TON;
            state.Ton_direct = ton_item->valuedouble;
            state.Ton = state.Ton_direct;

            state.Duty_used = (state.Ts > 0.0)
                ? (state.Ton / state.Ts) * 100.0
                : 0.0;

            state.Duty_direct = 0.0;
        }
        else {
            const cJSON *duty_item = cJSON_GetObjectItem(r, "Duty");
            if (!cJSON_IsNumber(duty_item)) {
                cJSON_Delete(r);
                return;
            }

            state.direct_mode = DIRECT_DUTY;
            state.Duty_direct = duty_item->valuedouble;
            state.Ton = state.Ts * state.Duty_direct / 100.0;
            state.Duty_used = state.Duty_direct;
            state.Ton_direct = 0.0;
        }
    }

    state.Toff = state.Ts - state.Ton;
    state.params_valid = (state.Ts > 0.0);
    pwm_driver_update(state.Frequency, state.Duty_used);
    ESP_LOGI("PARAMS", "I=%.3f Rs=%.3f Vpwm=%.3f R3=%.3f R5=%.3f Ts=%f",
         state.I, state.Rs, state.Vpwm, state.R3, state.R5, state.Ts);
    cJSON_Delete(r);
}

/* ================= PHASE ================= */
void select_phase(char phase)
{
    if (state.test_running) return;
    if (phase == 'A' || phase == 'B' || phase == 'C')
        state.selected_phase = phase;
}

/* ================= TEST ================= */
bool start_test(void)
{
    if (!state.params_valid) return false;
    if (state.test_running) return false;

    state.test_running = true;

    pwm_phase_t p =
        state.selected_phase == 'A' ? PHASE_A :
        state.selected_phase == 'B' ? PHASE_B :
                                      PHASE_C;

    pwm_driver_start(p);
    pwm_driver_update(state.Frequency, state.Duty_used);

    return true;
}

void stop_test(void)
{
    state.test_running = false;
    pwm_driver_stop();
}