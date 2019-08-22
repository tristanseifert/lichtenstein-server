cmake_minimum_required(VERSION 3.14)
project(lichtenstein_server)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# include directories
include_directories(src)
include_directories(src/crc32)
include_directories(src/db)
include_directories(src/libb64)

# libraries in the "libs" folder
include_directories(libs/inih)
include_directories(libs)
include_directories(libs/cpptime)

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
add_executable(server
        src/crc32/crc32.cpp
        src/crc32/crc32.h
        src/crc32/crclut.h
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
        src/libb64/cdecode.c
        src/libb64/cdecode.h
        src/libb64/cencode.c
        src/libb64/cencode.h
        src/libb64/decode.h
        src/libb64/encode.h
        src/CommandServer.cpp
        src/CommandServer.h
        src/EffectRunner.cpp
        src/EffectRunner.h
        src/Framebuffer.cpp
        src/Framebuffer.h
        src/HSIPixel.cpp
        src/HSIPixel.h
        src/lichtenstein_proto.h
        src/LichtensteinUtils.cpp
        src/LichtensteinUtils.h
        src/main.cpp
        src/NodeDiscovery.cpp
        src/NodeDiscovery.h
        src/OutputMapper.cpp
        src/OutputMapper.h
        src/ProtocolHandler.cpp
        src/ProtocolHandler.h
        src/Routine.cpp
        src/Routine.h
        ${version_file} src/version.h)


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

# link against SQLite
find_package(SQLite3 REQUIRED)
target_link_libraries(server SQLite::SQLite3)
# link against glog
find_package(glog REQUIRED)
target_link_libraries(server glog::glog)

# lastly, link in all the pieces we need from the lichtenstein library
add_subdirectory(libs/liblichtenstein EXCLUDE_FROM_ALL)

target_link_libraries(server lichtensteinIo)
target_link_libraries(server lichtensteinProto)

include_directories(libs/liblichtenstein/client)
include_directories(libs/liblichtenstein/protocol)
include_directories(${CMAKE_BINARY_DIR}/libs/liblichtenstein/protocol)
