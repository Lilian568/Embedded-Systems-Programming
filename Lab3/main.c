#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

#define AVE_TIMES 3
#define COMMON_SIZE 10
#define TOTAL_STEP 8
#define SIZE 4
#define ROTARY_SPEED 1

enum ON_HIGH_OFF_LOW_STATE {
    ON_HIGH = 1,
    OFF_LOW = 0,
};

enum PIN {
    ROTARY_A = 2,
    ROTARY_B = 3,
    ROTARY_C = 6,
    ROTARY_D = 13,
    LIGHT_ELECTRIC_PIN = 28
};

enum IS_CALIB {
    CALIB_TRUE = 1,
    CALIB_FALSE = 0
};

enum DIRECTION {
    CLOCKWISE = 1,
    ANTICLOCKWISE = 0
};


static uint is_calib_state = CALIB_FALSE;
static uint every_rotary_step = 0;
static int step = 0;
static uint rotary_arr[TOTAL_STEP][SIZE] = {
    {ON_HIGH, OFF_LOW, OFF_LOW, OFF_LOW},
    {ON_HIGH, ON_HIGH, OFF_LOW, OFF_LOW},
    {OFF_LOW, ON_HIGH, OFF_LOW, OFF_LOW},
    {OFF_LOW, ON_HIGH, ON_HIGH, OFF_LOW},
    {OFF_LOW, OFF_LOW, ON_HIGH, OFF_LOW},
    {OFF_LOW, OFF_LOW, ON_HIGH, ON_HIGH},
    {OFF_LOW, OFF_LOW, OFF_LOW, ON_HIGH},
    {ON_HIGH, OFF_LOW, OFF_LOW, ON_HIGH}
};

void change_line_to_end_symbol(char *str);

int read_str_by_fg(char *str, int count);

void init_gpio(int button_pin, int in_or_put, int is_pullup);

void next_step_for_direction(uint direction, int *step);

void rotary(int current_step);

void perform_rotation_steps(uint N, int direction, int current_step);

void get_status(uint is_calib_state);

void calibrate_motor_step_count(int *step, int direction, int *last_light_electric_status,
                                int *current_light_electric_status,
                                int *step_count);

void perform_calib(int *step, uint *every_rotary_step, uint *is_calib_state);


void init_gpios(void);

void read_common(char *common);

void del_buffer_area(void);

bool is_number(char *str);

bool judge_common_run_N(const char *common, const char *slip_material, char *slip_str, int *N);

int main() {
    //setbuf(stdout, NULL);
    //setbuf(stdin, NULL);
    // Initialize chosen serial port
    stdio_init_all();
    char common[COMMON_SIZE] = {0};
    char slip_material[] = " ";
    char *slip_str = NULL;
    int N = 0;

   init_gpios();

    read_common(common);


    // Loop forever
    while (true) {
        bool is_run_N = judge_common_run_N(common, slip_material, slip_str, &N);
        // fixed choose
        while (strcmp(common, "status") != 0 && strcmp(common, "calib") != 0 && is_run_N == false) {
            printf("ERROR: Please seriously depend tip to input if u want to get a good job !\n");
            read_common(common);
            is_run_N = judge_common_run_N(common, slip_material, slip_str, &N);
        }

        if (strcmp(common, "status") == 0) {
            get_status(is_calib_state);
        } else if (strcmp(common, "calib") == 0) {
            perform_calib(&step, &every_rotary_step, &is_calib_state);
            printf("calib done!\n");
        } else if (is_run_N) {
            if (every_rotary_step > 0) {
                perform_rotation_steps(N, CLOCKWISE, step);
            }else {
                printf("please calib firstly!!!\n");
            }
        }
        read_common(common);
    }
}

bool is_number(char *str) {
    if (str == NULL) return false;
    int num = 0;
    change_line_to_end_symbol(str);
    if (sscanf(str, "%d", &num) != 1) {
        return false;
    }
    char str2[10];
    snprintf(str2, sizeof(str2), "%d", num);
    if (strlen(str2) == strlen(str)) {
        return true;
    }
    return false;
}



bool judge_common_run_N(const char *common, const char *slip_material, char *slip_str, int *N) {
    char common_copy[COMMON_SIZE];
    strncpy(common_copy, common, COMMON_SIZE);
    common_copy[COMMON_SIZE - 1] = '\0';
    slip_str = strtok(common_copy, slip_material);
    if(strcmp(slip_str, "run") == 0) {
        slip_str = strtok(NULL, slip_material);
        if (slip_str == NULL) {
            *N = 8;
            return true;
        }
    }else {
        return false;
    }

    if(is_number(slip_str)) {
        *N = strtol(slip_str, NULL, 10);
        return true;
    }
    return false;
}

void change_line_to_end_symbol(char *str) {
    uint str_len = strlen(str);
    if (str_len > 0 && str[str_len - 1] == '\n' || str[str_len - 1] == '\r') {
        str[str_len - 1] = '\0';
    }
    if (str_len > 1 && str[str_len - 2] == '\r') {
        str[str_len - 2] = '\0';
    }
}

int read_str_by_fg(char *str, const int count) {
    fgets(str, count, stdin);
    change_line_to_end_symbol(str);

    if (!strcmp(str, "stop")) {
        return 0;
    }
    return 1;
}

void init_gpio(int button_pin, const int in_or_put, const int is_pullup) {
    gpio_init(button_pin);
    gpio_set_dir(button_pin, in_or_put);
    if (is_pullup) {
        gpio_pull_up(button_pin); //PULL UP DEFAULT:1 PRESS:0
    }
}


void next_step_for_direction(const uint direction, int *step) {
    if (direction == CLOCKWISE) {
        *step = (*step + 1) % 8;
    } else {
        *step = (*step - 1 + 8) % 8;
    }
}

void rotary(const int current_step) {
    gpio_put(ROTARY_A, rotary_arr[current_step][0]);
    gpio_put(ROTARY_B, rotary_arr[current_step][1]);
    gpio_put(ROTARY_C, rotary_arr[current_step][2]);
    gpio_put(ROTARY_D, rotary_arr[current_step][3]);
}

/**
 *
 * @param N * 4096/8  times
 * @param direction
 * @param current_step
 */
void perform_rotation_steps(uint N, const int direction, int current_step) {
    N = N * every_rotary_step / 8;
    for (int i = 0; i < N; i++) {
        rotary(current_step);
        next_step_for_direction(direction, &current_step);
        sleep_ms(ROTARY_SPEED);
    }
}

void calibrate_motor_step_count(int *step, int direction, int *last_light_electric_status,
                                int *current_light_electric_status,
                                int *step_count) {
    while (*last_light_electric_status != ON_HIGH || *current_light_electric_status != OFF_LOW) {
        rotary(*step);
        next_step_for_direction(direction, step);
        *last_light_electric_status = *current_light_electric_status;
        *current_light_electric_status = gpio_get(LIGHT_ELECTRIC_PIN);
        if (step_count != NULL) {
            (*step_count)++;
        }
        sleep_ms(ROTARY_SPEED);
    };
    *last_light_electric_status = *current_light_electric_status;
    *current_light_electric_status = gpio_get(LIGHT_ELECTRIC_PIN);
}

void get_status(const uint is_calib_state) {
    if (is_calib_state) {
        printf("status: every rotary number: %d\n", every_rotary_step);
    } else {
        fprintf(stderr, "status: not available\n");
    }
}

void perform_calib(int *step, uint *every_rotary_step, uint *is_calib_state) {
    int last_light_electric_status = OFF_LOW;
    int current_light_electric_status = gpio_get(LIGHT_ELECTRIC_PIN);
    int step_count = 0;
    calibrate_motor_step_count(step, CLOCKWISE, &last_light_electric_status, &current_light_electric_status,NULL);

    for (int i = 0; i < AVE_TIMES; ++i) {
        calibrate_motor_step_count(step, CLOCKWISE, &last_light_electric_status, &current_light_electric_status,
                                   &step_count);
    }
    *is_calib_state = CALIB_TRUE;
    *every_rotary_step = step_count / AVE_TIMES;
}


void init_gpios(void) {
    init_gpio(LIGHT_ELECTRIC_PIN, GPIO_IN, ON_HIGH);
    init_gpio(ROTARY_A, GPIO_OUT, OFF_LOW);
    init_gpio(ROTARY_B, GPIO_OUT, OFF_LOW);
    init_gpio(ROTARY_C, GPIO_OUT, OFF_LOW);
    init_gpio(ROTARY_D, GPIO_OUT, OFF_LOW);
}

void read_common(char *common) {
    memset(common, 0, COMMON_SIZE);
    printf("Input common<calib|run N>[N is number]: ");
    read_str_by_fg(common, COMMON_SIZE);
    change_line_to_end_symbol(common);
    printf("\n");
}
