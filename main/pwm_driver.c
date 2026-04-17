#include "pwm_driver.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* ===== GPIO MAP (ESP‑WROOM‑32) ===== */
#define PWM_A_GPIO   18
#define PWM_B_GPIO   21
#define PWM_C_GPIO   23

#define EN_A_GPIO    19
#define EN_B_GPIO    22
#define EN_C_GPIO    5

#define MCPWM_GROUP_ID  0
#define MCPWM_RES_HZ    (80 * 1000 * 1000)   // 80 MHz base

static const char *TAG = "PWM_DRIVER";

/* MCPWM handles */
static mcpwm_timer_handle_t timer = NULL;
static mcpwm_oper_handle_t operators[3];
static mcpwm_cmpr_handle_t comparators[3];
static mcpwm_gen_handle_t generators[3];

static pwm_phase_t active_phase = PHASE_A;
static uint32_t current_period_ticks = 0;

/* ===== ENABLE CONTROL ===== */
static void enable_selected_phase(bool enable)
{
    gpio_set_level(EN_A_GPIO, enable && active_phase == PHASE_A);
    gpio_set_level(EN_B_GPIO, enable && active_phase == PHASE_B);
    gpio_set_level(EN_C_GPIO, enable && active_phase == PHASE_C);
}

void pwm_driver_init(void)
{
    /* ===== ENABLE GPIOs ===== */
    gpio_config_t gpio_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL << EN_A_GPIO) |
            (1ULL << EN_B_GPIO) |
            (1ULL << EN_C_GPIO)
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    enable_selected_phase(false);

    /* ===== TIMER ===== */
    double init_freq = 16000.0;   // default only
    current_period_ticks = (uint32_t)(MCPWM_RES_HZ / init_freq);

    mcpwm_timer_config_t timer_cfg = {
        .group_id = MCPWM_GROUP_ID,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = MCPWM_RES_HZ,
        .period_ticks = current_period_ticks,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &timer));

    /* ===== OPERATORS + COMPARATORS + GENERATORS ===== */
    const int pwm_gpios[3] = { PWM_A_GPIO, PWM_B_GPIO, PWM_C_GPIO };

    for (int i = 0; i < 3; i++) {

        /* Operator */
        mcpwm_operator_config_t oper_cfg = {
            .group_id = MCPWM_GROUP_ID,
        };
        ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &operators[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators[i], timer));

        /* Comparator */
        mcpwm_comparator_config_t cmp_cfg = {
            .flags.update_cmp_on_tez = true,
        };
        ESP_ERROR_CHECK(mcpwm_new_comparator(
            operators[i], &cmp_cfg, &comparators[i]));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], 0));

        /* Generator */
        mcpwm_generator_config_t gen_cfg = {
            .gen_gpio_num = pwm_gpios[i],
        };
        ESP_ERROR_CHECK(mcpwm_new_generator(
            operators[i], &gen_cfg, &generators[i]));

        /* PWM actions */
        ESP_ERROR_CHECK(
            mcpwm_generator_set_action_on_timer_event(
                generators[i],
                MCPWM_GEN_TIMER_EVENT_ACTION(
                    MCPWM_TIMER_DIRECTION_UP,
                    MCPWM_TIMER_EVENT_EMPTY,
                    MCPWM_GEN_ACTION_HIGH)));

        ESP_ERROR_CHECK(
            mcpwm_generator_set_action_on_compare_event(
                generators[i],
                MCPWM_GEN_COMPARE_EVENT_ACTION(
                    MCPWM_TIMER_DIRECTION_UP,
                    comparators[i],
                    MCPWM_GEN_ACTION_LOW)));
    }

    /* ===== START TIMER ===== */
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(
        mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "MCPWM initialized (3 operators, 1 timer)");
}

void pwm_driver_update(double frequency_hz, double duty_percent)
{
    if (frequency_hz <= 0.0) return;

    if (duty_percent < 0.0) duty_percent = 0.0;
    if (duty_percent > 100.0) duty_percent = 100.0;

    uint32_t new_period = (uint32_t)(MCPWM_RES_HZ / frequency_hz);
    if (new_period == 0) return;

    uint32_t cmp = (uint32_t)((duty_percent / 100.0) * new_period);

    if (new_period != current_period_ticks) {
        ESP_ERROR_CHECK(mcpwm_timer_set_period(timer, new_period));
        current_period_ticks = new_period;
    }

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(
            mcpwm_comparator_set_compare_value(comparators[i], cmp));
    }
}

void pwm_driver_start(pwm_phase_t phase)
{
    active_phase = phase;
    enable_selected_phase(true);
}

void pwm_driver_stop(void)
{
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(
            mcpwm_comparator_set_compare_value(comparators[i], 0));
    }
    enable_selected_phase(false);
}