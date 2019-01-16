# Lichtenstein Server
This is the C++ server that controls the networked Lichtenstein LED controllers. The server is controlled via a simple JSON API over a UNIX socket. Data is persisted to disk with a simple embedded sqlite3 database. All effects are enumerated and executed as needed, and updates sent to nodes as needed.

Effects themselves are implemented as simple [AngelScript](http://www.angelcode.com/angelscript/) functions.

## Building
Once all dependencies have been satisfied (remember to pull submodules and download the AngelScript SDK and compile it) it should be as simple as running `make` in the root directory. (Note that the Makefile requires GNU Make.)

Be sure you have sqlite3, [glog](https://github.com/google/glog), and [gflags](https://github.com/gflags/gflags) installed on the system, and in your include/link paths. (Set the variables `INC_DIRS` and `LIB_DIRS` before running make, if needed.)

### macOS
Install glog and gflags via Homebrew; then run `make`. Everything should compile without problems.

### FreeBSD
Currently, the server doesn't quite compile on FreeBSD due to some missing features/differences from macOS. (See #1)
