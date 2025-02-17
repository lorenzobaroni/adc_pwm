#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define DISPLAY_ADDR 0x3C

#define JOYSTICK_X_PIN 27
#define JOYSTICK_Y_PIN 26
#define JOYSTICK_PB 22
#define BUTTON_A 5

#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12

#define PWM_FREQ 50
#define PWM_WRAP 4095

ssd1306_t ssd;
bool led_enabled = true;
bool led_green_state = false;
bool border_style = true;
int border_size = 2;
volatile uint32_t ultimo_tempo_joy = 0;
volatile uint32_t ultimo_tempo_A = 0;
const uint32_t debounce = 200;

void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if (gpio == JOYSTICK_PB && (tempo_atual - ultimo_tempo_joy > debounce)) {
        ultimo_tempo_joy = tempo_atual;
        led_green_state = !led_green_state;
        gpio_put(LED_GREEN, led_green_state);
        border_size = (border_size == 2) ? 4 : 2; // Alterna entre fino e grosso    
    } else if (gpio == BUTTON_A && (tempo_atual - ultimo_tempo_A > debounce)) {
        ultimo_tempo_A = tempo_atual;
        led_enabled = !led_enabled;
    }
}

void setup_pwm(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, PWM_WRAP);
    pwm_set_clkdiv(slice, 125.0);
    pwm_set_enabled(slice, true);
}

void set_led_brightness(uint pin, uint16_t value) {
    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    pwm_set_chan_level(slice, channel, value);
}

int main() {
    stdio_init_all();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, DISPLAY_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
    
    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB);
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    setup_pwm(LED_RED);
    setup_pwm(LED_BLUE);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    
    uint16_t adc_x, adc_y;
    while (true) {
        adc_select_input(1);
        adc_x = adc_read();
        adc_select_input(0);
        adc_y = adc_read();

        uint16_t pwm_x = led_enabled ? abs(adc_x - 2048) : 0;
        uint16_t pwm_y = led_enabled ? abs(adc_y - 2048) : 0;

        if (adc_x > 2040 || adc_x < 1950) {
            set_led_brightness(LED_RED, pwm_x);
        } else {
            set_led_brightness(LED_RED, 0);
        }
        if (adc_y > 2190 || adc_y < 2140) {
            set_led_brightness(LED_BLUE, pwm_y);
        } else {
            set_led_brightness(LED_BLUE, 0);
        }
        
        uint8_t pos_x = (adc_x * (WIDTH - 8)) / 4095;
        uint8_t pos_y = ((4095 - adc_y) * (HEIGHT - 8)) / 4095;        
        ssd1306_fill(&ssd, false);
        ssd1306_rect(&ssd, pos_y, pos_x, 8, 8, true, true);
        for (int i = 0; i < border_size; i++) {
            ssd1306_rect(&ssd, i, i, WIDTH - (2 * i), HEIGHT - (2 * i), true, false);
        }        
        ssd1306_send_data(&ssd);
        
        sleep_ms(50);
    }
}
