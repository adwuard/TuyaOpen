##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

list(APPEND media_srcs "${APP_PATH}/src/media")

list(APPEND APP_SRCS ${media_srcs})

# Add sensor integration sources if enabled
if (CONFIG_ENABLE_BNO08X_SENSOR STREQUAL "y" OR CONFIG_ENABLE_BMM150_SENSOR STREQUAL "y" OR CONFIG_ENABLE_GPS_LC76G STREQUAL "y" OR CONFIG_ENABLE_ENCODER_INPUT STREQUAL "y")
    message(STATUS "Adding sensor integration sources")
    aux_source_directory(${APP_PATH}/src/sensors SENSOR_SRCS)
    list(APPEND APP_SRCS ${SENSOR_SRCS})
endif()

set(APP_INC 
            ${APP_PATH}/assets
            ${APP_PATH}/include
            ${APP_PATH}/include/media
            ${APP_PATH}/src/sensors
)

list(APPEND APP_INC "${APP_PATH}/assets")

# cloud api
if (CONFIG_ENABLE_CLOUD_API STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/cloud_api CLOUD_API_SRCS)
    list(APPEND APP_SRCS ${CLOUD_API_SRCS})
    list(APPEND APP_INC "${APP_PATH}/src/cloud_api")
endif()

# dp
list(APPEND APP_INC "${APP_PATH}/src/dp")
aux_source_directory(${APP_PATH}/src/dp DP_SRCS)
list(APPEND APP_SRCS ${DP_SRCS})

# battery
if (CONFIG_ENABLE_BATTERY STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/battery BATTERY_SRCS)
    list(APPEND APP_SRCS ${BATTERY_SRCS})
    list(APPEND APP_INC "${APP_PATH}/src/battery")
endif()

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

########################################
# Add subdirectory
########################################

if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")
        add_subdirectory(${APP_PATH}/src/display)
endif()

add_subdirectory(${APP_PATH}/../../tuya.ai/ai_components/ai_audio)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/../ai_components/ai_audio)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/../../../src/peripherals/encoder)