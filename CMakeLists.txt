cmake_minimum_required(VERSION 3.5)

# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-smart-player)
