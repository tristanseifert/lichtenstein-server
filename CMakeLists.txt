cmake_minimum_required(VERSION 3.14)
project(lichtenstein_server)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# include directories (in our source)
include_directories(src)

# libraries in the "libs" folder (header only)
include_directories(libs/CTPL)
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
        src/main.cpp
        ${version_file} src/version.h)

# compile libconfig and link against it
set(BUILD_EXAMPLES OFF CACHE BOOL "Enable examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Enable tests" FORCE)
add_subdirectory(libs/libconfig)
target_link_libraries(server config)

include_directories(libs/libconfig/lib)

# compile/link angelscript, and the add-ons we want
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


# link against SQLite
find_package(SQLite3 REQUIRED)
target_link_libraries(server SQLite::SQLite3)
# link against glog
find_package(glog REQUIRED)
target_link_libraries(server glog::glog)

