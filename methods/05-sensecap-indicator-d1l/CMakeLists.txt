# Top-level ESP-IDF project for the SenseCAP Indicator D1L fleet-health monitor.
# Requires ESP-IDF v5.1.x.  Target: esp32s3.
#
#   . $IDF_PATH/export.sh
#   idf.py set-target esp32s3
#   idf.py menuconfig      # set Wi-Fi SSID/PSK + broker host/port
#   idf.py build
#   idf.py -p /dev/ttyACM0 flash monitor
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(fleet_health_indicator)
