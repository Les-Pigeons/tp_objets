
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include "I2C.h"
#include "BLE.h"
#include "esp_system.h" //esp_init funtions esp_err_t
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_event.h" //for wifi event
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "driver/i2c.h"
#include "lwip/ip_addr.h"
#include "characters.h"


#define NEXT 35
#define ENERGY 32
#define SKREEEEEEEE 25
#define LIGHT 19
#define SENSOR 14
#define SELFTAG "SELF_LOOP"
#define CONFIG_SNTP_TIME_SERVER "time.windows.com"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

#define EXAMPLE_ESP_WIFI_SSID      "cstjean"
#define EXAMPLE_ESP_WIFI_PASS      "humanisme2023"

const int BASE_FRAMEWORK_PROGRESS = 10;
const int BASE_ENERGY_INCREASE = 3;
const int BASE_ENERGY_DECREASE = 1;
const int MAX_ENERGY = 1000;
const int MAX_QUALITY = 5;
const int FRAMEWORK_CREATION_EXP = 500;
const int EXPERIENCE_QUALITY_MULTIPLIER = 10;
const char *ssid = "cstjean";
const char *pass = "humanisme2023";
int retry_num = 0;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 3600;

static const i2c_lcd1602_info_t *i2c_lcd1602_info;
static bool light_on = false;
static bool sensor = false;

static void obtain_time(void);

void main_sntp();

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

/**
 * Set game base datas
 * @return new game
 */
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

/**
 * Init the queue
 * @param queue queue
 */
void initQueue(Queue *queue) {
    queue->last = -1;

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue->array[i] = 0;
    }
}


/**
 * Circular queue to get an average of the last 20 framework levels
 * @param queue queue
 * @param value values
 * @return validation
 */
int enqueue(Queue *queue, int value) {
    queue->last = (queue->last + 1) % MAX_QUEUE_SIZE;
    queue->array[queue->last] = value;

    return 1;
}

/**
 * Set cursor position to home
 */
void txt_home() {
    lcd_move_cursor(i2c_lcd1602_info, 0, 0);
}

/**
 * Set cursor position to new line
 */
void txt_line() {
    lcd_move_cursor(i2c_lcd1602_info, 0, 1);
}


/**
 * Set up GPIO
 */
void setupIO() {
    gpio_set_direction(NEXT, GPIO_MODE_INPUT);
    gpio_set_direction(ENERGY, GPIO_MODE_INPUT);
    gpio_set_direction(SKREEEEEEEE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LIGHT, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENSOR, GPIO_MODE_INPUT);
}

/**
 * Set custom char depending on gotchi state
 * @param state Gotchi state
 */
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

/**
 * Get and set gotchi character depending on its state
 * @param game game data
 */
void get_avatar_state(Game *game) {
    if (game->energy < 50) {
        game->avatarState = EPUISE;
    } else if (game->energy < 300) {
        game->avatarState = FATIGUE;
    } else if (game->energy > 950) {
        game->avatarState = HYPERACTIF;
    } else if (game->lastProximity > 10*60*1000) {
        game->avatarState = SOLITAIRE;
    } else {
        game->avatarState = REPOSE;
    }

    set_state_custom_char(game->avatarState);
}

/**
 * Get the average of the last 20 framework levels
 * @param queue queue
 * @return framework average
 */
int get_framework_average(Queue *queue) {
    int sum = 0;
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        sum += queue->array[i];
    }

    return sum / MAX_QUEUE_SIZE;
}

/**
 * Change proximity status
 * @param game game data
 * @param isProximityActive boolean for proximity
 */
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


/**
 * Manage energy increase and decrease
 * @param game game data
 */
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

/**
 * Increase gotchi energy
 * @param game game data
 */
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

/**
 * Self explanatory
 * @return  time in string
 */
char *get_current_time() {
    time_t currentTime;
    time(&currentTime);

    char *timeString = (char *) malloc(6);

    struct tm *localTime = localtime(&currentTime);
    sprintf(timeString, "%02d:%02d", localTime->tm_hour, localTime->tm_min);

    return timeString;
}

/**
 * Set crying status
 * @param game game data
 */
void set_is_crying(Game *game) {

    if (game->socialMultiplier > 0.5) {
        game->isCrying = false;
    } else {
        game->isCrying = true;
    }
}

/**
 * Set framework quality multiplier for social
 * @param game
 */
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

/**
 * Change Social status
 * @param game game data
 * @param quantity number of friends near
 */
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

/**
 * Is used when the avatar feel alone and is crying
 * @param game game data
 * @param gameData Some other game data
 */
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

/**
 * Light LED when gotchi is tired
 * @param game game data
 */
void hardware_low_energy(Game *game) {
    if (game->energy < 300 && !light_on) {
        ESP_LOGW(SELFTAG, "Light on");
        gpio_set_level(LIGHT, 1);
        light_on = true;
    }
    if (game->energy >= 300 && light_on) {
        ESP_LOGW(SELFTAG, "Light off");
        gpio_set_level(LIGHT, 0);
        light_on = false;
    }
}

/**
 * Calculate exp required for next level
 * @param level current leven
 * @return Exp required
 */
int get_next_level_experience(int level) {
    return 100 * pow(level, 1.5);
}

/**
 * Give experience to the gotchi
 * @param game game data
 * @param quality
 */
void add_experience(Game *game, int quality) {
    game->experience += quality * EXPERIENCE_QUALITY_MULTIPLIER * game->stateExperienceMultiplier;

    if (game->experience >= get_next_level_experience(game->level)) {
        game->level++;
        game->experience = 0;
    }
}

/**
 * Add a new complete framework
 * @param game game data
 * @param frameworkQuality current framework quality
 */
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

/**
 * Define progress for current framework
 * @param game game data
 * @param frameworkQuality current framework quality
 */
void progress_framework(Game *game, Queue *frameworkQuality) {
    double progress = BASE_FRAMEWORK_PROGRESS * game->frameworkSpeedMultiplier + game->level;
    game->activeFrameworkProgress += (int) progress;

    if (game->activeFrameworkProgress >= FRAMEWORK_CREATION_EXP) {
        add_framework(game, frameworkQuality);
    }
}

/**
 * Display character for animation
 * @param frame Current
 * @param str1 first frame character
 * @param str2 second frame character
 */
void hardware_display_animation_header(int frame, char *str1, char *str2) {
    lcd_move_cursor(i2c_lcd1602_info, 11, 0);
    if (frame == 1) {
        lcd_write(i2c_lcd1602_info, str1);
    } else {
        lcd_write(i2c_lcd1602_info, str2);
    }
}

/**
 * Display animation for special events
 * @param frame Current frame playing
 * @param game game data
 */
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

/**
 * Display the progress bar for framework completion
 * @param game game data
 */
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

/**
 * get the average quality for frameworks
 * @param queue current queue
 */
void hardware_display_avg_quality(Queue *queue) {
    int quality = get_framework_average(queue);

    char pointeurStr[2] = "";
    sprintf(pointeurStr, "%d", quality);

    lcd_move_cursor(i2c_lcd1602_info, 14, 0);
    lcd_write(i2c_lcd1602_info, pointeurStr);
    lcd_move_cursor(i2c_lcd1602_info, 15, 0);
    _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_5);
}

/**
 * Display tab 1
 * @param game game data
 */
void hardware_display_tab_1(Game *game) {
    char formattedString[11];

    txt_home();
    snprintf(formattedString, 11, "EXP%7d", game->experience);
    lcd_write(i2c_lcd1602_info, formattedString);

    txt_line();
    snprintf(formattedString, 11, "LVL%7d", game->level);
    lcd_write(i2c_lcd1602_info, formattedString);
}

/**
 * Display tab 2
 * @param game game data
 */
void hardware_display_tab_2(Game *game) {
    char formattedString[11];

    txt_home();
    snprintf(formattedString, 11, "ENG%7d", game->energy);
    lcd_write(i2c_lcd1602_info, formattedString);

    txt_line();
    snprintf(formattedString, 11, "ENG%7d", game->energy);
    lcd_write(i2c_lcd1602_info, get_current_time());
}

/**
 * Display tab 3
 * @param game game data
 */
void hardware_display_tab_3(Game *game) {
    char formattedString[11];

    txt_home();
    snprintf(formattedString, 11, "FRW%7d", game->framework);
    lcd_write(i2c_lcd1602_info, formattedString);

    txt_line();
    snprintf(formattedString, 11, "QTY%7d", game->lastFrameworkLevel);
    hardware_display_progress_bar(game);
}

/**
 * Change displayed tab
 * @param game game data
 */
void next_tab(Game *game) {
    game->activeTab = (game->activeTab + 1) % 3;
}

/**
 * Support the different options for game tabs
 * @param game game data
 */
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

/**
 * Set data for framework quality
 * @param game game data
 */
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

/**
 * Software loop, manage the whole game
 * @param game game data
 */
void game_loop(Game *game) {
    Queue *frameworkQuality = malloc(sizeof(Queue));
    initQueue(frameworkQuality);
    int frame = 0;

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

        vTaskDelay(800 / portTICK_PERIOD_MS);
    }
}

/**
 * Deal with button & sensor interaction
 * @param game game data
 */
void hardware_loop(Game *game) {
    GameData *gameData = malloc(sizeof(GameData));
    gameData->lastCrying = 0;
    bool buttonPressed = false;

    while (true) {
        if (gpio_get_level(NEXT) == 1 && !buttonPressed) {
            ESP_LOGW("SELF_LOOP", "Switch screen");
            next_tab(game);
            buttonPressed = true;
        }
        if (gpio_get_level(ENERGY) == 1 && !buttonPressed) {
            ESP_LOGW("SELF_LOOP", "ENERGY");
            drink_energy(game);
            buttonPressed = true;
        }

        if (gpio_get_level(SENSOR) == 1 && !sensor) {
            set_proximity(game, true);
            ESP_LOGW(SELFTAG, "Sensor UP");
            sensor = true;
        }
        if (gpio_get_level(SENSOR) == 0 && sensor) {
            set_proximity(game, false);
            ESP_LOGW(SELFTAG, "Sensor DOWN");
            sensor = false;
        }

        if (gpio_get_level(NEXT) == 0 && gpio_get_level(ENERGY) == 0 && buttonPressed) {
            buttonPressed = false;
        }

        hardware_cry(game, gameData);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * Loop to manage BLE Events
 * @param game game data
 */
void BLE_loop(Game *game) {
    while (1) {
        BLE_switch();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ble_gestion();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        if (is_any_near) {
            ESP_LOGW(SELFTAG, "Found someone close");
            set_social(game, 1);
        } else {
            ESP_LOGW(SELFTAG, "I'm alone :'( ");
            set_social(game, 0);
        }

        BLE_switch();
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        ble_gestion();

        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}


/**
 * Handle wifi callback events
 * @param event_handler_arg ?
 * @param event_base Pointer to subsystem exposing events
 * @param event_id event id
 * @param event_data event data
 */
static void
wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        printf("WiFi CONNECTED\n");
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < 5) {
            esp_wifi_connect();
            retry_num++;
            printf("Retrying to Connect...\n");
        }
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        main_sntp();
    }
}

/**
 * Connect to WIFI
 */
void wifi_connection() {
    //                          s1.4
    // 2 - Wi-Fi Configuration Phase
    esp_netif_init();
    esp_event_loop_create_default();     // event loop                    s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station                      s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); //
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
            .sta = {
                    .ssid = "",
                    .password = "",

            }

    };
    strcpy((char *) wifi_configuration.sta.ssid, ssid);
    strcpy((char *) wifi_configuration.sta.password, pass);
    //esp_log_write(ESP_LOG_INFO, "Kconfig", "SSID=%s, PASS=%s", ssid, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
    printf("wifi_init_softap finished. SSID:%s  password:%s", ssid, pass);
}

/**
 * Entry point
 */
void app_main(void) {
    setupIO();
    ble_gestion();
    i2c_lcd1602_info = initializeLCD();
    wifi_connection();

    lcd_define_char(i2c_lcd1602_info, 3, charProgressEmpty);
    lcd_define_char(i2c_lcd1602_info, 4, charProgressFull);
    lcd_define_char(i2c_lcd1602_info, 5, charFlag);
    lcd_move_cursor(i2c_lcd1602_info, 15, 0);
    _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_5);

    Game game = initializeGame();

    xTaskCreate(game_loop, "game_loop", 4096, (void *) &game, 1, NULL);
    xTaskCreate(BLE_loop, "BLE_magic", 4096, (void *) &game, 1, NULL);
    hardware_loop(&game);

}

/**
 * Main function to connect to SNTP Servers in order to get the current time
 * https://github.com/espressif/esp-idf/blob/master/examples/protocols/sntp/README.md
 */
void main_sntp() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    else {
        // add 500 ms error to the current system time.
        // Only to demonstrate a work of adjusting method!
        {
            ESP_LOGI(SELFTAG, "Add a error for test adjtime");
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
            int64_t error_time = cpu_time + 500 * 1000L;
            struct timeval tv_error = { .tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L };
            settimeofday(&tv_error, NULL);
        }

        ESP_LOGI(SELFTAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#endif
    // Get time from New york std
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
            adjtime(NULL, &outdelta);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

}

static void obtain_time(void) {

#if LWIP_DHCP_GET_NTP_SRV
    /**
     * NTP server address could be acquired via DHCP,
     * see following menuconfig options:
     * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
     * 'LWIP_SNTP_DEBUG' - enable debugging messages
     *
     * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
     * otherwise NTP option would be rejected by default.
     */
    ESP_LOGI(SELFTAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
    config.start = false;                       // start SNTP service explicitly (after connecting)
    config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
    config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
    config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
    // configure the event on which we renew servers

    esp_netif_sntp_init(&config);

#endif /* LWIP_DHCP_GET_NTP_SRV */

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */

#if LWIP_DHCP_GET_NTP_SRV
    ESP_LOGI(SELFTAG, "Starting SNTP");
    esp_netif_sntp_start();
#if LWIP_IPV6 && SNTP_MAX_SERVERS > 2
    /* This demonstrates using IPv6 address as an additional SNTP server
     * (statically assigned IPv6 address is also possible)
     */
    ip_addr_t ip6;
    if (ipaddr_aton("2a01:3f7::1", &ip6)) {    // ipv6 ntp source "ntp.netnod.se"
        esp_sntp_setserver(2, &ip6);
    }
#endif  /* LWIP_IPV6 */

#else
    ESP_LOGI(SELFTAG, "Initializing and starting SNTP");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    /* This demonstrates configuring more than one server
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                               ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org" ) );
#else
    /*
     * This is the basic default config with one server and starting the service
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
#endif
    config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    config.smooth_sync = true;
#endif

    esp_netif_sntp_init(&config);
#endif

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(SELFTAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

    esp_netif_sntp_deinit();
}