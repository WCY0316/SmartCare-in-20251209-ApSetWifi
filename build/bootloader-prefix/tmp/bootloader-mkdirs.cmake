# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/chunyu/esp/esp-idf-v5.1.6/components/bootloader/subproject"
  "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader"
  "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix"
  "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix/tmp"
  "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix/src/bootloader-stamp"
  "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix/src"
  "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/chunyu/ESP32_WORKSPACE/SmartCare-in-20251209-ApSetWifi/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
