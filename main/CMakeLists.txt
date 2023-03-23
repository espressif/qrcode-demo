idf_component_register(SRCS "qrcode_app_new.c"
                            "qrcode_classifier.c"
                       INCLUDE_DIRS "" "."
                       PRIV_REQUIRES esp_timer pbm vfs fatfs)

target_compile_options(${COMPONENT_LIB} PRIVATE -O3)

idf_component_get_property(quirc_lib_name espressif__quirc COMPONENT_LIB)
target_compile_options(${quirc_lib_name} PRIVATE -O3)
