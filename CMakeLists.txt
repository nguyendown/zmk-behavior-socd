zephyr_library()

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_SOCD src/behaviors/behavior_socd.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
endif()
