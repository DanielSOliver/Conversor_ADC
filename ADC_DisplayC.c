#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"

const uint LED_AZUL = 12;
const uint LED_VERMELHO = 13;
const uint LED_VERDE = 11;
const uint BOTAO_A = 5;
const uint BOTAO_B = 6;
const uint JOYSTICK_X_PIN = 26;
const uint JOYSTICK_Y_PIN = 27;
const uint JOYSTICK_PB = 22;
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

static volatile uint32_t last_time = 0;
volatile bool desativa_leds = false; 
volatile bool apenas_verde = false; 

ssd1306_t ssd;
uint pos_x = 60, pos_y = 30;

void debounce_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time > 300000) {  // Debounce de 300ms
        last_time = current_time;

        if (gpio == BOTAO_A) {
            desativa_leds = !desativa_leds;
            apenas_verde = false;  // Se alternar LEDs, desativa modo "apenas verde"
        } 

        if (gpio == JOYSTICK_PB) {
            apenas_verde = !apenas_verde;  // Alterna estado de "apenas LED verde"
            desativa_leds = apenas_verde;  // Se "apenas verde" estiver ativo, desliga os outros LEDs
        } 
    }
}

void configurar_pwm(uint pino) {
    gpio_set_function(pino, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pino);
    pwm_set_wrap(slice, 4095);
    pwm_set_enabled(slice, true);
}

void atualizar_led_pwm(uint pino, uint16_t valor) {
    uint slice = pwm_gpio_to_slice_num(pino);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(pino), valor);
}

int main() {
    stdio_init_all();
    
    // Configuração do I2C para o display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, 128, 64, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd); // Configura o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    // Configuração do ADC (joystick)
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);

    // Configuração do LED verde
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0); // Inicia apagado

    // Configuração dos botões com pull-up
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB);

    // Configuração das interrupções
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &debounce_irq_handler);
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, &debounce_irq_handler);

    // Configuração PWM para os LEDs azul e vermelho
    configurar_pwm(LED_AZUL);
    configurar_pwm(LED_VERMELHO);

    while (true) {
        adc_select_input(0);
        uint16_t adc_x = adc_read();
        adc_select_input(1);
        uint16_t adc_y = adc_read();

        if (apenas_verde) {
            // Apenas LED verde aceso, desliga PWM
            atualizar_led_pwm(LED_VERMELHO, 0);
            atualizar_led_pwm(LED_AZUL, 0);
            gpio_put(LED_VERDE, 1);

            ssd1306_rect(&ssd, 1, 1, 122, 60, true, false);
            ssd1306_send_data(&ssd);
            sleep_ms(100);

        } 
        else if (!desativa_leds) {
            // Controle normal dos LEDs PWM
            uint16_t brilho_vermelho = abs((int)adc_y - 2048) * 2;
            uint16_t brilho_azul = abs((int)adc_x - 2048) * 2;

            if (brilho_vermelho < 500) brilho_vermelho = 0;
            if (brilho_azul < 500) brilho_azul = 0;

            atualizar_led_pwm(LED_VERMELHO, brilho_vermelho);
            atualizar_led_pwm(LED_AZUL, brilho_azul);
            gpio_put(LED_VERDE, 0);
        } 
        else {
            // Todos os LEDs desligados
            atualizar_led_pwm(LED_VERMELHO, 0);
            atualizar_led_pwm(LED_AZUL, 0);
            gpio_put(LED_VERDE, 0);
        }

        // Atualização do display OLED
        pos_x = 120 - ((adc_x * 50) / 4095 + 3);
        pos_y = (adc_y * 100) / 4095 + 3;
        ssd1306_fill(&ssd, false);
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);
        ssd1306_rect(&ssd, pos_x, pos_y + 6, 8, 8, true, true);
        ssd1306_send_data(&ssd);

        sleep_ms(100);
    }
}
