#include "beep.h"

volatile uint8_t BuzzerEnable;
volatile uint8_t BuzzerCnt;


void InitBuzzer(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = BUZZER_MODE,
        .timer_num = BUZZER_TIMER,
        .duty_resolution = BUZZER_DUTY_RES,
        .freq_hz = BUZZER_FREQ_CLICK,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = BUZZER_MODE,
        .channel = BUZZER_CHANNEL,
        .timer_sel = BUZZER_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BUZZER_FREQ_PIN,
        .duty = 0, // Set duty to 0%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void StartBuzzer(BUZZER_SOUND_TYPE type)
{
    uint32_t fullduty = (1 << BUZZER_DUTY_RES) - 1;
    uint32_t duty, ratio, freq_hz;
    uint32_t freqBuf[BST_MAX] = {BUZZER_FREQ_CLICK, BUZZER_FREQ_SUCC, BUZZER_FREQ_ERROR};

    if (type >= BST_MAX)
        return;
    // gpio_set_level(BUZZER_POWER_PIN, 1);
    freq_hz = freqBuf[type];
    ratio = 50;
    duty = fullduty * ratio / 100;
    printf("buzzer  pwm=%ld\n", duty);

    ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, freq_hz);
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, duty);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
    BuzzerCnt = 0;
    BuzzerEnable = 1;
}
void StopBuzzer(void)
{
    // gpio_set_level(BUZZER_POWER_PIN, 0);
    ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, BUZZER_FREQ_CLICK);
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}
