##
# @file CMakeLists.txt
# @brief Minimal CMakeLists for libsdk_test.a and include only
#/

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(MODULE_NAME ${PROJECT_NAME})

add_library(${MODULE_NAME} STATIC IMPORTED)
set_target_properties(${MODULE_NAME} PROPERTIES
    IMPORTED_LOCATION ${MODULE_PATH}/prebuilt_libs/libsdk_test.a
)

target_include_directories(${MODULE_NAME} INTERFACE /include)
