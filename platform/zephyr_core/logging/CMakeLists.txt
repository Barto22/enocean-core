add_library(zephyr_logger STATIC ZephyrAppender.cpp)

set_property(TARGET zephyr_logger PROPERTY CXX_STANDARD 23)

target_include_directories(zephyr_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(zephyr_logger PUBLIC plog::plog zephyr)
