#!/bin/bash

echo "=========================================="
echo "PXView Incremental Build Script (MinGW64)"
echo "=========================================="

# Stop any running PXView processes
echo "Stopping any running PXView processes..."
taskkill //F //IM PXView.exe 2>/dev/null || true

PROJECT_ROOT="/c/Users/admin/Downloads/DSView-main_2026_4_27cppnb"

echo "Step 1/3: Checking build directory..."

if [ ! -d "$PROJECT_ROOT/build" ]; then
    echo "ERROR: build directory not found, please run build_full.sh first"
    exit 1
fi

cd "$PROJECT_ROOT/build"

echo "Step 2/3: Incremental building..."
ninja -j 16
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed!"
    exit 1
fi

echo "Step 3/3: Installing..."
ninja install
if [ $? -ne 0 ]; then
    echo "ERROR: Install failed!"
    exit 1
fi

echo "=========================================="
echo "Incremental build completed!"
echo "Executable: $PROJECT_ROOT/install.dir/bin/PXView.exe"
echo "=========================================="

"$PROJECT_ROOT/install.dir/bin/PXView.exe" &
   
