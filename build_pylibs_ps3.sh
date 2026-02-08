#!/bin/bash
# Script to package Python and Ren'Py for PS3
if [ -z "$PORTLIBS" ]; then
    if [ -n "$PS3DEV" ]; then
        PORTLIBS=$PS3DEV/portlibs/ppu
    elif [ -n "$PSL1GHT" ]; then
        PORTLIBS=$PSL1GHT/portlibs/ppu
    else
        echo "Please set PORTLIBS, PS3DEV, or PSL1GHT environment variable."
        exit 1
    fi
fi

echo "Using PORTLIBS at $PORTLIBS"

mkdir -p tmp_build_ps3
rm -rf tmp_build_ps3/*

# Copy Python standard library contents
cp -r $PORTLIBS/lib/python2.7/. tmp_build_ps3/
rm -rf tmp_build_ps3/test
rm -rf tmp_build_ps3/lib2to3/tests

# Copy pygame_sdl2 and renpy modules
cp -r pygame_sdl2/src/pygame_sdl2 tmp_build_ps3/
cp -r renpy/renpy tmp_build_ps3/

pushd tmp_build_ps3
# Compile to bytecode
python2 -OO -m compileall .
# Remove source files and keep only .pyc
find . -name "*.py" -delete
# Create the zip
zip -r ../python27.zip .
popd

rm -rf tmp_build_ps3
echo "python27.zip created successfully."
echo "Now follow these steps:"
echo "1. Create a folder named USRDIR/lib/ on your PS3/Emulator."
echo "2. Copy python27.zip to USRDIR/lib/python27.zip"
echo "3. Copy renpy/renpy.py to USRDIR/renpy.py"
echo "4. Copy renpy/renpy/common directory to USRDIR/renpy/common"
