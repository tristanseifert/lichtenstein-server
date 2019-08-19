# Lichtenstein Server
This is the C++ server that controls the networked Lichtenstein LED controllers. The server is controlled via a simple JSON API over a UNIX socket. Data is persisted to disk with a simple embedded sqlite3 database. All effects are enumerated and executed as needed, and updates sent to nodes as needed.

Effects themselves are implemented as simple [AngelScript](http://www.angelcode.com/angelscript/) functions.

## Building
Once all dependencies have been satisfied (remember to pull submodules and download the AngelScript SDK) you can invoke CMake to generate projects for your favorite 

Be sure you have sqlite3, [glog](https://github.com/google/glog), and [gflags](https://github.com/gflags/gflags) installed on the system, and in your include/link paths. You will also need whatever dependencies [lichtenstein-lib](https://github.com/tristanseifert/lichtenstein-lib) requires; namely, the protobuf runtime, [LibreSSL](https://www.libressl.org/), and [Catch2](https://github.com/catchorg/Catch2/) for tests.

### macOS
Install glog and gflags via Homebrew; then invoke CMake. Everything should compile without problems.

### FreeBSD
FreeBSD support is experimental, and may break unexpectedly.

You will most likely need to build glog and Catch2 from source and install them that way. The binary packages provided via ports do not install the CMake configuration scripts.
