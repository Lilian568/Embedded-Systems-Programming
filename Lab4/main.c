#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define UART_ID uart1
#define BAUD_RATE 9600
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define SW0_PIN 9

char uart_buffer[64];

void uart_init_custom() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

bool read_line_with_timeout(uint32_t timeout_us) {
    int pos = 0;
    absolute_time_t end_time = make_timeout_time_us(timeout_us);

    while (absolute_time_diff_us(get_absolute_time(), end_time) > 0) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            if (c == '\n' || c == '\r') {
                if (pos > 0) {
                    uart_buffer[pos] = '\0';
                    return true;
                }
            } else if (pos < sizeof(uart_buffer) - 1) {
                uart_buffer[pos++] = c;
            }
        }
    }
    uart_buffer[pos] = '\0';
    return false;
}

int uart_send_command(const char *cmd, int timeout_ms) {
    uart_puts(UART_ID, cmd);
    uart_puts(UART_ID, "\r\n");
    return read_line_with_timeout(timeout_ms * 1000) ? 0 : -1;
}

void button_wait() {
    printf("Please press SW0\n");
    while (gpio_get(SW0_PIN)) {
        sleep_ms(20);
    }
    while (!gpio_get(SW0_PIN)) {
        sleep_ms(20);
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    gpio_init(SW0_PIN);
    gpio_set_dir(SW0_PIN, GPIO_IN);
    gpio_pull_up(SW0_PIN);

    uart_init_custom();

    int state = 1;

    while (true) {
        switch (state) {
            case 1:  // Wait for button press
                button_wait();
                state = 2;
                break;

            case 2: {  // Send AT command
                int attempts = 0;
                while (attempts < 5) {
                    if (uart_send_command("AT", 500) == 0 && strstr(uart_buffer, "OK")) {
                        printf("Connected to LoRa module\n");
                        state = 3;
                        break;
                    }
                    attempts++;
                    sleep_ms(100);
                }
                if (attempts == 5) {
                    printf("Module not responding\n");
                    state = 1;
                }
                break;
            }

            case 3:  // Read firmware version
                if (uart_send_command("AT+VER", 500) == 0) {
                    printf("Firmware version: %s\n", uart_buffer);
                    state = 4;
                } else {
                    printf("Module stopped responding\n");
                    state = 1;
                }
                break;

            case 4: {  // Read DevEui
                if (uart_send_command("AT+ID=DevEui", 500) == 0) {
                    printf("Raw DevEui: %s\n", uart_buffer);
                    char *eui_start = strstr(uart_buffer, "DevEui, ");
                    if (eui_start) {
                        eui_start += 8;  // Move past "DevEui, "
                        char processed[17] = {0};  // Ensure null-termination
                        int j = 0;
                        for (int i = 0; eui_start[i] != '\0' && j < 16; i++) {
                            if (isxdigit((unsigned char)eui_start[i])) {
                                processed[j++] = tolower(eui_start[i]);
                            }
                        }
                        if (j == 16) {
                            printf("%s\n", processed);
                        }
                    }
                    state = 1;
                } else {
                    printf("Module stopped responding\n");
                    state = 1;
                }
                break;
            }
        }
    }

    return 0;
}
