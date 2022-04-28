set(COMPONENT_SRCS "qrcode_app_main.cpp"
"../lib/quirc_opencv.cpp"
"camera/who_camera.c"
"../quirc/lib/decode.c"
"../quirc/lib/identify.c"
"../quirc/lib/quirc.c"
"../quirc/lib/version_db.c"
)

set(COMPONENT_ADD_INCLUDEDIRS "" "."
"camera"
"../quirc/lib"
"../lib"
)                    

set(EXTRA_COMPONENT_DIRS "../components")

register_component()
