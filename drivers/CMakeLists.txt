cmake_minimum_required(VERSION 2.6)

add_library(nusb
  usb/nusb.cpp
  usb/nusb.h
)

add_library(rtlsdr
  musen/librtlsdr.cc
  musen/reg_field.h
  musen/rtl-sdr.h
  musen/rtl-sdr_export.h
  musen/rtlsdr_i2c.h
  musen/tuner_e4k.cc
  musen/tuner_e4k.h
  musen/tuner_fc0012.cc
  musen/tuner_fc0012.h
  musen/tuner_fc0013.cc
  musen/tuner_fc0013.h
  musen/tuner_r82xx.cc
  musen/tuner_r82xx.h
)

target_link_libraries(rtlsdr nusb)

add_library(tenkey
  tenkey.cc
  tenkey.h
)

target_link_libraries(tenkey nusb)

add_library(rokujiku
  rokujiku.cc
  rokujiku.h
)

target_link_libraries(rokujiku nusb)

add_library(osemb
  osemb.cc
  osemb.h
)

add_library(webcam
  webcam.cc
  webcam.h
)

add_library(sdr
  sdr.cc
  sdr.h
)

target_link_libraries(sdr rtlsdr)

