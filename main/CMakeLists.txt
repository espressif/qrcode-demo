idf_component_register(SRCS "qrcode_app_main.cpp"
                    "../lib/quirc_opencv.cpp"
                    "../quirc/lib/decode.c"
                    "../quirc/lib/identify.c"
                    "../quirc/lib/quirc.c"
                    "../quirc/lib/version_db.c"
                    # lcd.c camera.c
                    lcd.c camera/who_camera.c

                    INCLUDE_DIRS "" "."
                    "../quirc/lib"
                    "../lib"
                    camera
                    REQUIRES opencvlib esp-dsp esp32-camera esp_lcd
                    PRIV_REQUIRES esp_timer)
