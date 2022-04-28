/* QR code scanner example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include <iostream>



#include <unistd.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "sdkconfig.h"
#include "quirc_opencv.h"


#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_camera.h"
#include "who_camera.h"

#include "esp_dsp.h"

static QueueHandle_t xQueueAIFrame1 = NULL;
static QueueHandle_t xQueueAIFrame2 = NULL;

#include "perfmon.h"

static const char* TAG = "QRcode-app";
#include "quirc.h"

#define CAM_W 640
#define CAM_H 480

#include "quirc_internal.h"

void process_task(void* data)
{
	QueueHandle_t xQueueAIFrame = (QueueHandle_t)data;

    camera_fb_t *frame = NULL;
	struct quirc_code* code;
	struct quirc_data* qr_data;

	quirc_opencv q_cv;
	int w = 640;
	int h = 480;
	struct quirc* qr;
	qr = quirc_new();
	if (quirc_resize(qr, w, h) < 0) {
		printf("couldn't allocate QR buffer\n");
	}
	int temp_count = 0;
	code = new quirc_code();
	qr_data = new quirc_data();

	while (true)
	{
		if (xQueueReceive(xQueueAIFrame, &frame, portMAX_DELAY))
		{
			unsigned int start_b = xthal_get_ccount();
			uint8_t *buf = quirc_begin(qr, &w, &h);

			for (size_t y = 0; y < h; y++)
			{
				for (size_t x = 0; x < w; x++)
				{
					buf[y*w + x] = frame->buf[y*frame->width + x];
				}
			}
			temp_count++;
			quirc_end(qr);
			q_cv.ConvertImage(qr);
			memset(code, 0, sizeof(quirc_code));
			int count = quirc_count(qr);
			quirc_decode_error_t err = QUIRC_ERROR_DATA_UNDERFLOW;

			for (int i = 0; i < count; i++) {
				quirc_extract(qr, i, code);

				err = quirc_decode(code, qr_data);
				unsigned int end_b = xthal_get_ccount();
				if (err == 0)
				{
					if (strstr((const char *)qr_data->payload, "RED") != NULL)
					{
						ESP_LOGE(TAG,"QR-code[%i]: %s, time = %i(ms)", qr_data->payload_len, qr_data->payload, (end_b - start_b)/240000);
					}
					else if (strstr((const char *)qr_data->payload, "YELLOW") != NULL)
					{
						ESP_LOGW(TAG,"QR-code[%i]: %s, time = %i(ms)", qr_data->payload_len, qr_data->payload, (end_b - start_b)/240000);
					}
					else if (strstr((const char *)qr_data->payload, "GREEN") != NULL)
					{
						ESP_LOGI(TAG,"QR-code[%i]: %s, time = %i(ms)", qr_data->payload_len, qr_data->payload, (end_b - start_b)/240000);
					} else
					{
						printf("QR-code[%i]: %s, time = %i(ms)\n", qr_data->payload_len, qr_data->payload, (end_b - start_b)/240000);
					}
				}
			}
			esp_camera_fb_return(frame);
		}
	}
}

extern "C" void app_main(void);

void app_main(void)
{
    ESP_LOGI(TAG, "Start");

    xQueueAIFrame1 = xQueueCreate(2, sizeof(camera_fb_t *));
    xQueueAIFrame2 = xQueueCreate(2, sizeof(camera_fb_t *));

    register_camera(PIXFORMAT_GRAYSCALE, FRAMESIZE_VGA, 2, xQueueAIFrame1, xQueueAIFrame2);
    xTaskCreatePinnedToCore(process_task, TAG, 16 * 1024, xQueueAIFrame1, 5, NULL, 0);
    xTaskCreatePinnedToCore(process_task, TAG, 16 * 1024, xQueueAIFrame2, 5, NULL, 1);

	return;
}
