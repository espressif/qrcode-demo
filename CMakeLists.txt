# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(qrcode_app)

#target_add_binary_data(qrcode_app.elf "main/data/qrcode_test1.bin" TEXT)
#target_add_binary_data(qrcode_app.elf "main/data/qrcode_test2.bin" TEXT)
