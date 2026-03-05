add_library(application STATIC AppEntry.cpp)
target_include_directories(
  application PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET application PROPERTY CXX_STANDARD 23)
target_link_libraries(application PRIVATE ithread plog::plog platform_core
                                          enocean_driver ible icrypto)
