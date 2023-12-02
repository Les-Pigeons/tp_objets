#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "driver/i2c.h"

typedef uint16_t i2c_address_t;

typedef struct
{
    bool init;
    i2c_port_t i2c_port;
    i2c_address_t address;
    portBASE_TYPE timeout;
} smbus_info_t;

typedef struct
{
    bool init;                                          ///< True if struct has been initialised, otherwise false
    smbus_info_t * smbus_info;                          ///< Pointer to associated SMBus info
    uint8_t backlight_flag;                             ///< Non-zero if backlight is to be enabled, otherwise zero
    uint8_t num_rows;                                   ///< Number of configured columns
    uint8_t num_columns;                                ///< Number of configured columns, including offscreen columns
    uint8_t num_visible_columns;                        ///< Number of visible columns
    uint8_t display_control_flags;                      ///< Currently active display control flags
    uint8_t entry_mode_flags;                           ///< Currently active entry mode flags
} i2c_lcd1602_info_t;

/**
 * @brief Enum of valid indexes for definitions of user-defined characters.
 */
typedef enum
{
    I2C_LCD1602_INDEX_CUSTOM_0 = 0,                     ///< Index of first user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_1,                         ///< Index of second user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_2,                         ///< Index of third user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_3,                         ///< Index of fourth user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_4,                         ///< Index of fifth user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_5,                         ///< Index of sixth user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_6,                         ///< Index of seventh user-defined custom symbol
    I2C_LCD1602_INDEX_CUSTOM_7,                         ///< Index of eighth user-defined custom symbol
} i2c_lcd1602_custom_index_t;

#define I2C_LCD1602_CHARACTER_CUSTOM_0     0b00001000   ///< User-defined custom symbol in index 0
#define I2C_LCD1602_CHARACTER_CUSTOM_1     0b00001001   ///< User-defined custom symbol in index 1
#define I2C_LCD1602_CHARACTER_CUSTOM_2     0b00001010   ///< User-defined custom symbol in index 2
#define I2C_LCD1602_CHARACTER_CUSTOM_3     0b00001011   ///< User-defined custom symbol in index 3
#define I2C_LCD1602_CHARACTER_CUSTOM_4     0b00001100   ///< User-defined custom symbol in index 4
#define I2C_LCD1602_CHARACTER_CUSTOM_5     0b00001101   ///< User-defined custom symbol in index 5
#define I2C_LCD1602_CHARACTER_CUSTOM_6     0b00001110   ///< User-defined custom symbol in index 6
#define I2C_LCD1602_CHARACTER_CUSTOM_7     0b00001111   ///< User-defined custom symbol in index 7

#define I2C_LCD1602_CHARACTER_ALPHA        0b11100000   ///< Lower-case alpha symbol
#define I2C_LCD1602_CHARACTER_BETA         0b11100010   ///< Lower-case beta symbol
#define I2C_LCD1602_CHARACTER_THETA        0b11110010   ///< Lower-case theta symbol
#define I2C_LCD1602_CHARACTER_PI           0b11110111   ///< Lower-case pi symbol
#define I2C_LCD1602_CHARACTER_OMEGA        0b11110100   ///< Upper-case omega symbol
#define I2C_LCD1602_CHARACTER_SIGMA        0b11110110   ///< Upper-case sigma symbol
#define I2C_LCD1602_CHARACTER_INFINITY     0b11110011   ///< Infinity symbol
#define I2C_LCD1602_CHARACTER_DEGREE       0b11011111   ///< Degree symbol
#define I2C_LCD1602_CHARACTER_ARROW_RIGHT  0b01111110   ///< Arrow pointing right symbol
#define I2C_LCD1602_CHARACTER_ARROW_LEFT   0b01111111   ///< Arrow pointing left symbol
#define I2C_LCD1602_CHARACTER_SQUARE       0b11011011   ///< Square outline symbol
#define I2C_LCD1602_CHARACTER_DOT          0b10100101   ///< Centred dot symbol
#define I2C_LCD1602_CHARACTER_DIVIDE       0b11111101   ///< Division sign symbol
#define I2C_LCD1602_CHARACTER_BLOCK        0b11111111   ///< 5x8 filled block

#define TAG "i2c-lcd1602"


#define I2C_MASTER_SCL_IO           22      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           21      /*!< GPIO number used for I2C master data  */

#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

//Keep the LEVELx values as they are here; they match up with (1<<level)
#define ESP_INTR_FLAG_LEVEL1        (1<<1)  ///< Accept a Level 1 interrupt vector (lowest priority)
#define ESP_INTR_FLAG_LEVEL2        (1<<2)  ///< Accept a Level 2 interrupt vector
#define ESP_INTR_FLAG_LEVEL3        (1<<3)  ///< Accept a Level 3 interrupt vector
#define ESP_INTR_FLAG_LEVEL4        (1<<4)  ///< Accept a Level 4 interrupt vector
#define ESP_INTR_FLAG_LEVEL5        (1<<5)  ///< Accept a Level 5 interrupt vector
#define ESP_INTR_FLAG_LEVEL6        (1<<6)  ///< Accept a Level 6 interrupt vector
#define ESP_INTR_FLAG_NMI           (1<<7)  ///< Accept a Level 7 interrupt vector (highest priority)
#define ESP_INTR_FLAG_SHARED        (1<<8)  ///< Interrupt can be shared between ISRs
#define ESP_INTR_FLAG_EDGE          (1<<9)  ///< Edge-triggered interrupt
#define ESP_INTR_FLAG_IRAM          (1<<10) ///< ISR can be called if cache is disabled
#define ESP_INTR_FLAG_INTRDISABLED  (1<<11) ///< Return with this interrupt disabled

#define MPU9250_SENSOR_ADDR                 0x3f        /*!< Slave address of the MPU9250 sensor */
#define MPU9250_WHO_AM_I_REG_ADDR           0x75        /*!< Register addresses of the "who am I" register */

#define MPU9250_PWR_MGMT_1_REG_ADDR         0x6B        /*!< Register addresses of the power managment register */
#define MPU9250_RESET_BIT                   7


#define DELAY_ENABLE_PULSE_WIDTH      1  // enable pulse must be at least 450ns wide
#define DELAY_ENABLE_PULSE_SETTLE    50  // command requires > 37us to settle (table 6 in datasheet)

#define COMMAND_CLEAR_DISPLAY       0x01
#define COMMAND_RETURN_HOME         0x02
#define COMMAND_ENTRY_MODE_SET      0x04
#define COMMAND_DISPLAY_CONTROL     0x08
#define COMMAND_SHIFT               0x10
#define COMMAND_FUNCTION_SET        0x20
#define COMMAND_SET_CGRAM_ADDR      0x40
#define COMMAND_SET_DDRAM_ADDR      0x80

#define DELAY_POWER_ON            50000  // wait at least 40us after VCC rises to 2.7V
#define DELAY_INIT_1               4500  // wait at least 4.1ms (fig 24, page 46)
#define DELAY_INIT_2               4500  // wait at least 4.1ms (fig 24, page 46)
#define DELAY_INIT_3                120  // wait at least 100us (fig 24, page 46)

#define DELAY_CLEAR_DISPLAY        2000
#define DELAY_RETURN_HOME          2000
#define SMBUS_DEFAULT_TIMEOUT (1000 / 100)

// COMMAND_ENTRY_MODE_SET flags
#define FLAG_ENTRY_MODE_SET_ENTRY_INCREMENT       0x02
#define FLAG_ENTRY_MODE_SET_ENTRY_DECREMENT       0x00
#define FLAG_ENTRY_MODE_SET_ENTRY_SHIFT_ON        0x01
#define FLAG_ENTRY_MODE_SET_ENTRY_SHIFT_OFF       0x00

// COMMAND_DISPLAY_CONTROL flags
#define FLAG_DISPLAY_CONTROL_DISPLAY_ON  0x04
#define FLAG_DISPLAY_CONTROL_DISPLAY_OFF 0x00
#define FLAG_DISPLAY_CONTROL_CURSOR_ON   0x02
#define FLAG_DISPLAY_CONTROL_CURSOR_OFF  0x00
#define FLAG_DISPLAY_CONTROL_BLINK_ON    0x01
#define FLAG_DISPLAY_CONTROL_BLINK_OFF   0x00

// COMMAND_SHIFT flags
#define FLAG_SHIFT_MOVE_DISPLAY          0x08
#define FLAG_SHIFT_MOVE_CURSOR           0x00
#define FLAG_SHIFT_MOVE_LEFT             0x04
#define FLAG_SHIFT_MOVE_RIGHT            0x00

// COMMAND_FUNCTION_SET flags
#define FLAG_FUNCTION_SET_MODE_8BIT      0x10
#define FLAG_FUNCTION_SET_MODE_4BIT      0x00
#define FLAG_FUNCTION_SET_LINES_2        0x08
#define FLAG_FUNCTION_SET_LINES_1        0x00
#define FLAG_FUNCTION_SET_DOTS_5X10      0x04
#define FLAG_FUNCTION_SET_DOTS_5X8       0x00


// Control flags
#define FLAG_BACKLIGHT_ON    0b00001000      // backlight enabled (disabled if clear)
#define FLAG_BACKLIGHT_OFF   0b00000000      // backlight disabled
#define FLAG_ENABLE          0b00000100
#define FLAG_READ            0b00000010      // read (write if clear)
#define FLAG_WRITE           0b00000000      // write
#define FLAG_RS_DATA         0b00000001      // data (command if clear)
#define FLAG_RS_COMMAND      0b00000000      // command

#define WRITE_BIT      I2C_MASTER_WRITE
#define READ_BIT       I2C_MASTER_READ
#define ACK_CHECK      true
#define NO_ACK_CHECK   false
#define ACK_VALUE      0x0
#define NACK_VALUE     0x1
#define MAX_BLOCK_LEN  255  // SMBus v3.0 increases this from 32 to 255

i2c_lcd1602_info_t * i2c_lcd1602_malloc(void)
{
    i2c_lcd1602_info_t * i2c_lcd1602_info = malloc(sizeof(*i2c_lcd1602_info));
    if (i2c_lcd1602_info != NULL)
    {
        memset(i2c_lcd1602_info, 0, sizeof(*i2c_lcd1602_info));
    }
    return i2c_lcd1602_info;
}

esp_err_t smbus_init(smbus_info_t * smbus_info, i2c_port_t i2c_port, i2c_address_t address)
{
    if (smbus_info != NULL)
    {
        smbus_info->i2c_port = i2c_port;
        smbus_info->address = address;
        smbus_info->timeout = SMBUS_DEFAULT_TIMEOUT;
        smbus_info->init = true;
    }
    else
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t _check_i2c_error(esp_err_t err)
{
    switch (err)
    {
        case ESP_OK:  // Success
            break;
        case ESP_ERR_INVALID_ARG:  // Parameter error
            break;
        case ESP_FAIL: // Sending command error, slave doesn't ACK the transfer.
            break;
        case ESP_ERR_INVALID_STATE:  // I2C driver not installed or not in master mode.
            break;
        case ESP_ERR_TIMEOUT:  // Operation timeout because the bus is busy.
            break;
        default:
    }
    return err;
}

/**
 * Verify if lcd is correctly initialized
 * @param i2c_lcd1602_info Screen infos
 * @return Bool
 */
static bool _is_init(const i2c_lcd1602_info_t * i2c_lcd1602_info) // Je suis au courant de l'erreur de pointeur ici, je ne sais malheureusement pas comment le gérer efficacement
{
    bool ok = false;
    if (i2c_lcd1602_info != NULL)
    {
        if (i2c_lcd1602_info->init)
        {
            ok = true;
        }
    }
    return ok;
}

/**
 * Send bytes directly to LCD
 *
 * @param smbus_info Screen infos
 * @param data Data to send
 * @return Error
 * @return Nothing
 */
esp_err_t smbus_send_byte(const smbus_info_t * smbus_info, uint8_t data)
{
    // Protocol: [S | ADDR | Wr | As | DATA | As | P]
    esp_err_t err = ESP_FAIL;
    if (_is_init(smbus_info))
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, smbus_info->address << 1 | WRITE_BIT, ACK_CHECK);
        i2c_master_write_byte(cmd, data, ACK_CHECK);
        i2c_master_stop(cmd);
        err = _check_i2c_error(i2c_master_cmd_begin(smbus_info->i2c_port, cmd, smbus_info->timeout));
        i2c_cmd_link_delete(cmd);
    }
    return err;
}

static esp_err_t _write_to_expander(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t data)
{
    // backlight flag must be included with every write to maintain backlight state
    return smbus_send_byte(i2c_lcd1602_info->smbus_info, data | i2c_lcd1602_info->backlight_flag);
}

// IMPORTANT - for the display to stay "in sync" it is important that errors do not interrupt the
// 2 x nibble sequence.

// clock data from expander to LCD by causing a falling edge on Enable
static esp_err_t _strobe_enable(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t data)
{
    esp_err_t err1 = _write_to_expander(i2c_lcd1602_info, data | FLAG_ENABLE);
    esp_rom_delay_us(DELAY_ENABLE_PULSE_WIDTH);
    esp_err_t err2 = _write_to_expander(i2c_lcd1602_info, data & ~FLAG_ENABLE);
    esp_rom_delay_us(DELAY_ENABLE_PULSE_SETTLE);
    return err1 ? err1 : err2;
}

// send top nibble to the LCD controller
static esp_err_t _write_top_nibble(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t data)
{
    esp_err_t err1 = _write_to_expander(i2c_lcd1602_info, data);
    esp_err_t err2 = _strobe_enable(i2c_lcd1602_info, data);
    return err1 ? err1 : err2;
}

// send command or data to controller
static esp_err_t _write(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t value, uint8_t register_select_flag)
{
    esp_err_t err1 = _write_top_nibble(i2c_lcd1602_info, (value & 0xf0) | register_select_flag);
    esp_err_t err2 = _write_top_nibble(i2c_lcd1602_info, ((value & 0x0f) << 4) | register_select_flag);
    return err1 ? err1 : err2;
}

// send command to controller
static esp_err_t _write_command(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t command)
{
    return _write(i2c_lcd1602_info, command, FLAG_RS_COMMAND);
}

// send data to controller
static esp_err_t _write_data(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t data)
{
    return _write(i2c_lcd1602_info, data, FLAG_RS_DATA);
}


// Send write char to controller
esp_err_t i2c_lcd1602_write_char(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t chr)
{
    esp_err_t err = ESP_FAIL;
    if (_is_init(i2c_lcd1602_info))
    {
        err = _write_data(i2c_lcd1602_info, chr);
    }
    return err;
}

// Send write to controller
esp_err_t lcd_write(const i2c_lcd1602_info_t * i2c_lcd1602_info, const char * string)
{
    esp_err_t err = ESP_FAIL;
    if (_is_init(i2c_lcd1602_info))
    {
        err = ESP_OK;
        for (int i = 0; err == ESP_OK && string[i]; ++i)
        {
            err = _write_data(i2c_lcd1602_info, string[i]);
        }
    }
    return err;
}

// Send command to return cursor to base 0 0 position
esp_err_t lcd_home(const i2c_lcd1602_info_t * i2c_lcd1602_info)
{
    esp_err_t err = ESP_FAIL;
    if (_is_init(i2c_lcd1602_info))
    {
        err = _write_command(i2c_lcd1602_info, COMMAND_RETURN_HOME);
        if (err == ESP_OK)
        {
            esp_rom_delay_us(DELAY_RETURN_HOME);
        }
    }
    return err;
}

// Send command to clear screen
esp_err_t lcd_clear(const i2c_lcd1602_info_t * i2c_lcd1602_info)
{
    esp_err_t err = ESP_FAIL;
    if (_is_init(i2c_lcd1602_info))
    {
        err = _write_command(i2c_lcd1602_info, COMMAND_CLEAR_DISPLAY);
        if (err == ESP_OK)
        {
            esp_rom_delay_us(DELAY_CLEAR_DISPLAY);
        }
    }
    return err;
}

// Send command to reset screen
esp_err_t i2c_lcd1602_reset(const i2c_lcd1602_info_t * i2c_lcd1602_info)
{
    esp_err_t first_err = ESP_OK;
    esp_err_t last_err = ESP_FAIL;

    // put Expander into known state - Register Select and Read/Write both low
    if ((last_err = _write_to_expander(i2c_lcd1602_info, 0)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    esp_rom_delay_us(1000);

    // select 4-bit mode on LCD controller - see datasheet page 46, figure 24.
    if ((last_err = _write_top_nibble(i2c_lcd1602_info, 0x03 << 4)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    esp_rom_delay_us(DELAY_INIT_1);

    // repeat
    if ((last_err = _write_top_nibble(i2c_lcd1602_info, 0x03 << 4)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    esp_rom_delay_us(DELAY_INIT_2);

    // repeat
    if ((last_err = _write_top_nibble(i2c_lcd1602_info, 0x03 << 4)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    esp_rom_delay_us(DELAY_INIT_3);

    // select 4-bit mode
    if ((last_err = _write_top_nibble(i2c_lcd1602_info, 0x02 << 4)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    // now we can use the command()/write() functions
    if ((last_err = _write_command(i2c_lcd1602_info, COMMAND_FUNCTION_SET | FLAG_FUNCTION_SET_MODE_4BIT | FLAG_FUNCTION_SET_LINES_2 | FLAG_FUNCTION_SET_DOTS_5X8)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    if ((last_err = _write_command(i2c_lcd1602_info, COMMAND_DISPLAY_CONTROL | i2c_lcd1602_info->display_control_flags)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    if ((last_err = lcd_clear(i2c_lcd1602_info)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    if ((last_err = _write_command(i2c_lcd1602_info, COMMAND_ENTRY_MODE_SET | i2c_lcd1602_info->entry_mode_flags)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    if ((last_err = lcd_home(i2c_lcd1602_info)) != ESP_OK)
    {
        if (first_err == ESP_OK)
            first_err = last_err;
    }

    return first_err;
}

// Initialize LCD display
esp_err_t i2c_lcd1602_init(i2c_lcd1602_info_t * i2c_lcd1602_info, smbus_info_t * smbus_info,
                           bool backlight, uint8_t num_rows, uint8_t num_columns, uint8_t num_visible_columns)
{
    esp_err_t err = ESP_FAIL;
    if (i2c_lcd1602_info != NULL)
    {
        i2c_lcd1602_info->smbus_info = smbus_info;
        i2c_lcd1602_info->backlight_flag = backlight ? FLAG_BACKLIGHT_ON : FLAG_BACKLIGHT_OFF;
        i2c_lcd1602_info->num_rows = num_rows;
        i2c_lcd1602_info->num_columns = num_columns;
        i2c_lcd1602_info->num_visible_columns = num_visible_columns;

        // display on, no cursor, no blinking
        i2c_lcd1602_info->display_control_flags = FLAG_DISPLAY_CONTROL_DISPLAY_ON | FLAG_DISPLAY_CONTROL_CURSOR_OFF | FLAG_DISPLAY_CONTROL_BLINK_OFF;

        // left-justified left-to-right text
        i2c_lcd1602_info->entry_mode_flags = FLAG_ENTRY_MODE_SET_ENTRY_INCREMENT | FLAG_ENTRY_MODE_SET_ENTRY_SHIFT_OFF;

        i2c_lcd1602_info->init = true;

        // Wait at least 40ms after power rises above 2.7V before sending commands.
        esp_rom_delay_us(DELAY_POWER_ON);

        err = i2c_lcd1602_reset(i2c_lcd1602_info);
    }
    else
    {
        err = ESP_FAIL;
    }
    return err;
}

// Send command to define custom characters
esp_err_t lcd_define_char(const i2c_lcd1602_info_t * i2c_lcd1602_info, i2c_lcd1602_custom_index_t index, const uint8_t pixelmap[])
{
    esp_err_t err = ESP_FAIL;
    if (_is_init(i2c_lcd1602_info))
    {
        index &= 0x07;  // only the first 8 indexes can be used for custom characters
        err = _write_command(i2c_lcd1602_info, COMMAND_SET_CGRAM_ADDR | (index << 3));
        for (int i = 0; err == ESP_OK && i < 8; ++i)
        {
            err = _write_data(i2c_lcd1602_info, pixelmap[i]);
        }
    }
    return err;
}

// Send command to move cursor to defined x y position
esp_err_t lcd_move_cursor(const i2c_lcd1602_info_t * i2c_lcd1602_info, uint8_t col, uint8_t row)
{
    esp_err_t err = ESP_FAIL;
    if (_is_init(i2c_lcd1602_info))
    {
        const int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
        if (row > i2c_lcd1602_info->num_rows)
        {
            row = i2c_lcd1602_info->num_rows - 1;
        }
        if (col > i2c_lcd1602_info->num_columns)
        {
            col = i2c_lcd1602_info->num_columns - 1;
        }
        err = _write_command(i2c_lcd1602_info, COMMAND_SET_DDRAM_ADDR | (col + row_offsets[row]));
    }
    return err;
}

// Initialize the LCD
i2c_lcd1602_info_t *initializeLCD() {
    i2c_config_t conf = {
            .mode = 1,
            .sda_io_num = I2C_MASTER_SDA_IO,         // select GPIO specific to your project
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_MASTER_SCL_IO,         // select GPIO specific to your project
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
    };

    i2c_lcd1602_info_t *i2c_lcd1602_info = i2c_lcd1602_malloc();

    smbus_info_t *smbus_info = malloc(sizeof(smbus_info_t));
    smbus_init(smbus_info, 0, MPU9250_SENSOR_ADDR);

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(smbus_info->i2c_port, 1, 0, 0, ESP_INTR_FLAG_SHARED);
    i2c_master_start(i2c_cmd_link_create());

    i2c_lcd1602_init(i2c_lcd1602_info, smbus_info, true, 2, 16, 16);

    return i2c_lcd1602_info;
}