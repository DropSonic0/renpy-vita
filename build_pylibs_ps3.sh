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

# Diagnostic: show where Python.h is to help find the library
PYTHON_H_PATH=$(find $PORTLIBS -name "Python.h" 2>/dev/null | head -n 1)
if [ -n "$PYTHON_H_PATH" ]; then
    echo "Found Python.h at: $PYTHON_H_PATH"
fi

mkdir -p tmp_build_ps3
rm -rf tmp_build_ps3/*

# Copy Python standard library contents
# Try common paths for PS3 portlibs
if [ -d "$PORTLIBS/lib/python2.7" ]; then
    cp -r $PORTLIBS/lib/python2.7/. tmp_build_ps3/
elif [ -d "$PORTLIBS/python/lib/python2.7" ]; then
    cp -r $PORTLIBS/python/lib/python2.7/. tmp_build_ps3/
else
    echo "Warning: Python 2.7 standard library not found in PORTLIBS. zip may be incomplete."
fi

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
# Create the zip (using python's zipfile module if zip is missing)
if command -v zip >/dev/null 2>&1; then
    zip -r ../python27.zip .
elif command -v 7z >/dev/null 2>&1; then
    7z a -tzip ../python27.zip .
else
    echo "Using python to create zip..."
    python2 -c "import zipfile, os; z = zipfile.ZipFile('../python27.zip', 'w', zipfile.ZIP_DEFLATED); [z.write(os.path.join(root, f)) for root, dirs, files in os.walk('.') for f in files]; z.close()"
fi
popd

rm -rf tmp_build_ps3
echo "python27.zip created successfully."
echo "Now follow these steps:"
echo "1. Create a folder named USRDIR/lib/ on your PS3/Emulator."
echo "2. Copy python27.zip to USRDIR/lib/python27.zip"
echo "3. Copy renpy/renpy.py to USRDIR/renpy.py"
echo "4. Create USRDIR/renpy folder and copy renpy/renpy/common directory to USRDIR/renpy/common"

if [ ! -f "renpy/renpy.py" ]; then
    echo "Warning: renpy/renpy.py not found. Make sure you have the submodules updated."
fi
