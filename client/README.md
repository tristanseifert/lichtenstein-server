# Lichtenstein Client
A relatively basic C++ implementation of a Lichtenstein LED client. It connects to a server (see `server` directory,) authenticates and can then receive pixel data. Internally, one or more output plugins are set up (or, in some cases, automagically detected) in the configuration which actually send this data to physical LEDs.

## Building
Once all dependencies have been satisfied (remember to pull submodules) you can invoke CMake to generate projects for your favorite build system. 

You will need [LibreSSL](https://www.libressl.org/) for network communication. These are the only system-wide dependencies required; all others are provided as submodules.

As an example, you might do the following to build:
```
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
make
```

### macOS
Install libressl via Homebrew; then invoke CMake. Everything should compile without problems.
