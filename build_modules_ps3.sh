#!/bin/bash
set -e

if [ -z "$PS3DEV" ]; then
    echo "Please set PS3DEV"
    exit 1
fi

export RENPY_DEPS_INSTALL=$PS3DEV/portlibs/ppu
export PYGAME_SDL2_STATIC=1
export RENPY_STATIC=1
export RENPY_PS3=1
export CC=ppu-gcc
export CXX=ppu-g++
export LD=ppu-gcc
export AR=ppu-ar
export RANLIB=ppu-ranlib

# PS3 specific flags
export CFLAGS="-I$PS3DEV/ppu/include -I$PS3DEV/portlibs/ppu/include -I$PS3DEV/portlibs/ppu/include/python2.7 -D__PS3__ -O2"
export LDFLAGS="-L$PS3DEV/ppu/lib -L$PS3DEV/portlibs/ppu/lib"

echo "Generating C source for pygame_sdl2 modules..."
cd pygame_sdl2
python2 setup.py generate
cd ..

echo "Generating C source for renpy modules..."
cd renpy/module
python2 setup.py generate
cd ../..

echo "Done. Now you can use the Makefile.ps3 in renpy-vita directory."
