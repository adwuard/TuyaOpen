##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC
aux_source_directory(${APP_PATH}/src APP_SRC)

# Add minizip source files (exclude main programs)
list(APPEND APP_SRC
    ${APP_PATH}/src/libminizip/unzip.c
    ${APP_PATH}/src/libminizip/ioapi.c
)

# APP_INC
set(APP_INC ${APP_PATH}/include)
set(APP_INC ${APP_INC} ${APP_PATH}/src/libminizip)
set(APP_INC ${APP_INC} ${APP_PATH}/src/zlib)  # For zlib.h

# APP_OPTIONS
set(APP_OPTIONS "")  # -W

########################################
# Build zlib from source
########################################
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build zlib shared library" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build zlib static library" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Build zlib tests" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "Install zlib" FORCE)
add_subdirectory(${APP_PATH}/src/zlib)

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
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )

# Link zlib (built from source)
target_link_libraries(${EXAMPLE_LIB} zlibstatic)
# Set INTERFACE properties so zlib dependency propagates to executables
set_target_properties(${EXAMPLE_LIB} PROPERTIES
    INTERFACE_LINK_LIBRARIES "zlibstatic;z"
    LINK_INTERFACE_LIBRARIES "zlibstatic;z"
)
