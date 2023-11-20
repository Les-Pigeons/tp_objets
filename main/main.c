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
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "initi2c.h"
#include "initBLE.h"
#include "scanBLE.h"
#include "esp_log.h"
#include "driver/i2c.h"

#define RED_BUTTON                  35
#define BLUE_BUTTON                  32

i2c_lcd1602_info_t* global_info;

void img_home()
{
    lcd_move_cursor(global_info, 0, 0);
}

void img_line()
{
    lcd_move_cursor(global_info, 0, 1);
}

void txt_home()
{
    lcd_move_cursor(global_info, 5, 0);
}

void txt_line()
{
    lcd_move_cursor(global_info, 5, 1);
}

void buttonTest(void *pParam)
{
    bool buttonPressed = false;
    int count = 998;
    char pointeurStr[50] = "";
    lcd_clear(global_info);
    lcd_write(global_info, "Button not pressed");

    while (1)
    {
        if (gpio_get_level(RED_BUTTON) == 1 && !buttonPressed)
        {
            count++;
            lcd_clear(global_info);
            sprintf(pointeurStr, "%d times", count);
            lcd_write(global_info, pointeurStr);
            txt_line();
            lcd_write(global_info, "Coucou :)");
        }
        if (gpio_get_level(BLUE_BUTTON) == 1 && !buttonPressed)
        {
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



void setupIO()
{
    gpio_set_direction(RED_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(BLUE_BUTTON, GPIO_MODE_INPUT);
}

void app_main(void)
{
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

    i2c_lcd1602_info_t* i2c_lcd1602_info = i2c_lcd1602_malloc();
    global_info = i2c_lcd1602_info;
    smbus_info_t* smbus_info = malloc(sizeof(smbus_info_t));
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
    uint8_t charset[] = {
            0x0E,
            0x0A,
            0x0E,
            0x0A,
            0x0A,
            0x0A,
            0x1F,
            0x1B
                        };
    lcd_define_char(i2c_lcd1602_info, 1, charset);

    uint8_t charset2[] = {
            0x0A,
            0x15,
            0x15,
            0x11,
            0x11,
            0x0A,
            0x04,
            0x00
    };

    lcd_define_char(i2c_lcd1602_info, 2, charset2);


    lcd_move_cursor(i2c_lcd1602_info, 1, 1);
    _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_1);
    _write_data(i2c_lcd1602_info, I2C_LCD1602_INDEX_CUSTOM_2);


    // xTaskCreate(buttonTest, "buttonTest", 2048, NULL, 10, NULL);
}
