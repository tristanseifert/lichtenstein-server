cmake_minimum_required(VERSION 3.14)
project(lichtenstein_server)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# include directories (in our source)
include_directories(src)

# libraries in the "libs" folder (header only)
include_directories(libs/CTPL)
include_directories(libs/cpptime)
include_directories(libs/cxxopts/include)
include_directories(libs/sqlite_orm/include/sqlite_orm)
include_directories(libs/cpp-httplib)

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
add_executable(lichtenstein_server
    src/main.cpp
    src/ConfigManager.cpp
    src/Logging.cpp
    src/db/DataStore.cpp
    src/db/DataStorePrimitives.cpp
    src/db/DataStorePrimitives+Json.cpp
    src/api/Server.cpp
    src/api/HandlerFactory.cpp
    src/api/IController.cpp
    src/api/controllers/ServerInfo.cpp
    src/api/controllers/Routines.cpp
    src/api/controllers/Groups.cpp
    src/api/controllers/Nodes.cpp
    src/api/controllers/NodeChannels.cpp
    src/render/Framebuffer.cpp
    src/render/PixelTypes+Json.cpp
    src/render/Pipeline.cpp
    ${version_file} src/version.h)


# compile cap'n proto files
find_package(CapnProto REQUIRED)
target_include_directories(lichtenstein_server PRIVATE ${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})

capnp_generate_cpp(CAPNP_SOURCES CAPNP_HEADERS src/db/packed_v1.capnp)
target_sources(lichtenstein_server PRIVATE ${CAPNP_SOURCES} ${CAPNP_HEADERS})

# then, link against the cap'n proto runtime and include the generated headers
target_link_libraries(lichtenstein_server PRIVATE CapnProto::capnp)
target_include_directories(lichtenstein_server PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)

# link against SQLite
find_package(SQLite3 REQUIRED)
target_link_libraries(lichtenstein_server PRIVATE SQLite::SQLite3)

# for each of the below libraries we compile ourselves, try to prefer using
# static libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# link against libconfig
set(BUILD_EXAMPLES OFF CACHE BOOL "Enable examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Enable tests" FORCE)
add_subdirectory(libs/libconfig)
include_directories(libs/libconfig/lib)
target_link_libraries(lichtenstein_server PRIVATE config++)

# compile/link spdlog (logging)
add_subdirectory(libs/spdlog)
target_link_libraries(lichtenstein_server PRIVATE spdlog::spdlog)

# include the JSON library (without tests)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_MultipleHeaders ON CACHE INTERNAL "")

add_subdirectory(libs/json)
target_link_libraries(lichtenstein_server PRIVATE nlohmann_json::nlohmann_json)

# compile/link angelscript, and the add-ons we want
add_subdirectory(libs/angelscript/sdk/angelscript/projects/cmake)
target_link_libraries(lichtenstein_server PRIVATE ${ANGELSCRIPT_LIBRARY_NAME})

include_directories(libs/angelscript/sdk/angelscript/include)
include_directories(libs/angelscript/sdk/add_on)

target_sources(lichtenstein_server PRIVATE
    libs/angelscript/sdk/add_on/scriptbuilder/scriptbuilder.cpp

    libs/angelscript/sdk/add_on/scriptstdstring/scriptstdstring.cpp
    libs/angelscript/sdk/add_on/scriptstdstring/scriptstdstring_utils.cpp

    libs/angelscript/sdk/add_on/scriptarray/scriptarray.cpp

    libs/angelscript/sdk/add_on/scriptdictionary/scriptdictionary.cpp

    libs/angelscript/sdk/add_on/scriptmath/scriptmath.cpp
    libs/angelscript/sdk/add_on/scriptmath/scriptmathcomplex.cpp

    libs/angelscript/sdk/add_on/datetime/datetime.cpp

    libs/angelscript/sdk/add_on/debugger/debugger.cpp)
