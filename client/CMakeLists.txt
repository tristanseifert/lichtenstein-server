cmake_minimum_required(VERSION 3.14)
project(lichtenstein_client)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 20)

# include directories (in our source)
include_directories(../shared/src)
include_directories(src)

# libraries in the "libs" folder (header only)
include_directories(../libs/cxxopts/include)
include_directories(../libs/cpp-httplib)
include_directories(../libs/stduuid/include)
include_directories(../libs/cista/include)

# compile in the git version file
include(GetGitRevisionDescription)
git_describe(VERSION --tags --dirty=-d)

get_git_head_revision(GIT_REFSPEC GIT_HASH)

string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.[0-9]+(.*)" "\\1" VERSION_SHA1 "${VERSION}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
set(version_file "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

# build all of the sources
add_executable(lichtenstein_client
    src/main.cpp
    ../shared/src/ConfigManager.cpp
    ../shared/src/Logging.cpp
    src/proto/Client.cpp
    src/proto/ClientIO.cpp
    src/output/PluginManager.cpp
    src/proto/MulticastReceiver.cpp
    ../shared/src/proto/MulticastCrypto.cpp
    ${version_file} src/version.h)


# find LibreSSL and link with it, with a fun Apple hack
find_package(LibreSSL REQUIRED)
if (APPLE)
    # force linking with the Homebrew version
    target_include_directories(lichtenstein_client SYSTEM BEFORE PUBLIC /usr/local/opt/libressl/include)
    target_link_libraries(lichtenstein_client PRIVATE /usr/local/opt/libressl/lib/libcrypto.dylib /usr/local/opt/libressl/lib/libssl.dylib /usr/local/opt/libressl/lib/libtls.dylib)
else ()
    target_link_libraries(lichtenstein_client PRIVATE LibreSSL::TLS)
endif()

# for each of the below libraries we compile ourselves, try to prefer using
# static libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# link against libconfig
set(BUILD_EXAMPLES OFF CACHE BOOL "Enable examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Enable tests" FORCE)
add_subdirectory(../libs/libconfig ${CMAKE_CURRENT_BINARY_DIR}/libs/libconfig)

target_include_directories(lichtenstein_client PRIVATE ../libs/libconfig/lib)
target_link_libraries(lichtenstein_client PRIVATE config++)

# fmt (string formatting; spdlog depends on it)
add_subdirectory(../libs/fmt ${CMAKE_CURRENT_BINARY_DIR}/libs/fmt)
target_link_libraries(lichtenstein_client PRIVATE fmt::fmt)

# compile/link spdlog (logging; use external libfmt)
find_package(spdlog REQUIRED)

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
target_link_libraries(lichtenstein_client PRIVATE spdlog::spdlog)

# include the JSON library (without tests)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_MultipleHeaders ON CACHE INTERNAL "")

add_subdirectory(../libs/json ${CMAKE_CURRENT_BINARY_DIR}/libs/json)
target_link_libraries(lichtenstein_client PRIVATE nlohmann_json::nlohmann_json)

# include the c++ base64 library
target_include_directories(lichtenstein_client PRIVATE ../libs/cpp-base64)

target_sources(lichtenstein_client PRIVATE
    ../libs/cpp-base64/base64.cpp
)


###############################################################################
add_library(plugin_dummy SHARED
    plugin/dummy/info.cpp
    plugin/dummy/DummyChannel.cpp
    ${version_file} src/version.h)

###############################################################################
add_library(plugin_ledchain SHARED
    plugin/ledchain/LedchainPlugin.cpp
    plugin/ledchain/LedchainChannel.cpp
    ${version_file} src/version.h)

###############################################################################
# RPi output plugin is compiled only on linux

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_library(plugin_rpi SHARED
        plugin/rpi/info.cpp
        plugin/rpi/Plugin.cpp
        plugin/rpi/OutputChannel.cpp

        # sources for the rpi_ws281x lib
        plugin/rpi/rpi_ws281x/mailbox.c
        plugin/rpi/rpi_ws281x/ws2811.c
        plugin/rpi/rpi_ws281x/pwm.c
        plugin/rpi/rpi_ws281x/pcm.c
        plugin/rpi/rpi_ws281x/dma.c
        plugin/rpi/rpi_ws281x/rpihw.c

        ${version_file} src/version.h)
    target_include_directories(plugin_rpi PRIVATE plugin/rpi/rpi_ws281x)
endif()
