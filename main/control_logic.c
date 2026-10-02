#include "control_logic.h"
#include "cJSON.h"
#include <string.h>
#include <esp_log.h>
#include "pwm_driver.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
static system_state_t state = {
   .test_running   = false,
   .params_valid   = false,
   .selected_phase = 'A',
   .active_test    = TEST_SINGLE,
   .seq_step       = 0,
};
/* ---- esp_timer handle for sequenced tests ---- */
static esp_timer_handle_t seq_timer = NULL;
const system_state_t *get_system_state(void)
{
   return &state;
}
/* ==========================================================
  HELPERS
  ========================================================== */
static bool parse_pwm_params(const cJSON *obj, pwm_params_t *p)
{
   const cJSON *mode_item = cJSON_GetObjectItem(obj, "mode");
   const cJSON *freq_item = cJSON_GetObjectItem(obj, "Frequency");
   if (!cJSON_IsString(mode_item) || !cJSON_IsNumber(freq_item))
       return false;
   p->Frequency = freq_item->valuedouble;
   p->Ts        = (p->Frequency > 0.0) ? 1.0 / p->Frequency : 0.0;
   /* ---- CALC ---- */
   if (strcmp(mode_item->valuestring, "calc") == 0) {
       const cJSON *i  = cJSON_GetObjectItem(obj, "I");
       const cJSON *rs = cJSON_GetObjectItem(obj, "Rs");
       const cJSON *vp = cJSON_GetObjectItem(obj, "Vpwm");
       const cJSON *r3 = cJSON_GetObjectItem(obj, "R3");
       const cJSON *r5 = cJSON_GetObjectItem(obj, "R5");
       if (!cJSON_IsNumber(i) || !cJSON_IsNumber(rs) ||
           !cJSON_IsNumber(vp) || !cJSON_IsNumber(r3) ||
           !cJSON_IsNumber(r5))
           return false;
       p->mode = MODE_CALC;
       p->I    = i->valuedouble;
       p->Rs   = rs->valuedouble;
       p->Vpwm = vp->valuedouble;
       p->R3   = r3->valuedouble;
       p->R5   = r5->valuedouble;
       p->Gain = (p->R3 > 0.0) ? (p->R5 / p->R3) : 0.0;
       if (p->Gain > 0.0 && p->Vpwm > 0.0 && p->Ts > 0.0) {
           double num = (3.0 * p->Rs * p->I) + 1.4 - 2.5 * (1.0 + p->Gain);
           double den = p->Gain * p->Vpwm;
           p->Ton = ((num / den) + 1.0) * p->Ts;
           if (p->Ton < 0.0)   p->Ton = 0.0;
           if (p->Ton > p->Ts) p->Ton = p->Ts;
       } else {
           p->Ton = 0.0;
       }
       p->Duty_used   = (p->Ts > 0.0) ? (p->Ton / p->Ts) * 100.0 : 0.0;
       p->Ton_direct  = 0.0;
       p->Duty_direct = 0.0;
   }
   /* ---- DIRECT ---- */
   else {
       p->mode = MODE_DIRECT;
       const cJSON *dm = cJSON_GetObjectItem(obj, "direct_mode");
       if (!cJSON_IsString(dm)) return false;
       if (strcmp(dm->valuestring, "ton") == 0) {
           const cJSON *ton_item = cJSON_GetObjectItem(obj, "Ton");
           if (!cJSON_IsNumber(ton_item)) return false;
           p->direct_mode = DIRECT_TON;
           p->Ton_direct  = ton_item->valuedouble;
           p->Ton         = p->Ton_direct;
           p->Duty_used   = (p->Ts > 0.0) ? (p->Ton / p->Ts) * 100.0 : 0.0;
           p->Duty_direct = 0.0;
       } else {
           const cJSON *duty_item = cJSON_GetObjectItem(obj, "Duty");
           if (!cJSON_IsNumber(duty_item)) return false;
           p->direct_mode = DIRECT_DUTY;
           p->Duty_direct = duty_item->valuedouble;
           p->Ton         = p->Ts * p->Duty_direct / 100.0;
           p->Duty_used   = p->Duty_direct;
           p->Ton_direct  = 0.0;
       }
   }
   p->Toff = p->Ts - p->Ton;
   return true;
}
/* ==========================================================
  SINGLE PHASE (legacy)
  ========================================================== */
void set_single_params_from_json(const char *json)
{
   if (state.test_running) return;
   cJSON *r = cJSON_Parse(json);
   if (!r) return;
   if (parse_pwm_params(r, &state.single)) {
       state.params_valid = (state.single.Ts > 0.0);
       pwm_driver_update(PHASE_A, state.single.Frequency, state.single.Duty_used);
       pwm_driver_update(PHASE_B, state.single.Frequency, state.single.Duty_used);
       pwm_driver_update(PHASE_C, state.single.Frequency, state.single.Duty_used);
   }
   cJSON_Delete(r);
}
void select_phase(char phase)
{
   if (state.test_running) return;
   if (phase == 'A' || phase == 'B' || phase == 'C')
       state.selected_phase = phase;
}
bool start_single_test(void)
{
   if (!state.params_valid) return false;
   if (state.test_running)  return false;
   state.active_test  = TEST_SINGLE;
   state.test_running = true;
   pwm_phase_t p =
       state.selected_phase == 'A' ? PHASE_A :
       state.selected_phase == 'B' ? PHASE_B : PHASE_C;
   pwm_driver_start(p);
   esp_rom_delay_us(2000); /* Ensure enable takes effect before PWM update */
   pwm_driver_update(p, state.single.Frequency, state.single.Duty_used);
   return true;
}
/* ==========================================================
  FRAME PARAMS (R Frame & V Frame share inrush/steady)
  ========================================================== */
void set_frame_params_from_json(const char *json)
{
   if (state.test_running) return;
   cJSON *r = cJSON_Parse(json);
   if (!r) return;
   const cJSON *inrush_obj = cJSON_GetObjectItem(r, "inrush");
   const cJSON *steady_obj = cJSON_GetObjectItem(r, "steady");
   bool ok = true;
   if (inrush_obj) ok &= parse_pwm_params(inrush_obj, &state.inrush);
   if (steady_obj) ok &= parse_pwm_params(steady_obj, &state.steady);
   state.params_valid = ok &&
                        (state.inrush.Ts > 0.0) &&
                        (state.steady.Ts > 0.0);
   cJSON_Delete(r);
}
/* ==========================================================
  SEQUENCE TIMER CALLBACK  (called from esp_timer ISR context)
  ========================================================== */
/*
R Frame sequence (step index):
   0 – Phase A inrush  START   (50 ms duration)
   1 – Phase A steady  START   (stays)
   2 – Phase B inrush  START   (50 ms duration, begins 8 ms after step 1)
   3 – Phase B steady  START   (stays)
V Frame adds:
   4 – Phase C inrush  START   (50 ms, begins 8 ms after step 3)
   5 – Phase C steady  START   (stays)
Stop is triggered by the user via stop_test().
*/
static void seq_timer_cb(void *arg)
{
   int max_steps = (state.active_test == TEST_R_FRAME) ? 4 : 6;
   state.seq_step++;
   if (state.seq_step >= max_steps) {
       /* All phases running steady — nothing more to schedule */
       return;
   }
   uint64_t next_delay_us = 0;
   switch (state.seq_step) {
       /* ---- A steady ---- */
       case 1:
           pwm_driver_update(PHASE_A, state.steady.Frequency, state.steady.Duty_used);
           next_delay_us = 8000ULL;   /* 8 ms until B/C inrush */
           break;
       /* ---- C inrush (R Frame) / C inrush (V Frame) ---- */
       case 2:
           pwm_driver_start(PHASE_C);
           pwm_driver_update(PHASE_C, state.inrush.Frequency, state.inrush.Duty_used);
           next_delay_us = 50000ULL;  /* 50 ms inrush duration */
           break;
       /* ---- B steady (R Frame) / C steady (V Frame) ---- */
       case 3:
           pwm_driver_update(PHASE_C, state.steady.Frequency, state.steady.Duty_used);
           if (state.active_test == TEST_V_FRAME) {
               next_delay_us = 8000ULL;
           }
           break;
       /* ---- C inrush (V Frame only) ---- */
       case 4:
           pwm_driver_start(PHASE_B);
           pwm_driver_update(PHASE_B, state.inrush.Frequency, state.inrush.Duty_used);
           next_delay_us = 50000ULL;  /* 50 ms inrush duration */
           break;
       /* ---- C steady (V Frame only) ---- */
       case 5:
           pwm_driver_update(PHASE_B, state.steady.Frequency, state.steady.Duty_used);
           break;
       default:
           break;
   }
   if (next_delay_us > 0 && state.seq_step < max_steps - 1) {
       esp_timer_start_once(seq_timer, next_delay_us);
   }
}
/* Helper: create the esp_timer once, or do nothing if already created */
static void seq_timer_ensure_created(void)
{
   if (seq_timer != NULL) return;
   const esp_timer_create_args_t args = {
       .callback        = seq_timer_cb,
       .arg             = NULL,
       .dispatch_method = ESP_TIMER_TASK,   /* runs in esp_timer task, not ISR */
       .name            = "seq_timer",
   };
   ESP_ERROR_CHECK(esp_timer_create(&args, &seq_timer));
}
/* ==========================================================
  R FRAME START
  ========================================================== */
bool start_r_frame_test(void)
{
   if (!state.params_valid) return false;
   if (state.test_running)  return false;
   state.active_test  = TEST_R_FRAME;
   state.test_running = true;
   state.seq_step     = 0;
   /* Phase A inrush */
   pwm_driver_start(PHASE_A);
   esp_rom_delay_us(2000); /* Ensure enable takes effect */
   pwm_driver_update(PHASE_A, state.inrush.Frequency, state.inrush.Duty_used);
   seq_timer_ensure_created();
   esp_timer_start_once(seq_timer, 50000ULL); /* 50 ms → step 1 (A steady) */
   return true;
}
/* ==========================================================
  V FRAME START
  ========================================================== */
bool start_v_frame_test(void)
{
   if (!state.params_valid) return false;
   if (state.test_running)  return false;
   state.active_test  = TEST_V_FRAME;
   state.test_running = true;
   state.seq_step     = 0;
   /* Phase A inrush */
   pwm_driver_start(PHASE_A);
   esp_rom_delay_us(2000); /* Ensure enable takes effect */
   pwm_driver_update(PHASE_A, state.inrush.Frequency, state.inrush.Duty_used);
   seq_timer_ensure_created();
   esp_timer_start_once(seq_timer, 50000ULL); /* 50 ms → step 1 (A steady) */
   return true;
}
/* ==========================================================
  STOP
  ========================================================== */
void stop_test(void)
{
   if (seq_timer != NULL) {
       esp_timer_stop(seq_timer); /* safe to call even if not running */
   }
   state.test_running = false;
   state.seq_step     = 0;
   pwm_driver_stop();
   pwm_driver_update(PHASE_A, 16000.0, 0.0f);
   pwm_driver_update(PHASE_B, 16000.0, 0.0f);
   pwm_driver_update(PHASE_C, 16000.0, 0.0f);
}