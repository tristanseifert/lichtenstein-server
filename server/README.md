# Lichtenstein Server
This is the C++ server that controls the networked Lichtenstein LED controllers. The server is controlled via a simple JSON API over a UNIX socket. Data is persisted to disk with a simple embedded sqlite3 database. All effects are enumerated and executed as needed, and updates sent to nodes as needed.

Effects themselves are implemented as simple [AngelScript](http://www.angelcode.com/angelscript/) functions.

## Building
Once all dependencies have been satisfied (remember to pull submodules) you can invoke CMake to generate projects for your favorite build system. 

Be sure you have sqlite3 installed on the system, and in your include/link paths. You will also need [LibreSSL](https://www.libressl.org/) and the [Cap'n Proto](https://capnproto.org/) runtime for network communication. These are the only external dependencies; all others are compiled automagically as part of the build process.

As an example, you might do the following to build:
```
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
make
```

### macOS
Install libressl and capnp via Homebrew; then invoke CMake. Everything should compile without problems.

### FreeBSD
FreeBSD support is experimental, and may break unexpectedly.
