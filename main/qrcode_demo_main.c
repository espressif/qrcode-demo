/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/sdmmc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "bsp/esp-bsp.h"
#include "sdkconfig.h"
#include "quirc.h"
#include "quirc_internal.h"
#include "esp_camera.h"
#include "src/misc/lv_color.h"
#include "qrcode_classifier.h"

static const char *TAG = "example";

// Camera & display image size. On ESP32-S3-EYE board, keep them equal for simplicity:
#define IMG_WIDTH 240
#define IMG_HEIGHT 240
#define CAM_FRAME_SIZE FRAMESIZE_240X240

// LVGL canvas object where the image is displayed
static lv_obj_t *s_camera_canvas;

// After showing something on the display (color fill or an icon), don't show the camera image for a while.
static const int64_t s_freeze_canvas_delay = 2000000;
static int64_t s_freeze_canvas_until;

static uint8_t rgb565_to_grayscale(const uint8_t *img);
static void rgb565_to_grayscale_buf(const uint8_t *src, uint8_t *dst, int qr_width, int qr_height);
static void display_set_color(int r, int g, int b);
static void display_set_icon(const char *name);
static void processing_task(void *arg);
static void main_task(void *arg);

// Application entry point
void app_main(void)
{
    xTaskCreatePinnedToCore(&main_task, "main", 4096, NULL, 5, NULL, 0);
}

// Main task: initializes the board, camera, and SD card, starts the processing task, then runs the main loop
static void main_task(void *arg)
{
    // Initialize the board
    bsp_i2c_init();
    bsp_leds_init();
    bsp_display_start();
    bsp_display_backlight_on();
    bsp_led_set(BSP_LED_GREEN, false);

    // Initialize the SD card, if present, and load the classifier
    esp_err_t err = bsp_sdcard_mount();
    if (err == ESP_OK) {
        classifier_init(CONFIG_BSP_SD_MOUNT_POINT "/qrclass.txt");
    } else {
        ESP_LOGW(TAG, "No SD card, no classifiers loaded");
    }

    // Initialize the camera
    camera_config_t camera_config = BSP_CAMERA_DEFAULT_CONFIG;
    camera_config.frame_size = CAM_FRAME_SIZE;
    ESP_ERROR_CHECK(esp_camera_init(&camera_config));
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    ESP_LOGI(TAG, "Camera Init done");

    // Create LVGL canvas for camera image
    bsp_display_lock(0);
    s_camera_canvas = lv_canvas_create(lv_scr_act());
    assert(s_camera_canvas);
    lv_obj_center(s_camera_canvas);
    uint8_t *canvas_buf = heap_caps_malloc(IMG_WIDTH * IMG_HEIGHT * 2, MALLOC_CAP_SPIRAM);
    assert(canvas_buf);
    lv_canvas_set_buffer(s_camera_canvas, canvas_buf, IMG_WIDTH, IMG_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(s_camera_canvas, lv_color_black(), LV_OPA_COVER);
    bsp_display_unlock();

    // The queue for passing camera frames to the processing task
    QueueHandle_t processing_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    assert(processing_queue);

    // The processing task will be running QR code detection and recognition
    xTaskCreatePinnedToCore(&processing_task, "processing", 35000, processing_queue, 1, NULL, 0);
    ESP_LOGI(TAG, "Processing task started");

    // The main loop to get frames from the camera
    while (1) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (pic == NULL) {
            ESP_LOGE(TAG, "Get frame failed");
            continue;
        }
        // Don't update the display if the display image was just updated
        // (i.e. is still frozen)
        if (esp_timer_get_time() < s_freeze_canvas_until) {
            esp_camera_fb_return(pic);
            continue;
        }

        // Draw camera image on the display
        if (bsp_display_lock(10)) {
            lv_canvas_copy_buf(s_camera_canvas, pic->buf, 0, 0, pic->width, pic->height);
            lv_obj_invalidate(s_camera_canvas);
            bsp_display_unlock();
        }

        // Send the frame to the processing task.
        // Note the short delay — if the processing task is busy, simply drop the frame.
        int res = xQueueSend(processing_queue, &pic, pdMS_TO_TICKS(10));
        if (res == pdFAIL) {
            esp_camera_fb_return(pic);
        }
    }
}

// Processing task: gets an image from the queue, runs QR code detection and recognition.
// If a QR code is detected, classifies the QR code and displays the result (color fill or a picture) on the display.
static void processing_task(void *arg)
{
    struct quirc *qr = quirc_new();
    assert(qr);

    int qr_width = IMG_WIDTH;
    int qr_height = IMG_HEIGHT;
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

        // Get the next frame from the queue
        int res = xQueueReceive(input_queue, &pic, portMAX_DELAY);
        assert(res == pdPASS);

        int64_t t_start = esp_timer_get_time();
        // Convert the frame to grayscale. We could have asked the camera for a grayscale frame,
        // but then the image on the display would be grayscale too.
        rgb565_to_grayscale_buf(pic->buf, qr_buf, qr_width, qr_height);

        // Return the frame buffer to the camera driver ASAP to avoid DMA errors
        esp_camera_fb_return(pic);

        // Process the frame. This step find the corners of the QR code (capstones)
        quirc_end(qr);
        ++frame;
        int64_t t_end_find = esp_timer_get_time();
        int count = quirc_count(qr);
        quirc_decode_error_t err = QUIRC_ERROR_DATA_UNDERFLOW;
        int time_find_ms = (int)(t_end_find - t_start) / 1000;
        ESP_LOGI(TAG, "QR count: %d   Heap: %d  Stack free: %d  time: %d ms",
                 count, heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 uxTaskGetStackHighWaterMark(NULL), time_find_ms);


        // If a QR code was detected, try to decode it:
        for (int i = 0; i < count; i++) {
            struct quirc_code code = {};
            struct quirc_data qr_data = {};
            // Extract raw QR code binary data (values of black/white modules)
            quirc_extract(qr, i, &code);
            quirc_flip(&code);

            // Decode the raw data. This step also performs error correction.
            err = quirc_decode(&code, &qr_data);
            int64_t t_end = esp_timer_get_time();
            int time_decode_ms = (int)(t_end - t_end_find) / 1000;
            ESP_LOGI(TAG, "Decoded in %d ms", time_decode_ms);
            if (err != 0) {
                ESP_LOGE(TAG, "QR err: %d", err);
            } else {
                // Indicate that we have successfully decoded something by blinking an LED
                bsp_led_set(BSP_LED_GREEN, true);

                if (strstr((const char *)qr_data.payload, "COLOR:") != NULL) {
                    // If the QR code contains a color string, fill the display with the same color
                    int r = 0, g = 0, b = 0;
                    sscanf((const char *)qr_data.payload, "COLOR:%02x%02x%02x", &r, &g, &b);
                    ESP_LOGI(TAG, "QR code: COLOR(%d, %d, %d)", r, g, b);
                    display_set_color(r, g, b);
                } else {
                    ESP_LOGI(TAG, "QR code: %d bytes: '%s'", qr_data.payload_len, qr_data.payload);
                    // Otherwise, use the rules defined in qrclass.txt to find the image to display,
                    // based on the kind of data in the QR code.
                    const char *filename = classifier_get_pic_from_qrcode_data((const char *) qr_data.payload);
                    if (filename == NULL) {
                        ESP_LOGI(TAG, "Classifier returned NULL");
                    } else {
                        ESP_LOGI(TAG, "Classified as '%s'", filename);
                        // The classifier should return the image file name to display.
                        display_set_icon(filename);
                    }
                }
                bsp_led_set(BSP_LED_GREEN, false);
            }
        }
    }
}


// Helper functions to convert an RGB565 image to grayscale
typedef union {
    uint16_t val;
    struct {
        uint16_t b: 5;
        uint16_t g: 6;
        uint16_t r: 5;
    };
} rgb565_t;

static uint8_t rgb565_to_grayscale(const uint8_t *img)
{
    uint16_t *img_16 = (uint16_t *) img;
    rgb565_t rgb = {.val = __builtin_bswap16(*img_16)};
    uint16_t val = (rgb.r * 8 + rgb.g * 4 + rgb.b * 8) / 3;
    return (uint8_t) MIN(255, val);
}

static void rgb565_to_grayscale_buf(const uint8_t *src, uint8_t *dst, int qr_width, int qr_height)
{
    for (size_t y = 0; y < qr_height; y++) {
        for (size_t x = 0; x < qr_width; x++) {
            dst[y * qr_width + x] = rgb565_to_grayscale(&src[(y * qr_width + x) * 2]);
        }
    }
}

// Fill the display with the specified color
static void display_set_color(int r, int g, int b)
{
    if (bsp_display_lock(100)) {
        s_freeze_canvas_until = esp_timer_get_time() + s_freeze_canvas_delay;
        lv_canvas_fill_bg(s_camera_canvas, lv_color_make(b, r, g), LV_OPA_COVER);
        lv_obj_invalidate(s_camera_canvas);
        bsp_display_unlock();
    }
}

// Display the specified image file
static void display_set_icon(const char *name)
{
    if (bsp_display_lock(100)) {
        s_freeze_canvas_until = esp_timer_get_time() + s_freeze_canvas_delay;
        lv_canvas_fill_bg(s_camera_canvas, lv_color_make(255, 255, 255), LV_OPA_COVER);
        lv_draw_img_dsc_t dsc;
        lv_draw_img_dsc_init(&dsc);

        char filename[32];
        snprintf(filename, sizeof(filename), "A:" CONFIG_BSP_SD_MOUNT_POINT "/%s", name);
        lv_canvas_draw_img(s_camera_canvas, (IMG_WIDTH - 192) / 2, (IMG_HEIGHT - 192) / 2, filename, &dsc);
        lv_obj_invalidate(s_camera_canvas);
        bsp_display_unlock();
    }
}
