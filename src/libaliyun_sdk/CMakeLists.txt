##
# @file CMakeLists.txt
# @brief Aliyun SDK CMakeLists
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
aux_source_directory(${MODULE_PATH}/src LIB_SRCS)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

# LIB_PRIVATE_INC
set(LIB_PRIVATE_INC ${MODULE_PATH}/include/c_utils)

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

# Link with TuyaOpen TAL libraries
target_link_libraries(${MODULE_NAME}
    PRIVATE
        tal_system
        tal_kv
    )

# Test library is linked directly below

# Create a combined library with all prebuilt libraries
add_library(${MODULE_NAME}_prebuilt STATIC IMPORTED)
set_target_properties(${MODULE_NAME}_prebuilt PROPERTIES
    IMPORTED_LOCATION ${MODULE_PATH}/prebuilt_libs/libaliyun_sdk.a
)

# Link the main library with prebuilt libraries
target_link_libraries(${MODULE_NAME}
    PUBLIC
        ${MODULE_NAME}_prebuilt
        ${MODULE_PATH}/prebuilt_libs/libsdk_test.a
        ${MODULE_PATH}/prebuilt_libs/libcjson.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_application.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_brightness.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_camera.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_device.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_multimedia.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_music.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_recording.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_screen.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_telephone.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_vl.a
        ${MODULE_PATH}/prebuilt_libs/libcmd_volume.a
        ${MODULE_PATH}/prebuilt_libs/libhal.a
        ${MODULE_PATH}/prebuilt_libs/libtinycrypt.a
    )

# Test library is already linked above

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
