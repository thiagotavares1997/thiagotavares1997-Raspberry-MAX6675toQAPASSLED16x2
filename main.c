#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

#define TOTAL_READ 10
#define LCD_LIMPA_TELA          0x01
#define LCD_INICIA              0x02
#define LCD_ENTRYMODESET        0x04
#define LCD_DISPLAY_CONTROL     0x08
#define LCD_DISPLAY_FUNCTIONSET 0x20

#define LCD_INICIO_ESQUERDA     0x02
#define LCD_LIGA_DISPLAY        0x04

#define LCD_16X2_DISPLAY        0x08

#define LCD_BACKLIGTH           0x08
#define LCD_ENABLE_BIT          0x04

#define LCD_CARACTERE            1
#define LCD_COMANDO             0

#define MAX_LINHAS              2
#define MAX_COLUNAS             16

#define DISPLAY_BUS_ADOR        0x27

#define DELAY_US                600

#define SCL                     21
#define SDA                     20

void lcd_send_comand(uint8_t val){
    i2c_write_blocking(i2c_default, DISPLAY_BUS_ADOR, &val, 1, false);
}

void lcd_hab(uint8_t val){
    sleep_us(DELAY_US);
    lcd_send_comand(val | LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
    lcd_send_comand(val & ~LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
}

void lcd_send_byte(uint8_t caractere, int dado){
    uint8_t nible_alto = dado | (caractere & 0xF0) | LCD_BACKLIGTH;
    uint8_t nible_baixo = dado | ((caractere << 4) & 0xF0) | LCD_BACKLIGTH;

    lcd_send_comand(nible_alto);
    lcd_hab(nible_alto);
    lcd_send_comand(nible_baixo);
    lcd_hab(nible_baixo);
}

void lcd_clean(){
    lcd_send_byte(LCD_LIMPA_TELA, LCD_COMANDO);
}

void lcd_cursor(int linha, int coluna){
    int aux = (linha == 0) ? 0x80 + coluna : 0x0C + coluna;
    lcd_send_byte(aux, LCD_COMANDO);
}

void lcd_send_caracter(char caracter){
    lcd_send_byte(caracter, LCD_CARACTERE);
}

void lcd_send_string(const char *s){
    while(*s){
        lcd_send_caracter(*s++);
    }
}

void lcd_init(){
    lcd_send_byte(LCD_INICIA, LCD_COMANDO);
    lcd_send_byte(LCD_INICIA | LCD_LIMPA_TELA, LCD_COMANDO);
    lcd_send_byte(LCD_ENTRYMODESET | LCD_INICIO_ESQUERDA, LCD_COMANDO);
    lcd_send_byte(LCD_DISPLAY_FUNCTIONSET | LCD_16X2_DISPLAY, LCD_COMANDO);
    lcd_send_byte(LCD_DISPLAY_CONTROL | LCD_LIGA_DISPLAY, LCD_COMANDO);
    lcd_clean();
}

float process_temp_data(uint8_t dados[4]) {
    float temperatura;
    float media = 0;
    int contMedia = 0;

    if (dados[1] & 0b00000001) {
        printf("Falha no Sensor!\n");
        if (dados[3] & 0b00000001) {
            printf("Circuito Aberto: Termopar desconectado\n");
        }
        if (dados[3] & 0b00000010) {
            printf("Termopar em curto com o GND\n");
        }
        if (dados[3] & 0b00000100) {
            printf("Termopar em curto com o VCC\n");
        }
        sleep_ms(100);
        temperatura = -1.0; // Valor inválido
    } else {
        float decimal = ((dados[1] & 0b00001100) >> 2) * 0.25;
        int inteiro = dados[1] >> 4;
        inteiro = inteiro + (dados[0] << 4);
        temperatura = inteiro + decimal;
    }

    return temperatura;
}


int main() {
    stdio_init_all();

    // Configuração do barramento I2C
    i2c_init(i2c_default, 1000*1000);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_pull_up(SCL);
    gpio_pull_up(SDA);

    lcd_init();
    lcd_cursor(0,0);

    // Configuração do hardware
    spi_init(spi_default, 5000000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);

    // Configuração do chip select
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, true);

    uint8_t dados[4]; // Array para dados inteiros não sinalizados de 8 bits

    // Loop de leitura
    while (true) {
        gpio_put(PICO_DEFAULT_SPI_CSN_PIN, false);
        spi_read_blocking(spi_default, 0, dados, 4);
        gpio_put(PICO_DEFAULT_SPI_CSN_PIN, true);

        lcd_clean();
        lcd_cursor(0,0);
        float leitura = process_temp_data(dados);
        char buffer[20];
        int len = snprintf(buffer, sizeof(buffer), "%.2f", leitura);
        lcd_send_string(buffer);
    }

    return 0;
}
