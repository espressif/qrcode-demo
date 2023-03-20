/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_camera.h"
#include "quirc.h"
#include "quirc_internal.h"
#include "esp_timer.h"
#include "pbm.h"
#include "esp_vfs_semihost.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "src/misc/lv_color.h"
#include "qrcode_classifier.h"
#include "esp_heap_caps.h"

static const char *TAG = "example";
static const char *s_image_name = NULL;
static bool s_write_pics;
static int64_t s_freeze_canvas_delay = 2000000;
static int64_t s_freeze_canvas_until;
static lv_obj_t *s_camera_canvas;
typedef union {
    uint16_t val;
    struct {
        uint16_t b: 5;
        uint16_t g: 6;
        uint16_t r: 5;
    };
} rgb565_t;

static inline uint8_t rgb565_to_grayscale(const uint8_t* img)
{
    uint16_t* img_16 = (uint16_t*) img;
    rgb565_t rgb = {.val = __builtin_bswap16(*img_16)};
    uint16_t val = (rgb.r * 8 + rgb.g * 4 + rgb.b * 8)/3;
    return (uint8_t) MIN(255, val);
}


static void rgb565_to_grayscale_buf(const uint8_t* src, uint8_t * dst, int qr_width, int qr_height)
{
    for (size_t y = 0; y < qr_height; y++) {
        for (size_t x = 0; x < qr_width; x++) {
            dst[y*qr_width + x] = rgb565_to_grayscale(&src[(y*qr_width + x)*2]);
        }
    }
}

static void display_set_color(int r, int g, int b)
{
    if (bsp_display_lock(100)) {
        s_freeze_canvas_until = esp_timer_get_time() + s_freeze_canvas_delay;
        lv_canvas_fill_bg(s_camera_canvas, lv_color_make(b, r, g), LV_OPA_COVER);
        lv_obj_invalidate(s_camera_canvas);
        bsp_display_unlock();
    }
}

static void display_set_icon(const char* name)
{
    if (bsp_display_lock(100)) {
        s_freeze_canvas_until = esp_timer_get_time() + s_freeze_canvas_delay;
        lv_canvas_fill_bg(s_camera_canvas, lv_color_make(255, 255, 255), LV_OPA_COVER);
        lv_draw_img_dsc_t dsc;
        lv_draw_img_dsc_init(&dsc);
     
        char filename[32];
        snprintf(filename, sizeof(filename), "A:/data/%s", name);
        lv_canvas_draw_img(s_camera_canvas, (240-192)/2, (240-192)/2, filename, &dsc);
        lv_obj_invalidate(s_camera_canvas);
        bsp_display_unlock();
    }
}

static void processing_task(void* arg)
{
    struct quirc* qr;
    qr = quirc_new();
    int qr_width = 240;
    int qr_height = 240;
    if (quirc_resize(qr, qr_height, qr_height) < 0) {
        ESP_LOGE(TAG, "Failed to allocate QR buffer");
        return;
    }


    QueueHandle_t input_queue = (QueueHandle_t) arg;
    int frame = 0;
    ESP_LOGI(TAG, "Processing task ready");
    while (true) {
        camera_fb_t *pic;
        uint8_t *qr_buf = quirc_begin(qr, NULL, NULL);

        int res = xQueueReceive(input_queue, &pic, portMAX_DELAY);
        assert(res == pdPASS);
        // ESP_LOGI(TAG, "Processing task got frame");
        int64_t t_start = esp_timer_get_time();
    
        rgb565_to_grayscale_buf(pic->buf, qr_buf, qr_width, qr_height);

        int64_t t_end_conv = esp_timer_get_time();
        
        esp_camera_fb_return(pic);

        if (s_write_pics) {
            char pgm_filename[32];
            snprintf(pgm_filename, sizeof(pgm_filename), "/data/pic%04dp.pgm", frame);
            ESP_LOGI(TAG, "Writing original pic %s", pgm_filename);
            pgm_save(pgm_filename, qr_width, qr_height, qr_buf);
        }
        ++frame;
        quirc_end(qr);

        int64_t t_end_find = esp_timer_get_time();

        if (s_write_pics) {
            char pgm_filename[32];
            snprintf(pgm_filename, sizeof(pgm_filename), "/data/pic%04dr.pgm", frame);
            ESP_LOGI(TAG, "Writing rectified pic %s", pgm_filename);
            pgm_save(pgm_filename, qr_width, qr_height, qr->pixels);
        }

        int count = quirc_count(qr);
        quirc_decode_error_t err = QUIRC_ERROR_DATA_UNDERFLOW;
        int time_find_ms = (int)(t_end_find - t_start)/1000;
        ESP_LOGI(TAG, "QR count: %d   Heap: %d   time: %d ms", count, heap_caps_get_free_size(MALLOC_CAP_DEFAULT), time_find_ms);
        for (int i = 0; i < count; i++) {
            struct quirc_code code = {};
            struct quirc_data qr_data = {};
            quirc_extract(qr, i, &code);
            quirc_flip(&code);
            err = quirc_decode(&code, &qr_data);
            int64_t t_end = esp_timer_get_time();
            int time_decode_ms = (int)(t_end - t_end_find)/1000;
            ESP_LOGI(TAG, "Decoded in %d ms", time_decode_ms);
            if (err != 0) {
                ESP_LOGI(TAG, "QR err: %d", err);
            } else {
                bsp_led_set(BSP_LED_GREEN, true);
                if (strstr((const char *)qr_data.payload, "COLOR:") != NULL) {
                    int r = 0, g = 0, b = 0;
                    sscanf((const char *)qr_data.payload, "COLOR:%02x%02x%02x", &r, &g, &b);
                    ESP_LOGI(TAG, "QR code: COLOR(%d, %d, %d)", r, g, b);
                    display_set_color(r, g, b);
                } else {
                    const char* filename = classifier_get_pic_from_qrcode_data((const char*) qr_data.payload);
                    ESP_LOGI(TAG, "QR-code: %d bytes: '%s'. Classified as '%s'.", qr_data.payload_len, qr_data.payload, filename);
                    display_set_icon(filename);
                }
                bsp_led_set(BSP_LED_GREEN, false);
            }
        }
    }
}

void main_task(void* arg) {
    bsp_i2c_init();
    bsp_leds_init();
    bsp_display_start();
    bsp_display_backlight_on(); // Set display brightness to 100%
    bsp_led_set(BSP_LED_GREEN, false);


    // Initialize the camera
    const camera_config_t camera_config = BSP_CAMERA_DEFAULT_CONFIG;
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    ESP_LOGI(TAG, "Camera Init done");

    // Create LVGL canvas for camera image
    bsp_display_lock(0);
    s_camera_canvas = lv_canvas_create(lv_scr_act());
    assert(s_camera_canvas);
    lv_obj_center(s_camera_canvas);
    uint8_t* canvas_buf = heap_caps_malloc(240*240*2, MALLOC_CAP_SPIRAM);
    assert(canvas_buf);
    lv_canvas_set_buffer(s_camera_canvas, canvas_buf, 240, 240, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(s_camera_canvas, lv_color_black(), LV_OPA_COVER);
    bsp_display_unlock();



    camera_fb_t *pic;
    QueueHandle_t processing_queue = xQueueCreate(1, sizeof(camera_fb_t*));
    assert(processing_queue);
    
    xTaskCreatePinnedToCore(&processing_task, "processing", 100000, processing_queue, 1, NULL, 0);
    ESP_LOGI(TAG, "Processing task started");

    while (1) {
        pic = esp_camera_fb_get();
        if (pic) {
            if (esp_timer_get_time() > s_freeze_canvas_until) {
                if (bsp_display_lock(10)) {
                    lv_canvas_copy_buf(s_camera_canvas, pic->buf, 0, 0, pic->width, pic->height);
                    lv_obj_invalidate(s_camera_canvas);
                    bsp_display_unlock();
                }
            }

            int res = xQueueSend(processing_queue, &pic, pdMS_TO_TICKS(10));
            if (res == pdFAIL) {
                esp_camera_fb_return(pic);
            }
        } else {
            ESP_LOGE(TAG, "Get frame failed");
        }
    }

}

static void sdcard_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 1,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    const sdmmc_slot_config_t slot_config = {
        .clk = BSP_SD_CLK,
        .cmd = BSP_SD_CMD,
        .d0 = BSP_SD_D0,
        .d1 = GPIO_NUM_NC,
        .d2 = GPIO_NUM_NC,
        .d3 = GPIO_NUM_NC,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 1,
        .flags = 0,
    };
    sdmmc_card_t *bsp_sdcard = NULL;
    esp_err_t err = esp_vfs_fat_sdmmc_mount("/data", &host, &slot_config, &mount_config, &bsp_sdcard);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SD Card mounted");
        // s_write_pics = true;
    } else {
        ESP_LOGI(TAG, "No SD card");
    }
}

static void semihosting_mount(void)
{
    esp_err_t err = esp_vfs_semihost_register("/data");
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Semihosting started");
        s_write_pics = true;
    } else {
        ESP_LOGI(TAG, "Semihosting start failed");
    }
}

void app_main(void)
{
    sdcard_mount();
    classifier_init();
    xTaskCreatePinnedToCore(&main_task, "main", 4096, NULL, 5, NULL, 0);
}
