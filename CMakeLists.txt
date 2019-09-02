cmake_minimum_required(VERSION 3.14)
project(lichtenstein_server)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# enable address and ub sanitizer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=undefined,address,integer")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=undefined,address,integer")

# include directories
include_directories(src)
include_directories(src/db)

# libraries in the "libs" folder
include_directories(libs/inih)
include_directories(libs)
include_directories(libs/cpptime)

# build all of the sources
add_executable(server
        src/db/Channel.cpp
        src/db/Channel.h
        src/db/DataStore.cpp
        src/db/DataStore.h
        src/db/Group.cpp
        src/db/Group.h
        src/db/Node.cpp
        src/db/Node.h
        src/db/Routine.cpp
        src/db/Routine.h
        src/EffectRunner.cpp
        src/EffectRunner.h
        src/Framebuffer.cpp
        src/Framebuffer.h
        src/HSIPixel.cpp
        src/HSIPixel.h
        src/main.cpp
        src/OutputMapper.cpp
        src/OutputMapper.h
        src/protocol/ProtocolHandler.cpp
        src/protocol/ProtocolHandler.h
        src/Routine.cpp
        src/Routine.h
        src/version.h
        src/api/IRequestHandler.h
        src/api/HandlerFactory.cpp
        src/api/HandlerFactory.h
        src/api/API.cpp
        src/api/API.h
        src/api/ClientHandler.cpp
        src/api/ClientHandler.h
        src/rt/IRequestHandler.h
        src/rt/HandlerFactory.cpp
        src/rt/HandlerFactory.h
        src/rt/API.cpp
        src/rt/API.h
        src/rt/ClientHandler.cpp
        src/rt/ClientHandler.h
        src/rt/PixelDataHandler.cpp
        src/rt/PixelDataHandler.h src/rt/handlers/JoinChannel.cpp src/rt/handlers/JoinChannel.h src/rt/handlers/ChannelDataAck.cpp src/rt/handlers/ChannelDataAck.h src/helpers/IPHelpers.cpp src/helpers/IPHelpers.h src/protocol/AdoptionHandler.cpp src/protocol/AdoptionHandler.h src/api/handlers/AuthHello.cpp src/api/handlers/AuthHello.h src/rt/handlers/AuthHello.cpp src/rt/handlers/AuthHello.h)


# get Git info and compile it into the binary
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

target_sources(server PRIVATE ${version_file})


# compile/link angelscript, and the add-ons wfe want
add_subdirectory(libs/angelscript/sdk/angelscript/projects/cmake)
target_link_libraries(server ${ANGELSCRIPT_LIBRARY_NAME})

include_directories(libs/angelscript/sdk/angelscript/include)
include_directories(libs/angelscript/sdk/add_on)

target_sources(server PRIVATE
        libs/angelscript/sdk/add_on/scriptbuilder/scriptbuilder.cpp

        libs/angelscript/sdk/add_on/scriptstdstring/scriptstdstring.cpp
        libs/angelscript/sdk/add_on/scriptstdstring/scriptstdstring_utils.cpp

        libs/angelscript/sdk/add_on/scriptarray/scriptarray.cpp

        libs/angelscript/sdk/add_on/scriptdictionary/scriptdictionary.cpp

        libs/angelscript/sdk/add_on/scriptmath/scriptmath.cpp
        libs/angelscript/sdk/add_on/scriptmath/scriptmathcomplex.cpp

        libs/angelscript/sdk/add_on/datetime/datetime.cpp

        libs/angelscript/sdk/add_on/debugger/debugger.cpp)


# JSON library
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(libs/json)

target_link_libraries(server nlohmann_json::nlohmann_json)

# stduuid library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/stduuid/include)

# link against SQLite
find_package(SQLite3 REQUIRED)
target_link_libraries(server SQLite::SQLite3)
# link against glog
find_package(glog REQUIRED)
target_link_libraries(server glog::glog)
# link against LibreSSL
find_package(LibreSSL REQUIRED)

if (APPLE)
    # a kind of nasty hack for macOS, otherwise it will link with system OpenSSL :(
    include_directories(BEFORE SYSTEM /usr/local/opt/libressl/include)
    target_link_libraries(server /usr/local/opt/libressl/lib/libcrypto.dylib /usr/local/opt/libressl/lib/libssl.dylib /usr/local/opt/libressl/lib/libtls.dylib)
else ()
    target_link_libraries(server LibreSSL::TLS)
endif ()

# lastly, link in all the pieces we need from the lichtenstein library
add_subdirectory(libs/liblichtenstein EXCLUDE_FROM_ALL)

target_link_libraries(server lichtensteinIo)
target_link_libraries(server lichtensteinProto)

#include_directories(libs/liblichtenstein)
include_directories(${CMAKE_BINARY_DIR}/libs/liblichtenstein/protocol)
