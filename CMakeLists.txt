cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(BLUEPAD32_ROOT $ENV{HOME}/esp/bluepad32)
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)

set(EXTRA_COMPONENT_DIRS ${BLUEPAD32_ROOT}/src/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rc_xbox_controller)
