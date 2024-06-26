# VoiceOps

Supported environments:
- Windows, via MSYS2 UCRT64 or MSYS2 MINGW64

## Setting Up Development Environment

1. Install [MSYS2](https://www.msys2.org/).
    - Note that MSYS2 does not come with any build tools by itself, everything must be installed manually.
2. Open `MSYS2 UCRT64` from the start menu
3. Repeatedly run `pacman -Syuu` until all packages are up-to-date.
4. Install the build tools: 
    - `pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain`
    - `pacman -S mingw-w64-ucrt-x86_64-gtk4 mingw-w64-ucrt-x86_64-gtkmm-4.0`

## Compilation

Run `make` in MSYS2, then run `./bin/voiceops`.

You can also run `make clean` to clean-up build artifacts, like the `.o` files and the executable.

## Attributions
[Mic icons created by Ferdinand - Flaticon](https://www.flaticon.com/free-icons/mic)