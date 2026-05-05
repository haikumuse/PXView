@echo off
echo ==========================================
echo DSView Incremental Build Script
echo ==========================================

echo Stopping any running DSView processes...
powershell -Command "Stop-Process -Name DSView -Force -ErrorAction SilentlyContinue"

set MSYS2_PATH=D:\msys64
set PROJECT_ROOT=/c/Users/admin/Downloads/DSView-main_2026_4_27cppnb

echo Step 1/3: Starting MinGW64 environment...

%MSYS2_PATH%\msys2_shell.cmd -mingw64 -defterm -no-start -here -c "cd %PROJECT_ROOT% && if [ -d build ]; then cd build && echo 'Step 2/3: Incremental building...' && ninja -j 16 && echo 'Step 3/3: Installing...' && ninja install && echo '==========================================' && echo 'Incremental build completed!' && echo 'Executable: %PROJECT_ROOT%/install.dir/bin/DSView.exe' && echo '==========================================' ; else echo 'ERROR: build directory not found, please run build_full.cmd first' ; exit 1 ; fi"

pause
