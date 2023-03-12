#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"


#define BOARD_LCD_MOSI 47
#define BOARD_LCD_MISO -1
#define BOARD_LCD_SCK 21
#define BOARD_LCD_CS 44
#define BOARD_LCD_DC 43
#define BOARD_LCD_RST -1
#define BOARD_LCD_BL 48
#define BOARD_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define BOARD_LCD_BK_LIGHT_ON_LEVEL 0
#define BOARD_LCD_BK_LIGHT_OFF_LEVEL !BOARD_LCD_BK_LIGHT_ON_LEVEL
#define BOARD_LCD_H_RES 240
#define BOARD_LCD_V_RES 240
#define BOARD_LCD_CMD_BITS 8
#define BOARD_LCD_PARAM_BITS 8
#define LCD_HOST SPI2_HOST
#define PARALLEL_LINES 16
#define BOARD_LED_GPIO 3

static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t *lines_buf;

typedef union {
    struct {
        uint16_t g:6;
        uint16_t b:5;
        uint16_t r:5;
    };
    uint16_t val;
} clr_t;


void board_led_set(int on)
{
    if (on) {
        gpio_set_direction(BOARD_LED_GPIO, GPIO_MODE_INPUT);
    } else {
        gpio_set_direction(BOARD_LED_GPIO, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(BOARD_LED_GPIO, 0);
    }
}

void lcd_init()
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_LCD_BL
    };
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_LCD_BL, BOARD_LCD_BK_LIGHT_ON_LEVEL));


    gpio_config_t pwr_gpio_config = {
        .mode = GPIO_MODE_OUTPUT_OD,
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO
    };
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&pwr_gpio_config));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_LED_GPIO, 0));


    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_LCD_SCK,
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * BOARD_LCD_H_RES * 2 + 8
    };

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_DC,
        .cs_gpio_num = BOARD_LCD_CS,
        .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = BOARD_LCD_CMD_BITS,
        .lcd_param_bits = BOARD_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    // Initialize the LCD configuration
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    esp_lcd_panel_invert_color(panel_handle, true);
    lines_buf = (uint16_t*) calloc(PARALLEL_LINES * BOARD_LCD_H_RES * 2, 1);
    if (lines_buf == NULL) { abort(); }
}

void lcd_fill(int r, int g, int b)
{
    clr_t color = {
        .b = b * 31 / 100,
        .g = g * 63 / 100,
        .r = r * 31 / 100
    };
    for (size_t i = 0; i < PARALLEL_LINES * BOARD_LCD_H_RES; i++) {
        lines_buf[i] = color.val;
    }
    for (int y = 0; y < BOARD_LCD_V_RES; y += PARALLEL_LINES) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, BOARD_LCD_H_RES, y + PARALLEL_LINES, lines_buf);
    }
}

static inline uint16_t gray2color(uint8_t val) {
    val /= 8;
    clr_t color = {
        .b = ((uint16_t) val) * 31 / 255,
        .g = ((uint16_t) val) * 63 / 255,
        .r = ((uint16_t) val) * 31 / 255
    };
    return color.val;
}

void lcd_draw_grayscale(uint8_t* buf, int w, int h)
{
    assert(w >= 2 * BOARD_LCD_H_RES);
    assert(h == 2 * BOARD_LCD_V_RES);

    int x0 = (w - 2 * BOARD_LCD_H_RES) / 2;

    for (int y = 0; y < BOARD_LCD_V_RES; y += PARALLEL_LINES) {
        for (int dy = 0; dy < PARALLEL_LINES; ++dy) {
            int by = (y + dy) * 2;
            for (int x = 0; x < BOARD_LCD_H_RES; ++x) {
                int bx = x * 2 + x0;
                uint8_t buf_val = buf[bx + w * by];
                lines_buf[x + dy * BOARD_LCD_H_RES] = gray2color(buf_val);
            }
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, BOARD_LCD_H_RES, y + PARALLEL_LINES, lines_buf);
    }
}