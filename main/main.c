
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <sys/time.h>

#include "initi2c.h"
#include "initBLE.h"
#include "scanBLE.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "characters.h"

#define NEXT 35
#define ENERGY 32
#define SKREEEEEEEE 25
#define LIGHT 19
#define SENSOR 14
#define TAG "SELF_LOOP"
#define MAX_DISTANCE 400 // Maximum sensor distance is rated at 400-500cm.
float timeOut = MAX_DISTANCE * 60;
int soundVelocity = 340; // define sound speed=340m/s

i2c_lcd1602_info_t *global_info;

const int BASE_FRAMEWORK_PROGRESS = 10;
const int BASE_ENERGY_INCREASE = 3;
const int BASE_ENERGY_DECREASE = 1;
const int MAX_ENERGY = 1000;
const int MAX_QUALITY = 5;
const int FRAMEWORK_CREATION_EXP = 500;
const int EXPERIENCE_QUALITY_MULTIPLIER = 10;

static const i2c_lcd1602_info_t *i2c_lcd1602_info;
static bool light_on = false;
static bool sensor = false;

// Queue
#define MAX_QUEUE_SIZE 20

#define REPOSE 0
#define FATIGUE 1
#define EPUISE 2
#define HYPERACTIF 3
#define SOLITAIRE 4

typedef struct {
    int array[MAX_QUEUE_SIZE];
    int last;
} Queue;

typedef struct Game {
    int avatarState;
    int activeTab;

    // Level
    int level;
    int experience;
    double stateExperienceMultiplier; // Multiplier for the experience gain

    // Framework
    int framework; //Number of framework
    double frameworkQualityMultiplier; // Multiplier for the framework quality
    double frameworkSpeedMultiplier; // Multiplier for the framework speed
    int activeFrameworkProgress; // Progress of the active framework
    int lastFrameworkLevel; // Last framework level

    // Proximity
    int lastProximity; // Last time proximity was detected
    bool isProximityActive; // Is proximity active

    // Social
    int social; // Number of social interactions
    int lastSocial; // Last time social interaction was detected
    double socialMultiplier; // Multiplier for the social interaction
    bool isCrying; // Is crying

    // Energy
    int energy;
    int energyMultiplier; // Number of time energy drink was given
    int energyIncrease; // Amount of energy to increase
    int lastEnergyIncrease; // Last time energy was increased
} Game;

typedef struct GameData {
    int lastCrying; // Last time crying was executed
} GameData;

Game initializeGame() {
    Game newGame;

    time_t currentTime;
    time(&currentTime);

    // Set default values
    newGame.avatarState = 0;  // Set to the default avatar state
    newGame.activeTab = 1;  // Set to the default tab

    // Initialize other members with default values
    newGame.level = 1;
    newGame.experience = 0;
    newGame.stateExperienceMultiplier = 1.0;

    newGame.framework = 0;
    newGame.frameworkQualityMultiplier = 1.0;
    newGame.frameworkSpeedMultiplier = 1.0;
    newGame.activeFrameworkProgress = 0;
    newGame.lastFrameworkLevel = -1;

    newGame.lastProximity = currentTime;
    newGame.isProximityActive = false;

    newGame.social = 0;
    newGame.lastSocial = currentTime;
    newGame.socialMultiplier = 1.0;
    newGame.isCrying = false;

    newGame.energy = 1000;  // Set an initial energy value
    newGame.energyMultiplier = 1;
    newGame.energyIncrease = 0;
    newGame.lastEnergyIncrease = currentTime;

    return newGame;
}

void initQueue(Queue *queue) {
    queue->last = -1;

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue->array[i] = 0;
    }
}

void initializeLCD() {
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

    i2c_lcd1602_info = i2c_lcd1602_malloc();
    global_info = i2c_lcd1602_info;

    smbus_info_t *smbus_info = malloc(sizeof(smbus_info_t));
    smbus_init(smbus_info, 0, MPU9250_SENSOR_ADDR);

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(smbus_info->i2c_port, 1, 0, 0, ESP_INTR_FLAG_SHARED);
    i2c_master_start(i2c_cmd_link_create());

    uint8_t data[2];
    i2c_lcd1602_init(i2c_lcd1602_info, smbus_info, true, 2, 16, 16);
}

// Circular queue to get an average of the last 20 framework levels
int enqueue(Queue *queue, int value) {
    queue->last = (queue->last + 1) % MAX_QUEUE_SIZE;
    queue->array[queue->last] = value;

    return 1;
}

void img_home() {
    lcd_move_cursor(global_info, 0, 0);
}

void img_line() {
    lcd_move_cursor(global_info, 0, 1);
}

void txt_home() {
    lcd_move_cursor(global_info, 0, 0);
}

void txt_line() {
    lcd_move_cursor(global_info, 0, 1);
}

void setupIO() {
    gpio_set_direction(NEXT, GPIO_MODE_INPUT);
    gpio_set_direction(ENERGY, GPIO_MODE_INPUT);
    gpio_set_direction(SKREEEEEEEE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LIGHT, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENSOR, GPIO_MODE_INPUT);
}

void set_state_custom_char(int state) {

    switch (state) {
        case REPOSE: {
            lcd_define_char(i2c_lcd1602_info, 1, charStateRepose1);
            lcd_define_char(i2c_lcd1602_info, 2, charStateRepose2);
            break;
        }
        case FATIGUE: {
            lcd_define_char(i2c_lcd1602_info, 1, charStateTired1);
            lcd_define_char(i2c_lcd1602_info, 2, charStateTired2);
            break;
        }
        case EPUISE: {
            lcd_define_char(i2c_lcd1602_info, 1, charStateWornOut1);
            lcd_define_char(i2c_lcd1602_info, 2, charStateWornOut2);
            break;
        }
        case HYPERACTIF: {
            lcd_define_char(i2c_lcd1602_info, 1, charStateHyperactive1);
            lcd_define_char(i2c_lcd1602_info, 2, charStateHyperactive2);
            break;
        }
        case SOLITAIRE: {
            lcd_define_char(i2c_lcd1602_info, 1, charStateAlone1);
            lcd_define_char(i2c_lcd1602_info, 2, charStateAlone2);
            break;
        }
    }
}

// TODO: Add proper values
void get_avatar_state(Game *game) {
    if (game->energy < 50) {
        game->avatarState = EPUISE;
    } else if (game->energy < 300) {
        game->avatarState = FATIGUE;
    } else if (game->energy > 900) {
        game->avatarState = HYPERACTIF;
    } else if (game->lastProximity > 1000) {
        game->avatarState = SOLITAIRE;
    } else {
        game->avatarState = REPOSE;
    }

    set_state_custom_char(game->avatarState);
}

// Get the average of the last 20 framework levels
int get_framework_average(Queue *queue) {
    int sum = 0;
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        sum += queue->array[i];
    }

    return sum / MAX_QUEUE_SIZE;
}

void set_proximity(Game *game, bool isProximityActive) {
    game->isProximityActive = isProximityActive;

    if (!isProximityActive) {
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
        if (game->energy <= 1000 && game->energy > 0) {
            game->energy -= BASE_ENERGY_DECREASE;
        }
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


/* TODO - Romain: Add wifi to sync time. It might only be needed for the initialisation of the device. If it's the case
    put it in main before any initialization */
char *get_current_time() {
    time_t currentTime;
    time(&currentTime);

    char *timeString = (char *) malloc(6);

    struct tm *localTime = localtime(&currentTime);
    sprintf(timeString, "%02d:%02d", localTime->tm_hour, localTime->tm_min);

    return timeString;
}


void set_is_crying(Game *game) {

    if (game->socialMultiplier > 0.5) {
        game->isCrying = false;
    } else {
        game->isCrying = true;
    }
}

// TODO: Add proper values
void set_social_multiplier(Game *game) {
    time_t currentTime;
    time(&currentTime);

    if (game->social > 0) {
        game->socialMultiplier = 1.5 * (double) game->social;
    } else if (currentTime - game->lastSocial < 60) {
        game->socialMultiplier = 1.5;
    } else if (currentTime - game->lastSocial < 120) {
        game->socialMultiplier = 1.25;
    } else if (currentTime - game->lastSocial < 180) {
        game->socialMultiplier = 1.0;
    } else if (currentTime - game->lastSocial < 240) {
        game->socialMultiplier = 0.75;
    } else if (currentTime - game->lastSocial < 300) {
        game->socialMultiplier = 0.5;
    } else {
        game->socialMultiplier = 0.25;
    }

    set_is_crying(game);
}


void set_social(Game *game, int quantity) {
    if (quantity <= 0) {
        return;
    }

    time_t currentTime;
    time(&currentTime);

    game->lastSocial = currentTime;
    game->social = quantity;

    set_social_multiplier(game);
}

// Is used when the avatar feel alone and is crying
void hardware_cry(Game *game, GameData *gameData) {
    if (game->isCrying) {
        time_t currentTime;
        time(&currentTime);

        if (currentTime - gameData->lastCrying > 30) {
            gameData->lastCrying = currentTime;
            gpio_set_level(SKREEEEEEEE, 1);
            vTaskDelay(900 / portTICK_PERIOD_MS);
            gpio_set_level(SKREEEEEEEE, 0);
        } else {
            gpio_set_level(SKREEEEEEEE, 0);
        }
    }
}

void hardware_low_energy(Game *game) {
    if (game->energy < 300 && !light_on) {
        ESP_LOGW(TAG, "Light on");
        gpio_set_level(LIGHT, 1);
        light_on = true;
    }
    if (game->energy >= 300 && light_on) {
        ESP_LOGW(TAG, "Light off");
        gpio_set_level(LIGHT, 0);
        light_on = false;
    }
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


void add_framework(Game *game, Queue *frameworkQuality) {
    game->framework++;
    game->activeFrameworkProgress = 0;

    int quality = ((rand() % MAX_QUALITY) + 1) * game->frameworkQualityMultiplier;

    if (quality > MAX_QUALITY) {
        quality = MAX_QUALITY;
    }

    enqueue(frameworkQuality, quality);
    game->lastFrameworkLevel = quality;

    add_experience(game, quality);
}

void progress_framework(Game *game, Queue *frameworkQuality) {
    double progress = BASE_FRAMEWORK_PROGRESS * game->frameworkSpeedMultiplier + game->level;
    game->activeFrameworkProgress += (int) progress;

    if (game->activeFrameworkProgress >= FRAMEWORK_CREATION_EXP) {
        add_framework(game, frameworkQuality);
    }
}


void hardware_display_animation_header(int frame, char *str1, char *str2) {
    lcd_move_cursor(i2c_lcd1602_info, 11, 0);
    if (frame == 1) {
        lcd_write(i2c_lcd1602_info, str1);
    } else {
        lcd_write(i2c_lcd1602_info, str2);
    }
}

void hardware_display_animation(int frame, Game *game) {
    lcd_move_cursor(i2c_lcd1602_info, 12, 1);
    if (frame == 1) {
        _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_1);
    } else {
        _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_2);
    }

    if (game->isProximityActive) {
        hardware_display_animation_header(frame, "!?", "?!");
    } else if (game->avatarState == EPUISE) {
        hardware_display_animation_header(frame, "Zz", "zZ");
    } else {
        hardware_display_animation_header(frame, "  ", "  ");
    }
}

void hardware_display_progress_bar(Game *game) {
    int progress = (int) round((double) game->activeFrameworkProgress / FRAMEWORK_CREATION_EXP * 10.0);

    for (int i = 0; i < 10; i++) {
        lcd_move_cursor(i2c_lcd1602_info, i, 1);

        if (i < progress) {
            _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_4);
        } else {
            _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_3);
        }
    }
}

void hardware_display_avg_quality(Queue *queue) {
    int quality = get_framework_average(queue);

    char pointeurStr[2] = "";
    sprintf(pointeurStr, "%d", quality);


    lcd_move_cursor(i2c_lcd1602_info, 14, 0);
    lcd_write(i2c_lcd1602_info, pointeurStr);
    lcd_move_cursor(global_info, 15, 0);
    _write_data(global_info, I2C_LCD1602_INDEX_CUSTOM_5);
}

// Display experience and level
void hardware_display_tab_1(Game *game) {
    char formattedString[11];

    txt_home();
    snprintf(formattedString, 11, "EXP%7d", game->experience);
    lcd_write(i2c_lcd1602_info, formattedString);

    txt_line();
    snprintf(formattedString, 11, "LVL%7d", game->level);
    lcd_write(i2c_lcd1602_info, formattedString);
}

// TODO - Romain Trouver une solution pour définir l'heure (wifi)
// Display energy and time
void hardware_display_tab_2(Game *game) {
    char formattedString[11];

    txt_home();
    snprintf(formattedString, 11, "ENG%7d", game->energy);
    lcd_write(i2c_lcd1602_info, formattedString);

    txt_line();
    snprintf(formattedString, 11, "ENG%7d", game->energy);
    lcd_write(i2c_lcd1602_info, get_current_time());
}

// Display framework and progress
void hardware_display_tab_3(Game *game) {
    char formattedString[11];

    txt_home();
    snprintf(formattedString, 11, "FRW%7d", game->framework);
    lcd_write(i2c_lcd1602_info, formattedString);

    txt_line();
    snprintf(formattedString, 11, "QTY%7d", game->lastFrameworkLevel);
    hardware_display_progress_bar(game);
}

void next_tab(Game *game) {
    game->activeTab = (game->activeTab + 1) % 3;
}

void hardware_display_tab(Game *game) {
    switch (game->activeTab) {
        case 0:
            hardware_display_tab_1(game);
            break;
        case 1:
            hardware_display_tab_2(game);
            break;
        case 2:
            hardware_display_tab_3(game);
            break;
    }
}

// TODO: Add proper values
void get_framework_multipliers(Game *game) {
    switch (game->avatarState) {
        case REPOSE:
            game->frameworkQualityMultiplier = 1.25;
            game->frameworkSpeedMultiplier = 1.25;
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

// DEBUG
void print_queue(Queue *queue) {
    printf("Queue: ");
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        printf("%d ", queue->array[i]);
    }
    printf("\n");

    printf("Average: %d\n", get_framework_average(queue));
}

// DEBUG
void print_game(Game *game) {
    printf("int level %d\n", game->level);
    printf("int experience %d\n", game->experience);
    printf("double stateExperienceMultiplier %f\n", game->stateExperienceMultiplier);
    printf("int framework %d\n", game->framework);
    printf("double frameworkQualityMultiplier %f\n", game->frameworkQualityMultiplier);
    printf("double frameworkSpeedMultiplier %f\n", game->frameworkSpeedMultiplier);
    printf("int activeFrameworkProgress %d\n", game->activeFrameworkProgress);
    printf("int lastFrameworkLevel %d\n", game->lastFrameworkLevel);
    printf("int lastProximity %d\n", game->lastProximity);
    printf("bool isProximityActive %d\n", game->isProximityActive);
    printf("int energy %d\n", game->energy);
    printf("int energyMultiplier %d\n", game->energyMultiplier);
    printf("int energyIncrease %d\n", game->energyIncrease);
    printf("int lastEnergyIncrease %d\n\n", game->lastEnergyIncrease);
}

void game_loop(Game *game) {

    Queue *frameworkQuality = malloc(sizeof(Queue));
    initQueue(frameworkQuality);

    int frame = 0;

    printf("Average: %d\n", get_framework_average(frameworkQuality));
    ESP_LOGW(TAG, "Game loop entrance");

    while (true) {
        frame = (frame + 1) % 2;

        get_avatar_state(game);
        get_framework_multipliers(game);

        progress_energy(game);
        progress_framework(game, frameworkQuality);

        hardware_display_animation(frame, game);
        hardware_display_tab(game);
        hardware_display_avg_quality(frameworkQuality);

        hardware_low_energy(game);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void hardware_loop(Game *game) {
    GameData *gameData = malloc(sizeof(GameData));
    gameData->lastCrying = 0;
    bool buttonPressed = false;

    while (true) {

        if (gpio_get_level(NEXT) == 1 && !buttonPressed) {
            ESP_LOGW("SELF_LOOP", "Switch screen");
            lcd_clear(global_info);
            next_tab(game);
            buttonPressed = true;
        }
        if (gpio_get_level(ENERGY) == 1 && !buttonPressed) {
            ESP_LOGW("SELF_LOOP", "ENERGY");
            drink_energy(game);
            buttonPressed = true;
        }

        // TODO - Romain: Sur la détection de la proximité, on alerte le personnage
        if (gpio_get_level(SENSOR) == 1 && !sensor) {
            set_proximity(game, true);
            ESP_LOGW(TAG, "Sensor UP");
            sensor = true;
        }
        if (gpio_get_level(SENSOR) == 0 && sensor) {
            set_proximity(game, false);
            ESP_LOGW(TAG, "Sensor DOWN");
            sensor = false;
        }

        // TODO - Romain: Sur la détection d'intercation sociale, on alerte le personnage
        // set_social(game, 0 ou +);

        // Je sais pas si pour les boutons on va avoir besoin de garder en mémoire le previous state
        // pour pas envoyé 1000 fois la même action

        if (gpio_get_level(NEXT) == 0 && gpio_get_level(ENERGY) == 0 && buttonPressed) {
            buttonPressed = false;
        }

        hardware_cry(game, gameData);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void BLE_magic(Game *game) {
    esp_err_t ret;
    while (1) {
        ble_kill();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        init_SCAN();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        if (is_any_near) {
            ESP_LOGW(TAG, "Found someone close");
            set_social(game, 1);
        } else {
            ESP_LOGW(TAG, "I'm alone :'( ");
            set_social(game, 0);
        }

        kill_scan();
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        ble_emit();

        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    setupIO();
    ble_emit();
    initializeLCD();

    lcd_define_char(global_info, 3, charProgressEmpty);
    lcd_define_char(global_info, 4, charProgressFull);
    lcd_define_char(global_info, 5, charFlag);
    lcd_move_cursor(global_info, 15, 0);
    _write_data(global_info, I2C_LCD1602_INDEX_CUSTOM_5);

    Game game = initializeGame();

    xTaskCreate(game_loop, "game_loop", 4096, (void *) &game, 1, NULL);
    xTaskCreate(BLE_magic, "BLE_magic", 4096, (void *) &game, 1, NULL);
    hardware_loop(&game);
}
