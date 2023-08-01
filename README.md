# ESP32-S3 QR Code Recognition Demo

This example project demonstrates how an ESP32-S3 can be used to scan QR codes.

This demo is an [ESP-IDF](https://github.com/espressif/esp-idf) application. Aside from ESP-IDF, it relies on several components from [IDF Component Registry](https://components.espressif.com):

* [espressif/esp32-camera](https://components.espressif.com/components/espressif/esp32-camera) — to obtain an image from the camera
* [espressif/esp32_s3_eye](https://components.espressif.com/components/espressif/esp32_s3_eye) — board support package for ESP32-S3-EYE board
* [espressif/quirc](https://components.espressif.com/components/espressif/quirc) — QR code decoding library
* [lvgl/lvgl](https://components.espressif.com/components/lvgl/lvgl) — for the UI

## Hardware Required

This demo runs on an [ESP32-S3-EYE](https://github.com/espressif/esp-who/blob/master/docs/en/get-started/ESP32-S3-EYE_Getting_Started_Guide.md) board. With some work, it can be adapted to other ESP32 and ESP32-S3 boards.

This demo also requires an SD card.

## Building and Running

This demo has been tested with ESP-IDF v5.2-rc1.

To build, activate ESP-IDF build environment and run:
```bash
idf.py build
```

To flash the demo to the board and see the console output, run:
```bash
idf.py flash monitor
```

## Expected output

The demo will capture frames from the camera and decode them. Results of the decoding process will be shown in console (`idf.py monitor`).

When no QR code is found in the frame, the following line will be printed in console:
```
I (64266) example: QR count: 0   Heap: 8086720  Stack free: 11692  time: 22 ms
```

When a QR code is detected, the data embedded into QR code will be shown:
```
I (64823) example: QR count: 1   Heap: 8086720  Stack free: 11692  time: 229 ms
I (64827) example: Decoded in 3 ms
I (64827) example: QR code: 62 bytes: 'https://www.espressif.com/en/products/devkits/esp-eye/overview'
```

## Preparing the SD Card (optional)

For visual feedback, this demo can also display an image on the LCD, based on the contents of the QR code. For example, you can show a "Wi-Fi" icon when the QR code contains information about a Wi-Fi AP credentials.

To use this feature, you need to prepare an SD card and put a few files there.

Make sure the SD card is formatted as FAT, and place the following files on the card:

1. `qrclass.txt` with the list of QR code classification rules. Each rule is a pair of `<regular expression> <png file name>`. If the QR code matches the `<regular expression>` then the demo will display the corresponding PNG file on the screen.
   ```
   ^:WiFi.* wifi.png
   .* unknown.png
   ```
2. For each classifier, a PNG file with a 192x192 image. With the example above, `wifi.png` and `unknown.png`.

If SD card is not detected when the demo starts up, it will proceed without this feature.

## License

The code in this repository is Copyright (c) 2022-2023 Espressif Systems (Shanghai) Co. Ltd. and licensed under Apache 2.0 license.

