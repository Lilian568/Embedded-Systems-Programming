#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pico/util/queue.h>
#include <sys/unistd.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

enum I2C_INFO {
    I2C_SDA = 16,
    I2C_SCL = 17,
    BAUD_RATE = 400 * 1000,
    EEPROM_DEVICE_ADDR = 0b1010000
};

#define MEM_ADDR {0x7F, 0xFF - 1} //pay attention to the address

#define I2C_PORT i2c0
#define WRITE_BUF_SIZE 64
#define ADDR_BUFF_SIZE 2
#define INIT_LED_STATE 0b010
#define ON_HIGH 1
#define OFF_LOW 0
#define LOG_LIMIT_NUMBER 32  //2048/64
#define LOG_SIZE 64

enum LED_PIN_AND_BUTTON_PIN {
    LED_PIN_D1 = 20,
    LED_PIN_D2 = 21,
    LED_PIN_D3 = 22,
    BUTTON_PIN_SW0 = 9,
    BUTTON_PIN_SW1 = 8,
    BUTTON_PIN_SW2 = 7,
};

typedef struct led_state {
    uint8_t state;
    uint8_t not_state;
} led_state_D1_D2_D3;


typedef struct log {
    uint8_t read_log_buf[64];
    uint8_t log_mem_addr[2];
    uint8_t log_data[64];
    uint8_t log_CRC_high;
    uint8_t log_CRC_low;
    int current_log_num;
    int full_clean;
} Log;


static queue_t events;
volatile static uint button_last_time = 0;
const static uint button_time_interval = 300;
static bool button_pressed = false;
static uint64_t start_time_point = 0;
static uint8_t read_buf[64] = {0};


int i2c_write(const uint8_t *addr_buf, const uint8_t *data, const int data_len, bool no_stop) {
    uint8_t write_buf[WRITE_BUF_SIZE] = {0};
    memcpy(write_buf, addr_buf, ADDR_BUFF_SIZE);
    memcpy(&write_buf[ADDR_BUFF_SIZE], data, data_len);
    return i2c_write_blocking(I2C_PORT, EEPROM_DEVICE_ADDR, write_buf, data_len + 2, no_stop);
}

int i2c_read(const uint8_t *addr_buf, uint8_t *read_buf, const int data_len) {
    memset(read_buf, 0, 64);
    i2c_write_blocking(I2C_PORT, EEPROM_DEVICE_ADDR, addr_buf, ADDR_BUFF_SIZE, true);
    // printf("read_write: %d\n", read_write);
    return i2c_read_blocking(I2C_PORT, EEPROM_DEVICE_ADDR, read_buf, data_len, false);
}

uint16_t crc16(const uint8_t *data_p, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;
    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t) (x << 12)) ^ ((uint16_t) (x << 5)) ^ ((uint16_t) x);
    }
    return crc;
}


//renew the LED state
void set_led_state(led_state_D1_D2_D3 *ls, const uint8_t value) {
    ls->state = value;
    ls->not_state = ~value;
}

bool led_state_is_valid(const led_state_D1_D2_D3 *ls) {
    return ls->state == (uint8_t) ~ls->not_state;
}

// 0 is invalid, 1 is valid
bool log_data_check(Log *log) {
    // first byte is 0x00
    if (log->read_log_buf[0] == 0x00) {
        return false;
    }
    // the log have '\0'
    char *result = strchr((char *) log->read_log_buf, '\0');
    if (result == NULL) {
        return false;
    }
    int end_index = result - (char *) log->read_log_buf;
    if (end_index >= 62) {
        return false;
    }

    int i = crc16(log->read_log_buf, end_index+3);
    // CRC check
    if (i != 0) {
        return false;
    }

    return true;
}

void format_addr(Log *log, int current_log_num) {
    uint16_t address = current_log_num * 64;
    log->log_mem_addr[0] = (uint8_t) ((address >> 8) & 0xFF);
    log->log_mem_addr[1] = (uint8_t) (address & 0xFF);
}

//find the last log number
int find_log_last_point_num(Log *log) {
    for (int current_log_num = 0; current_log_num < LOG_LIMIT_NUMBER; ++current_log_num) {
        memset(log->read_log_buf, 0, 64);
        format_addr(log, current_log_num);
        i2c_read(log->log_mem_addr, log->read_log_buf, LOG_SIZE);
        if (!log_data_check(log)) {
            log->current_log_num = current_log_num;
            return current_log_num;
        }
    }
    return -1;
}


void clean_log_mem(Log *log) {
    if (log->current_log_num == -1) {
        log->current_log_num = LOG_LIMIT_NUMBER;
    }
    uint8_t init_data[LOG_SIZE] = {0x00};
    for (int current_log_num = 0; current_log_num < log->current_log_num; ++current_log_num) {
        format_addr(log, current_log_num);
        i2c_write(log->log_mem_addr, init_data, sizeof(init_data),false);
        sleep_ms(5);
    }
}


void init_I2C(void) {
    i2c_init(I2C_PORT, BAUD_RATE);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void gpio_button_init(const int button_pin, const int in_or_put) {
    gpio_init(button_pin);
    gpio_set_dir(button_pin, in_or_put);
    gpio_pull_up(button_pin); //PULL UP DEFAULT:1 PRESS:0
}

void led_pin_init(const int pin, const int in_or_put) {
    gpio_init(pin);
    gpio_set_dir(pin, in_or_put);
}

static void gpio_handler(uint gpio, uint32_t event_mask) {
    uint current_time = to_ms_since_boot(get_absolute_time());
    if (event_mask & GPIO_IRQ_EDGE_FALL && (current_time - button_last_time >= button_time_interval)) {
        queue_try_add(&events, &gpio);
        button_pressed = true;
        button_last_time = current_time;
    }
}

void init_LED_BUTTON() {
    for (int i = 0; i < 3; ++i) {
        // Initialize LED pin
        led_pin_init(LED_PIN_D1 + i, GPIO_OUT);
        // init button
        gpio_button_init(BUTTON_PIN_SW2 + i, GPIO_IN);
        gpio_set_irq_enabled_with_callback(BUTTON_PIN_SW2 + i, GPIO_IRQ_EDGE_FALL, true, &gpio_handler);
    }
}

void to_binary(uint8_t val, uint8_t *state_arr) {
    for (int i = 0; i < 3; ++i) {
        state_arr[i] = ((val & (1 << i)) ? 1 : 0);
    }
}

void led_on_off(const uint8_t *state_arr) {
    for (int i = 0; i < 3; ++i) {
        gpio_put(LED_PIN_D1 + i, state_arr[i]);
    }
}

void set_log_data_CRC(Log *log) {
    int data_len = strlen(log->log_data);
    log->log_data[data_len] = '\0';

    uint16_t crc = crc16(log->log_data, data_len+1);
    log->log_CRC_high = (uint8_t) (crc >> 8);
    log->log_CRC_low = (uint8_t) crc;

    log->log_data[data_len + 1] = log->log_CRC_high;
    log->log_data[data_len + 2] = log->log_CRC_low;
}

void write_log(Log *log) {
    // check does it full
    log->current_log_num = find_log_last_point_num(log);
    if (log->current_log_num == -1) {
        clean_log_mem(log);
        log->current_log_num = 0x00;
    }
    format_addr(log, log->current_log_num);
    set_log_data_CRC(log);
    int write_num = i2c_write(log->log_mem_addr, log->log_data, strlen((const char *) log->log_data) + 3, false);
    // printf("write_num: %d\n", write_num);
    if (write_num < 0) {
        printf("write error\n");
    }
}

void get_time_from_start_to_current(const uint8_t *led_state_arr, Log *log) {
    double used_time = (time_us_64() - start_time_point) / 1000000.0;
    memset(log->log_data, 0, 64);
    snprintf((char *) log->log_data, sizeof(log->log_data),
             "D3D2D1: %d%d%d\ntime: %.2fs",
             led_state_arr[0], led_state_arr[1], led_state_arr[2],
             used_time);

    write_log(log);

    // printf("used_time: %.6f seconds\n", used_time);
    // printf("led state: D3:%d, D2:%d, D1:%d\n", led_state_arr[0], led_state_arr[1], led_state_arr[2]);
}

void print_all_logs(Log *log, size_t total_size, size_t log_size) {
    size_t log_count = total_size / log_size;

    for (size_t log_index = 0; log_index < log_count; ++log_index) {
        format_addr(log, log_index);
        i2c_read(log->log_mem_addr, log->read_log_buf, LOG_SIZE);

        printf("Log %zu:\n", log_index + 1);

        for (size_t i = 0; i < log_size && log->read_log_buf[i] != '\0'; ++i) {
            if (log->read_log_buf[i] == ';') {
                printf("\n");
            } else {
                printf("%c", log->read_log_buf[i]);
            }
        }

        printf("\n");
    }
}

void handle_log_command(const char *command, Log *log) {
    if (strcmp(command, "read") == 0) {
        log->current_log_num = find_log_last_point_num(log);
        if (log->current_log_num == 0) {
            printf("no logs\n");
            return;
        }
        print_all_logs(log, log->current_log_num * LOG_SIZE, LOG_SIZE);
    } else if (strcmp(command, "erase") == 0) {
        printf("cleaning...\n");
        clean_log_mem(log);
        printf("cleaned\n");
    } else {
        printf("Error, please commend<read|erase> again: %s", command);
    }
}

int main() {
    Log log = {0};;
    queue_init(&events, sizeof(int), 32);
    led_state_D1_D2_D3 led_states = {0};
    start_time_point = time_us_64();
    uint8_t mem_addr[] = MEM_ADDR;
    uint8_t led_state_arr[3] = {0};
    uint8_t data[3] = {0};
    int gpio_pin = 0;
    int rd = 0;
    int wt = 0;
    char input_buffer[32] = {0};
    size_t input_pos = 0;

    // Initialize chosen serial port
    stdio_init_all();
    init_I2C();

    strncpy(log.log_data, "Boot", 4);
    write_log(&log);

    init_LED_BUTTON();
    sleep_ms(500);

    //init led state from eeprom
    rd = i2c_read(mem_addr, read_buf, 3);
    if (rd) {
        led_states.state = read_buf[0];
        led_states.not_state = read_buf[1];
        if (led_state_is_valid(&led_states)) {
            to_binary(led_states.state, led_state_arr);
        } else {
            to_binary(INIT_LED_STATE, led_state_arr);
        }
        led_on_off(led_state_arr);
        get_time_from_start_to_current(led_state_arr, &log);

        busy_wait_ms(100);
    } else {
        printf("read error\n");
    }

    printf("Do not input other things if u want you want");
    printf("\nplease commend<read|erase>: ");
    while (1) {
        int ch = getchar_timeout_us(0); // 0 no block
        if (ch != PICO_ERROR_TIMEOUT) {
            if (ch == '\n') {
                input_buffer[input_pos] = '\0';
                handle_log_command(input_buffer, &log);
                input_pos = 0;
                memset(input_buffer, 0, sizeof(input_buffer));
                printf("\nplease commend<read|erase>: ");
            } else if (input_pos < sizeof(input_buffer) - 1 && ch != '\r') {
                input_buffer[input_pos++] = ch;
            }
        }
        while (queue_try_remove(&events, &gpio_pin)) {
            if (gpio_pin == BUTTON_PIN_SW0) {
                led_state_arr[0] = !gpio_get(LED_PIN_D1);
            } else if (gpio_pin == BUTTON_PIN_SW1) {
                led_state_arr[1] = !gpio_get(LED_PIN_D2);
            } else if (gpio_pin == BUTTON_PIN_SW2) {
                led_state_arr[2] = !gpio_get(LED_PIN_D3);
            }
            set_led_state(&led_states, (led_state_arr[2] << 2) | (led_state_arr[1] << 1) | led_state_arr[0]);
            led_on_off(led_state_arr);
            get_time_from_start_to_current(led_state_arr, &log);
            data[0] = led_states.state;
            data[1] = led_states.not_state;
            busy_wait_ms(200);
            i2c_write(mem_addr, data, sizeof(data),false);
        }
    }
}
