zephyr_include_directories(.)
zephyr_include_directories(${CMAKE_BINARY_DIR})

target_sources(
  app
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app_version.c
)
