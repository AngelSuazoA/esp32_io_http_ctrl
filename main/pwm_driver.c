#include "pwm_driver.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
/* ===== GPIO MAP (ESP‑WROOM‑32) ===== */
#define PWM_A_GPIO   4
#define PWM_B_GPIO   5
#define PWM_C_GPIO   6
#define EN_A_GPIO    15
#define EN_B_GPIO    16
#define EN_C_GPIO    17
#define MCPWM_GROUP_ID  0
#define MCPWM_RES_HZ    (80 * 1000 * 1000)   /* 80 MHz base clock */
static const char *TAG = "PWM_DRIVER";
/* Each channel owns its own timer so frequencies can be truly independent */
static mcpwm_timer_handle_t  timers[3];
static mcpwm_oper_handle_t   operators[3];
static mcpwm_cmpr_handle_t   comparators[3];
static mcpwm_gen_handle_t    generators[3];
static uint32_t period_ticks[3] = {0, 0, 0};
static const int PWM_GPIOS[3] = { PWM_A_GPIO, PWM_B_GPIO, PWM_C_GPIO };
static const int EN_GPIOS[3]  = { EN_A_GPIO,  EN_B_GPIO,  EN_C_GPIO  };
/* ===== ENABLE CONTROL ===== */
static void set_enable(pwm_phase_t phase, bool enable)
{
   gpio_set_level(EN_GPIOS[phase], enable ? 1 : 0);
}
/* ===== INIT ===== */
void pwm_driver_init(void)
{
   /* Enable GPIOs */
   gpio_config_t gpio_cfg = {
       .mode = GPIO_MODE_OUTPUT,
       .pin_bit_mask =
           (1ULL << EN_A_GPIO) |
           (1ULL << EN_B_GPIO) |
           (1ULL << EN_C_GPIO)
   };
   ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
   set_enable(PHASE_A, false);
   set_enable(PHASE_B, false);
   set_enable(PHASE_C, false);
   double init_freq = 16000.0;
   for (int i = 0; i < 3; i++) {
       period_ticks[i] = (uint32_t)(MCPWM_RES_HZ / init_freq);
       /* --- Timer (one per channel) --- */
       mcpwm_timer_config_t timer_cfg = {
           .group_id      = MCPWM_GROUP_ID,
           .clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT,
           .resolution_hz = MCPWM_RES_HZ,
           .period_ticks  = period_ticks[i],
           .count_mode    = MCPWM_TIMER_COUNT_MODE_UP,
       };
       ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &timers[i]));
       /* --- Operator --- */
       mcpwm_operator_config_t oper_cfg = { .group_id = MCPWM_GROUP_ID };
       ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &operators[i]));
       ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators[i], timers[i]));
       /* --- Comparator --- */
       mcpwm_comparator_config_t cmp_cfg = {
           .flags.update_cmp_on_tez = true,
       };
       ESP_ERROR_CHECK(mcpwm_new_comparator(operators[i], &cmp_cfg, &comparators[i]));
       /* Start with compare = period → output always LOW (inverted: always HIGH) */
       ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], period_ticks[i]));
       /* --- Generator --- */
       mcpwm_generator_config_t gen_cfg = { .gen_gpio_num = PWM_GPIOS[i] };
       ESP_ERROR_CHECK(mcpwm_new_generator(operators[i], &gen_cfg, &generators[i]));
       /*
        * INVERTED PWM logic:
        *   On timer empty (count == 0) → set output LOW
        *   On compare match           → set output HIGH
        *
        * This means the pin is HIGH from compare→period and LOW from 0→compare.
        * Logical duty D% → compare = D/100 * period
        * Actual high time = (period - compare) / period = (1 - D/100) = inverted.
        *
        * Example: logical 10% → compare = 0.1*period → actual HIGH = 90%
        */
       ESP_ERROR_CHECK(
           mcpwm_generator_set_action_on_timer_event(
               generators[i],
               MCPWM_GEN_TIMER_EVENT_ACTION(
                   MCPWM_TIMER_DIRECTION_UP,
                   MCPWM_TIMER_EVENT_EMPTY,
                   MCPWM_GEN_ACTION_LOW)));   /* timer empty → LOW */
       ESP_ERROR_CHECK(
           mcpwm_generator_set_action_on_compare_event(
               generators[i],
               MCPWM_GEN_COMPARE_EVENT_ACTION(
                   MCPWM_TIMER_DIRECTION_UP,
                   comparators[i],
                   MCPWM_GEN_ACTION_HIGH)));  /* compare match → HIGH */
       /* Enable and start timer */
       ESP_ERROR_CHECK(mcpwm_timer_enable(timers[i]));
       ESP_ERROR_CHECK(mcpwm_timer_start_stop(timers[i], MCPWM_TIMER_START_NO_STOP));
   }
   ESP_LOGI(TAG, "MCPWM initialized: 3 independent timers, inverted output");
}
void pwm_driver_update(pwm_phase_t phase, double frequency_hz, double duty_percent)
{
    if (frequency_hz <= 0.0) return;
    if (duty_percent < 0.0)   duty_percent = 0.0;
    if (duty_percent > 100.0) duty_percent = 100.0;

    int i = (int)phase;
    uint32_t new_period = (uint32_t)(MCPWM_RES_HZ / frequency_hz);
    if (new_period == 0) return;

    uint32_t cmp = (uint32_t)((new_period * duty_percent) / 100.0);

    if (new_period != period_ticks[i]) {
        ESP_ERROR_CHECK(mcpwm_timer_set_period(timers[i], new_period));
        period_ticks[i] = new_period;
    }

    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], cmp));
}

/* ===== START (enable one phase) ===== */
void pwm_driver_start(pwm_phase_t phase)
{
   set_enable(phase, true);
}
/* ===== STOP ALL ===== */
void pwm_driver_stop(void)
{
   /* Drive compare to period → output always LOW (inverted: always HIGH = safe idle) */
   for (int i = 0; i < 3; i++) {
    //    ESP_ERROR_CHECK(
    //        mcpwm_comparator_set_compare_value(comparators[i], period_ticks[i]));
       set_enable((pwm_phase_t)i, false);
       esp_rom_delay_us(10000);
   }
}