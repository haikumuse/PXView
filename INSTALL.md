# INSTALL

## Requirements
- git
- gcc (>= 9.0) or clang
- g++
- make
- cmake >= 3.16
- ninja-build
- Qt >= 6.11.0 (Core, Gui, Widgets, Svg, Concurrent)
- libglib >= 2.32.0
- zlib
- libusb-1.0 >= 1.0.16
- libboost >= 1.42
- libfftw3 >= 3.3
- python >= 3.8
- pkg-config >= 0.22

## Building and installing

### Step 1: Installing the requirements

#### Ubuntu / Debian (e.g. Ubuntu 22.04 / 24.04):
```bash
sudo apt update
sudo apt install git gcc g++ make cmake ninja-build libglib2.0-dev zlib1g-dev libusb-1.0-0-dev libboost-dev libfftw3-dev python3-dev libudev-dev pkg-config libgl1-mesa-dev libxkbcommon-dev libvulkan-dev python3-pip
```

**How to install Qt 6.11 on Ubuntu:**
The default apt repository may not provide Qt 6.11, so you must install it manually using `aqtinstall`:
```bash
pip3 install aqtinstall
# Install Qt 6.11 to the current directory's 'Qt' folder
aqt install-qt linux desktop 6.11.0 linux_gcc_64 --outputdir ./Qt
```

#### Fedora:
```bash
sudo dnf install git gcc gcc-c++ make cmake ninja-build libtool pkgconf glib2-devel zlib-devel libudev-devel libusb1-devel python3-devel boost-devel fftw-devel qt6-qtbase-devel qt6-qtsvg-devel
```
*(Fedora typically provides recent Qt6 versions in its standard repositories)*

#### Arch Linux:
```bash
sudo pacman -S base-devel git cmake ninja glib2 zlib libusb python boost qt6-base qt6-svg fftw
```

#### macOS (Homebrew):
```bash
brew install git cmake ninja gettext glib libusb zlib boost fftw python3 qt pkg-config
```
*(Note: If the default `qt` brew formula is not 6.11.0 yet, or if it isn't automatically linked, you may need to find the brew Qt installation path, typically `/opt/homebrew/opt/qt`)*

### Step 2: Get the PXView source code
```bash
git clone https://github.com/PXLogic/PXView
cd PXView
```

### Step 3: Building

If you installed Qt manually via `aqtinstall` (e.g. on Ubuntu) in Step 1, you must tell CMake where to find it. Otherwise, if you used system packages (Arch/Fedora/macOS), you can omit the `CMAKE_PREFIX_PATH` flag.

```bash
mkdir build && cd build

# For Ubuntu with aqtinstall:
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="$(pwd)/../Qt/6.11.0/gcc_64"

# For Arch / Fedora / macOS (System Qt):
# cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

ninja
DESTDIR=../install.dir ninja install
```

### Step 4: Packaging as AppImage (Linux)
If you want to bundle the compiled binary and all its Qt dependencies into a single, portable `.AppImage` file, you can use linuxdeploy on the install directory:

```bash
# Go back to the project root directory
cd ..
wget -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
wget -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy*.AppImage

# Explicitly tell linuxdeploy where the Qt 6.11 binaries are located.
# Adjust the QMAKE and LD_LIBRARY_PATH if your Qt installation is elsewhere.
export QMAKE="$(pwd)/Qt/6.11.0/gcc_64/bin/qmake"
export LD_LIBRARY_PATH="$(pwd)/Qt/6.11.0/gcc_64/lib:$LD_LIBRARY_PATH"
export OUTPUT="PXView-x86_64.AppImage"

./linuxdeploy-x86_64.AppImage --appdir install.dir -e install.dir/usr/bin/PXView -d install.dir/usr/share/applications/pxview.desktop --plugin qt --output appimage
```

**Note on Hardware Access (udev rules):**
To allow the AppImage (or native build) to communicate with USB hardware without needing root permissions, you MUST manually copy the udev rules to your system once:

```bash
sudo cp PXView/px.rules /etc/udev/rules.d/60-px.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

See the following wiki page for more (OS-specific) instructions:
http://sigrok.org/wiki/Building

The latest source code:
https://github.com/PXLogic/PXView
