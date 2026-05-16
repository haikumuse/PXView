@echo off
echo ==========================================
echo PXView Full Build Script
echo ==========================================

echo Stopping any running PXView processes...
powershell -Command "Stop-Process -Name PXView -Force -ErrorAction SilentlyContinue"

set MSYS2_PATH=D:\msys64
set PROJECT_ROOT=/c/Users/admin/Downloads/DSView-main_2026_4_27cppnb

echo Step 1/5: Starting MinGW64 environment...

%MSYS2_PATH%\msys2_shell.cmd -mingw64 -defterm -no-start -here -c "cd %PROJECT_ROOT% && echo 'Step 2/5: Cleaning old build...' && rm -rf build && rm -rf install.dir && mkdir build && mkdir install.dir && cd build && echo 'Step 3/5: Running CMake...' && cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install.dir -G Ninja && echo 'Step 4/5: Building...' && ninja -j 16 && echo 'Step 5/5: Installing...' && ninja install && echo '==========================================' && echo 'Build completed!' && echo 'Executable: %PROJECT_ROOT%/install.dir/bin/PXView.exe' && echo '=========================================='"

pause
