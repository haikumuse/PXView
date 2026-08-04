#!/bin/bash
# =============================================================================
# package.sh — Windows MinGW dependency bundling for PXView
#
# Run from the repo root (inside MSYS2/MinGW64 shell):
#   bash window/package.sh
#
# Prerequisites:
#   - CMake build + install completed (install.dir/ must exist)
#   - MinGW64 environment active (/mingw64)
#   - Python embeddable zip downloaded to python/ (for stdlib .pyc)
# =============================================================================
set -e

rm -rf package
mkdir package
cd package

# --- PXView executable and resources ---
cp ../install.dir/bin/PXView.exe .
cp -r ../install.dir/share/PXView/* .
cp -r ../install.dir/share/libsigrokdecode/* .

# --- Resolve MinGW DLL dependencies via ldd ---
# This copies python314.dll, libgcc_s_seh-1.dll, libstdc++-6.dll, etc.
../window/copy-deps.sh PXView.exe /mingw64

# --- Qt6 plugins ---
mkdir -p plugins
cp -r /mingw64/share/qt6/plugins/* .
../window/copy-deps.sh imageformats/qsvg.dll /mingw64
../window/copy-deps.sh imageformats/qjpeg.dll /mingw64

# --- Python standard library ---
# Detect Python version from MinGW's python314.dll
PY_VER=$(python -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>/dev/null || echo "")
if [ -z "$PY_VER" ]; then
    # Fallback: extract from /mingw64/bin/python3*.dll filename
    PY_DLL=$(ls /mingw64/bin/python3*.dll 2>/dev/null | head -1)
    if [ -n "$PY_DLL" ]; then
        PY_BASE=$(basename "$PY_DLL" .dll)  # e.g. python314
        PY_MAJOR="${PY_BASE:6:1}"
        PY_MINOR="${PY_BASE:7}"
        PY_VER="${PY_MAJOR}.${PY_MINOR}"
    fi
fi

if [ -z "$PY_VER" ]; then
    echo "WARNING: Could not detect Python version, skipping stdlib"
else
    echo "Detected MinGW Python version: $PY_VER"

    # Extract stdlib from embeddable zip (contains pre-compiled .pyc files)
    PY_ZIP=$(ls ../python/python3*.zip 2>/dev/null | head -1)
    if [ -n "$PY_ZIP" ]; then
        echo "Extracting Python stdlib from: $PY_ZIP"
        mkdir -p "lib/python${PY_VER}"
        unzip -q "$PY_ZIP" -d "lib/python${PY_VER}/"
    else
        echo "WARNING: No python3*.zip found in python/, skipping stdlib .pyc"
    fi

    # Copy MinGW's compiled extension modules (.pyd)
    if [ -d "/mingw64/lib/python${PY_VER}/lib-dynload" ]; then
        echo "Copying MinGW Python extension modules (.pyd)"
        cp /mingw64/lib/python${PY_VER}/lib-dynload/*.pyd "lib/python${PY_VER}/" 2>/dev/null || true
    else
        echo "WARNING: /mingw64/lib/python${PY_VER}/lib-dynload not found"
    fi

    # Copy python3XX._pth if it exists (configures sys.path)
    PY_SHORT=$(echo "$PY_VER" | tr -d '.')
    if [ -f "../python/python${PY_SHORT}._pth" ]; then
        cp "../python/python${PY_SHORT}._pth" .
    fi
fi

# --- Web UI (Vite web client) ---
if [ -d ../web/dist ]; then
    mkdir -p webui
    cp -r ../web/dist/* webui/
fi
