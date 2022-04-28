idf_component_register(SRCS "qrcode_app_main.cpp"
                    "../lib/quirc_opencv.cpp"
                    "camera/who_camera.c"
                    "../quirc/lib/decode.c"
                    "../quirc/lib/identify.c"
                    "../quirc/lib/quirc.c"
                    "../quirc/lib/version_db.c"

                    INCLUDE_DIRS "" "."
                    "camera"
                    "../quirc/lib"
                    "../lib"
                    REQUIRES opencvlib esp-dsp esp32-camera)
