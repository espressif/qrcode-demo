idf_component_register(SRCS "qrcode_app_new.c"
                    "../quirc/lib/decode.c"
                    "../quirc/lib/identify.c"
                    "../quirc/lib/quirc.c"
                    "../quirc/lib/version_db.c"
                    "qrcode_classifier.c"

                    INCLUDE_DIRS "" "."
                    "../quirc/lib"

                    PRIV_REQUIRES esp_timer pbm vfs fatfs)


target_compile_definitions(${COMPONENT_LIB} PRIVATE QUIRC_USE_TGMATH)
target_compile_definitions(${COMPONENT_LIB} PRIVATE QUIRC_FLOAT_TYPE=float)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
