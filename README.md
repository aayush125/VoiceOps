# VoiceOps

Supported environments:
- Windows, via MSYS2 UCRT64, MSYS2 MINGW64 or MSYS2 CLANG64. Server and Client are both supported.
- Linux, only GCC is tested. Server only.

## Setting Up Development Environment

### Windows

1. Install [MSYS2](https://www.msys2.org/).
    - Note that MSYS2 does not come with any build tools by itself, everything must be installed manually.
2. Open `MSYS2 CLANG64` from the start menu
3. Repeatedly run `pacman -Syuu` until all packages are up-to-date.
4. Install the build tools: 
    - `pacman -S --needed base-devel mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-gtk4 mingw-w64-clang-x86_64-gtkmm-4.0`

### Linux

Note that only the server is supported on Linux.

Make sure you have the following packages installed:
- `cmake`
- `pkg-config` or `pkgconf`
- `libglib2.0-dev`
- `libsqlite3-dev`
- and a C++ toolchain, such as `build-essential`

## Compilation

If you're on Windows, make sure to run the following commands from the correct MSYS2 environment (CLANG64 if you followed instructions above).

```bash
mkdir build
cd build
cmake ..
ninja # or make if ninja is not installed
```

To run the compiled client:

```bash
cd .. # Going back to repository root
./build/client
```

The server can be run with: `./build/server`

## Attributions
[Mic icons created by Ferdinand - Flaticon](https://www.flaticon.com/free-icons/mic)
