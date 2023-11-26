/* i2c - Simple example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   See README.md file to get detailed usage of this example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include "initi2c.h"
#include "initBLE.h"
#include "scanBLE.h"
#include "esp_log.h"
#include "driver/i2c.h"

#define RED_BUTTON                  35
#define BLUE_BUTTON                  32

i2c_lcd1602_info_t *global_info;

const double BASE_FRAMEWORK_PROGRESS = 10;
const int BASE_ENERGY_INCREASE = 2;
const int BASE_ENERGY_DECREASE = 1;
const int MAX_ENERGY = 1000;
const int MAX_QUALITY = 5;
const int FRAMEWORK_CREATION_EXP = 500;
const int EXPERIENCE_QUALITY_MULTIPLIER = 10;

// Queue
#define MAX_QUEUE_SIZE 20
typedef struct {
    int array[MAX_QUEUE_SIZE];
    int last;
} Queue;



enum State {
    REPOSE,
    FATIGUE,
    EPUISE,
    HYPERACTIF,
    SOLITAIRE
};

typedef struct {
    enum State avatarState;

    // Level
    double level;
    double experience;
    double stateExperienceMultiplier; // Multiplier for the experience gain

    // Framework
    int framework; //Number of framework
    Queue frameworkLevel; // Circular queue to get an average of the last 20 framework levels
    double frameworkQualityMultiplier; // Multiplier for the framework quality
    double frameworkSpeedMultiplier; // Multiplier for the framework speed
    double activeFrameworkProgress; // Progress of the active framework
    int lastFrameworkLevel; // Last framework level

    // Proximity
    int lastProximity; // Last time proximity was detected
    bool isProximityActive; // Is proximity active

    // Energy
    int energy;
    int energyMultiplier; // Number of time energy drink was given
    int energyIncrease; // Amount of energy to increase
    int lastEnergyIncrease; // Last time energy was increased
} Game;

int enqueue(Queue *queue, int value);
void initQueue(Queue *queue);
float get_framework_average(Queue *queue);
void initGame(Game *game);
void get_avatar_state(Game *game);
void set_proximity(Game *game, bool isProximityActive);

void initGame(Game *game) {
    game->avatarState = REPOSE;
    game->energy = 1000;

    game->level = 0;
    game->experience = 0;
    game->stateExperienceMultiplier = 1;

    initQueue(&(game->frameworkLevel));

    game->framework = 0;
    game->frameworkQualityMultiplier = 1;
    game->frameworkSpeedMultiplier = 1;
    game->activeFrameworkProgress = 0;
    game->lastFrameworkLevel = -1;

    time_t currentTime;
    time(&currentTime);

    game->lastProximity = currentTime;
    game->isProximityActive = false;
    game->energyMultiplier = 1;
    game->energyIncrease = 0;
    game->lastEnergyIncrease = currentTime;
}

// constructor
void initQueue(Queue *queue) {
    queue->last = -1;
}

// Circular queue to get an average of the last 20 framework levels
int enqueue(Queue *queue, int value) {
    queue->last = (queue->last + 1) % MAX_QUEUE_SIZE;
    queue->array[queue->last] = value;

    return 1;
}

// Get the average of the last 20 framework levels
float get_framework_average(Queue *queue) {
    if (queue->last < MAX_QUEUE_SIZE) {
        return 0;
    }

    int sum = 0;
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        sum += queue->array[i];
    }

    return sum / MAX_QUEUE_SIZE;
}

void img_home() {
    lcd_move_cursor(global_info, 0, 0);
}

void img_line() {
    lcd_move_cursor(global_info, 0, 1);
}

void txt_home() {
    lcd_move_cursor(global_info, 5, 0);
}

void txt_line() {
    lcd_move_cursor(global_info, 5, 1);
}

void buttonTest(void *pParam) {
    bool buttonPressed = false;
    int count = 998;
    char pointeurStr[50] = "";
    lcd_clear(global_info);
    lcd_write(global_info, "Button not pressed");

    while (1) {
        if (gpio_get_level(RED_BUTTON) == 1 && !buttonPressed) {
            count++;
            lcd_clear(global_info);
            sprintf(pointeurStr, "%d times", count);
            lcd_write(global_info, pointeurStr);
            txt_line();
            lcd_write(global_info, "Coucou :)");
        }
        if (gpio_get_level(BLUE_BUTTON) == 1 && !buttonPressed) {
            count--;
            lcd_clear(global_info);
            sprintf(pointeurStr, "%d times", count);
            lcd_write(global_info, pointeurStr);
            txt_line();
            lcd_write(global_info, "Coucou :)");
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void setupIO() {
    gpio_set_direction(RED_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(BLUE_BUTTON, GPIO_MODE_INPUT);
}

// TODO: Add proper values
void get_avatar_state(Game *game) {
    if (game->energy < 100) {
        game->avatarState = FATIGUE;
    } else if (game->energy < 50) {
        game->avatarState = EPUISE;
    } else if (game->energy > 1000) {
        game->avatarState = HYPERACTIF;
    } else if (game->lastProximity < 1000) {
        game->avatarState = SOLITAIRE;
    } else {
        game->avatarState = REPOSE;
    }
}


void set_proximity(Game *game, bool isProximityActive) {
    game->isProximityActive = isProximityActive;

    if (!isProximityActive)
    {
        return;
    }

    time_t currentTime;
    time(&currentTime);

    if (isProximityActive) {
        game->lastProximity = currentTime;
    }
}


// Manage energy increase and decrease
void progress_energy(Game *game) {
    if (game->energyIncrease > 0) {
        int energyIncrease = BASE_ENERGY_INCREASE * game->energyMultiplier;

        if (game->energyIncrease < energyIncrease) {
            energyIncrease = game->energyIncrease;
            game->energyIncrease = 0;
        } else {
            game->energyIncrease -= energyIncrease;
        }

        if (game->energy + energyIncrease > MAX_ENERGY) {
            game->energy = MAX_ENERGY;
        } else {
            game->energy += energyIncrease;
        }

        if (game->energyIncrease == 0) {
            game->energyMultiplier = 1;
        }
    } else {
        game->energy -= BASE_ENERGY_DECREASE;
    }
}

void drink_energy(Game *game) {
    time_t currentTime;
    time(&currentTime);

    game->energyMultiplier++;
    game->lastEnergyIncrease = currentTime;

    if (game->energyIncrease + 200 > MAX_ENERGY) {
        game->energyIncrease = MAX_ENERGY;
    } else {
        game->energyIncrease += 200;
    }
}



char* get_current_time() {
    time_t currentTime;
    time(&currentTime);

    char* timeString = (char*)malloc(6);

    struct tm *localTime = localtime(&currentTime);
    sprintf(timeString, "%02d:%02d", localTime->tm_hour, localTime->tm_min);

    return timeString;
}



int get_next_level_experience(int level) {
    return 100 * pow(level, 1.5);
}

void add_experience(Game *game, int quality) {
    game->experience += quality * EXPERIENCE_QUALITY_MULTIPLIER * game->stateExperienceMultiplier;

    if (game->experience >= get_next_level_experience(game->level)) {
        game->level++;
        game->experience = 0;
    }
}



void add_framework(Game *game) {
    game->framework++;
    game->activeFrameworkProgress = 0;

    int quality = ((rand() % MAX_QUALITY) + 1) * game->frameworkQualityMultiplier;

    if (quality > MAX_QUALITY) {
        quality = MAX_QUALITY;
    }

    enqueue(&(game->frameworkLevel), quality);
    game->lastFrameworkLevel = quality;

    add_experience(game, quality);
}

void progress_framework(Game *game) {
    double progress = BASE_FRAMEWORK_PROGRESS * game->frameworkSpeedMultiplier + game->level;
    game->activeFrameworkProgress += (int)progress;

    if (game->activeFrameworkProgress >= FRAMEWORK_CREATION_EXP) {
        add_framework(game);
    }
}

// TODO: Add proper values
void get_framework_multipliers(Game *game) {
    switch (game->avatarState) {
        case REPOSE:
            game->frameworkQualityMultiplier = 1;
            game->frameworkSpeedMultiplier = 1;
            game->stateExperienceMultiplier = 3;
            break;
        case FATIGUE:
            game->frameworkQualityMultiplier = 0.5;
            game->frameworkSpeedMultiplier = 0.5;
            game->stateExperienceMultiplier = 1;
            break;
        case EPUISE:
            game->frameworkQualityMultiplier = 0.25;
            game->frameworkSpeedMultiplier = 0.25;
            game->stateExperienceMultiplier = 0.5;
            break;
        case HYPERACTIF:
            game->frameworkQualityMultiplier = 1.5;
            game->frameworkSpeedMultiplier = 1.5;
            game->stateExperienceMultiplier = 2;
            break;
        case SOLITAIRE:
            game->frameworkQualityMultiplier = 0.75;
            game->frameworkSpeedMultiplier = 0.75;
            game->stateExperienceMultiplier = 0.75;
            break;
    }
}

void update_screen(Game *game)
{
    printf("Framework Count: %d\n", game->framework);
    printf("Framework Progress: %d / 500\n", game->activeFrameworkProgress);
    printf("Energy value: %d\n", game->energy);
    printf("Experience value: %d\n", game->experience);
    printf("Level value: %f\n", game->level);
    printf("State value: %d\n", game->avatarState);
    printf("Framework quality: %d\n", game->lastFrameworkLevel);
    printf("Time %s\n", get_current_time());
}

void game_loop(Game *game) {
    while (true) {
        get_avatar_state(game);
        get_framework_multipliers(game);

        progress_energy(game);
        progress_framework(game);

        update_screen(game);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void hardware_loop(Game *game) {
    while (true) {
        printf("\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    int i2c_master_port = 0;
    i2c_config_t conf = {
            .mode = 1,
            .sda_io_num = I2C_MASTER_SDA_IO,         // select GPIO specific to your project
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_MASTER_SCL_IO,         // select GPIO specific to your project
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
            // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };

    setupIO();
    ble_init();
    init_SCAN();

    i2c_lcd1602_info_t *i2c_lcd1602_info = i2c_lcd1602_malloc();
    global_info = i2c_lcd1602_info;
    smbus_info_t *smbus_info = malloc(sizeof(smbus_info_t));
    smbus_init(smbus_info, 0, MPU9250_SENSOR_ADDR);

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(smbus_info->i2c_port, 1, 0, 0, ESP_INTR_FLAG_SHARED);
    i2c_master_start(i2c_cmd_link_create());

    uint8_t data[2];
    i2c_lcd1602_init(i2c_lcd1602_info, smbus_info, true, 2, 16, 16);
    lcd_write(i2c_lcd1602_info, "Hello Maxime!");
    lcd_home(i2c_lcd1602_info);
    lcd_clear(i2c_lcd1602_info);
    lcd_write(i2c_lcd1602_info, "Hello test3");
    img_line();
    uint8_t charset[] = {0x0E, 0x0A, 0x0E, 0x0A, 0x0A, 0x0A, 0x1F, 0x1B};
    lcd_define_char(i2c_lcd1602_info, 1, charset);

    uint8_t charset2[] = {0x0A, 0x15, 0x15, 0x11, 0x11, 0x0A, 0x04, 0x00};
    lcd_define_char(i2c_lcd1602_info, 2, charset2);


    lcd_move_cursor(i2c_lcd1602_info, 1, 1);
    _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_1);
    _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_2);

    // xTaskCreate(buttonTest, "buttonTest", 2048, NULL, 10, NULL);

    Game game;
    initGame(&game);


    xTaskCreate(game_loop, "game_loop", 4096, (void *) &game, 1, NULL);
    xTaskCreate(hardware_loop, "hardware_loop", 4096, (void *) &game, 1, NULL);
}
