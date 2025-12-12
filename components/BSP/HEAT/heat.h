#ifndef _HEAT_H__
#define _HEAT_H__

#define TEMPERATURE_ADC_CHAN ADC_CHANNEL_0
#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_2_5

#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED 1

#define HEAT_CTRL_PIN 21
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define HEAT_FREQUENCY (20)

#define HEAT_PWM_RATIO 80 // 20  //加热占空比

#define DS18B20_PIN 1

// #if TEST_DEBUG
// #define TEMPERATURE_GOAL 20
// #else
// #define TEMPERATURE_GOAL 46 // wcy debug
// #endif

#define TEMPERATURE_BUFFER 0.3
#define TEMPERATURE_HIGH_TH (TEMPERATURE_GOAL + TEMPERATURE_BUFFER)
#define TEMPERATURE_LOW_TH (TEMPERATURE_GOAL - TEMPERATURE_BUFFER)

static void InitHeatPwm(void);
void VHeatTask(void *pvParam);
void SetHeatTempOK(uint8_t ok);
void InitHeat(void);
int IsTemperatureOK(void);

extern uint8_t AutoHeatEnable;
extern volatile float temperatureGoal;

#endif
