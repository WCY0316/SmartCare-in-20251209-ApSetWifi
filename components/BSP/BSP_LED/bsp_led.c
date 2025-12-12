#include "bsp_led.h"
#include <driver/i2c.h>
#include "bsp_gpio.h"

COLOR_LED_CTRL_STRUCT ColorLedConfig[rgbLed_Index_Max];
volatile int blinkCnt = 0, flipFlag = 0;
COLOR_LED_CTRL_STRUCT *pConfig;

static i2c_config_t i2c_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = AW9523_SDA_PIN,
    .scl_io_num = AW9523_SCL_PIN,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000 // Adjust the I2C clock speed as needed
};

esp_err_t aw9523_init(void)
{
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    // Reset AW9523
    gpio_config_t rst_gpio_conf = {
        .pin_bit_mask = (1ULL << AW9523_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&rst_gpio_conf);
    gpio_set_level(AW9523_RST_PIN, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS); // Hold reset low for at least 10ms
    gpio_set_level(AW9523_RST_PIN, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS); // Allow AW9523 to initialize

    // Configure AW9523 as LED driver
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, (AW9523_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    // i2c_master_write_byte(cmd, AW9523_MODE1_REG, true);
    // i2c_master_write_byte(cmd, 0x00, true); // Set MODE1 register to enable PWM mode
    // i2c_master_stop(cmd);
    // i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    // i2c_cmd_link_delete(cmd);

    return ESP_OK;
}
esp_err_t aw9523b_set_led_mode(void)
{
    // Set LED mode switch to 0 (LED mode)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, AW9523_MODE_SWITCH_P0_REG, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, AW9523_MODE_SWITCH_P1_REG, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}

esp_err_t aw9523b_set_led_mode_for_pin(uint8_t pin)
{
    if (pin > 15) // AW9523 has 16 pins (P0_0 to P1_7)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t led_mode_reg = (pin < 8) ? 0x12 : (0x13);
    uint8_t led_mode_val;

    // Read the current value of the LED mode register
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, led_mode_reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &led_mode_val, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
    {
        return ret;
    }

    // Set the corresponding bit to 0 to enable LED mode for the specified pin
    led_mode_val &= ~(1 << (pin % 8));

    // Write the new value back to the LED mode register
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, led_mode_reg, true);
    i2c_master_write_byte(cmd, led_mode_val, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}
esp_err_t aw9523b_set_dim(uint8_t pin, uint8_t value)
{
    if (pin > 9)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t dim_reg = AW9523_DIM4_REG + pin;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, dim_reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}
esp_err_t aw9523_set_rgb(uint8_t ledIndex, uint8_t R, uint8_t G, uint8_t B)
{
    if (ledIndex >= 9)
        return ESP_ERR_INVALID_ARG;

    uint8_t dimRegBase = AW9523_DIM4_REG + ledIndex * 3; // LED 驱动电流配置,P0_0 口开始

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, dimRegBase, true);
    i2c_master_write_byte(cmd, R, true);

    // i2c_master_write_byte(cmd, dimRegBase+1, true);
    i2c_master_write_byte(cmd, G, true);
    // i2c_master_write_byte(cmd, dimRegBase+2, true);
    i2c_master_write_byte(cmd, B, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}
esp_err_t aw9523_set_pwm(uint8_t pin, uint8_t value)
{
    if (pin > 15) // AW9523 has 16 pins (P0_0 to P1_7)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t pwm_reg = AW9523_DIM0_REG + pin;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, pwm_reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

void InitLed(void)
{
    memset(&ColorLedConfig, 0, sizeof(ColorLedConfig));
    // 初始化AW9523B
    if (aw9523_init() != ESP_OK)
    {
        printf("Failed to initialize AW9523B\n");
    }
    else
        printf("aw9523 init OK\n");

    // 将0x12 0x13均配置为0，即LED模式
    if (aw9523b_set_led_mode() != ESP_OK)
    {
        printf("Failed to set LED mode\n");
        return;
    }
    // 确保P1_4配置为LED模式
    if (aw9523b_set_led_mode_for_pin(12) != ESP_OK)
    {
        printf("Failed to set P1_4 LED mode\n");
        return;
    }

    // buzzer_pwm_init();
    // xTaskCreate(RGBLedTask, "RGBLED_Task", 2048, NULL, 10, NULL);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    SetRGBLedColor(rgbLed_Heating, LED_COLOR_OFF, LED_NOT_BLINK);
    SetRGBLedColor(rgbLed_Reacting, LED_COLOR_OFF, LED_NOT_BLINK);
    SetRGBLedColor(rgbLed_Wifi, LED_COLOR_BLUE, LED_BLINK);
}
// 加热：    R:P0_0  G:P0_1  B:P0_2
// 反应：    R:P0_3  G:P0_4  B:P0_5
// wifi：    R:P0_6  G:P0_7  B:P1_4
esp_err_t SetRGBLedColor(RGB_LED_INDEX index, LED_COLOR color, uint8_t IsBlink)
{
    uint8_t R = 0, G = 0, B = 0;
    COLOR_LED_CTRL_STRUCT *pConfig;

    if ((index >= rgbLed_Index_Max) || (color >= LED_COLOR_MAX))
    {
        printf("Led index or color error\n");
        return -2;
    }

    switch (color)
    {
    // wcy debug
    case LED_COLOR_RED:
        B = 255;
        break;
    case LED_COLOR_GREEN:
        R = 255;
        break;
    case LED_COLOR_BLUE:
        G = 255;
        break;
    case LED_COLOR_OFF:
    default:
        break;
    }
    pConfig = &ColorLedConfig[index];
    pConfig->R = R;
    pConfig->G = G;
    pConfig->B = B;
    pConfig->BlinkFlag = IsBlink;
    if (IsBlink == 0)
        pConfig->NeedAction = 1;
    printf("Led%d set R=%d, G=%d, B=%d, blink=%d\n", index, pConfig->R, pConfig->G, pConfig->B, pConfig->BlinkFlag);
    
    return ESP_OK;
}
// 呼吸灯控制
void BreathLedCtrl(LED_STATE ledState)
{
    gpio_set_level(BREATH_LED_PIN, ledState);
}
// 闪光灯控制
void CameraLedCtrl(LED_STATE ledState)
{
    gpio_set_level(CAMERA_LED_PIN, ledState);
    // printf("camera LED: %d\n",ledState);
}

void SelfCheck(void)
{
    CameraLedCtrl(LED_ON);
    vTaskDelay(pdMS_TO_TICKS(500));
    CameraLedCtrl(LED_OFF);
}