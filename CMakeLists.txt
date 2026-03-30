cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    components
    components/esp-who/components/who_peripherals/who_cam
    components/esp-who/components/who_peripherals/who_usb
    components/esp-who/components/who_task
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(face_detector)
