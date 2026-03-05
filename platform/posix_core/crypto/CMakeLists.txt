find_package(OpenSSL REQUIRED)

add_library(crypto STATIC Crypto.cpp)
target_include_directories(
  crypto PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(
  crypto
  PUBLIC icrypto
  PRIVATE OpenSSL::SSL OpenSSL::Crypto plog::plog)
set_property(TARGET crypto PROPERTY CXX_STANDARD 23)
