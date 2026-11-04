if(CONFIG_ZMK_DISPLAY AND CONFIG_MACROPAD2_RAIN_SCREEN)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(rain_screen.c)
endif()
