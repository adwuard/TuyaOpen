##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC
# aux_source_directory(${APP_PATH}/src APP_SRC)

# APP_INC
set(APP_INC ${APP_PATH}/include)

# APP_OPTIONS
set(APP_OPTIONS "")  # -W

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
        ${CMAKE_SOURCE_DIR}/src/libaliyun_sdk/include
        ${CMAKE_SOURCE_DIR}/src/libaliyun_sdk_test/include
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )

# Link with libaliyun_sdk and the specified static test library
target_link_libraries(${EXAMPLE_LIB} 
    libaliyun_sdk
    libaliyun_sdk_test
)


# Test library is now available in platform's lib directory