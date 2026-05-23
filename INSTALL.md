# INSTALL

## Requirements
- git
- gcc (>= 4.0)
- g++
- make
- cmake >= 2.6
- Qt >= 5.0
- libglib >= 2.32.0
- zlib
- libusb-1.0 >= 1.0.16
  On FreeBSD, this is an integral part of the FreeBSD libc, not an extra package/library.
  This is part of the standard OpenBSD install (not an extra package), apparently.
- libboost >= 1.42
- libfftw3 >= 3.3
- libpython > 3.2
- libtool
- pkg-config >= 0.22

## Building and installing

### Step 1: Installing the requirements

Please check your respective distro's package manager tool if you use other distros.

#### Debian/Ubuntu:
```bash
sudo apt install git gcc g++ make cmake libglib2.0-dev zlib1g-dev libusb-1.0-0-dev libboost-dev libfftw3-dev python3-dev libudev-dev pkg-config libgl1-mesa-dev libxkbcommon-dev libvulkan-dev python3-pip
```

**How to install Qt6 (>= 6.6) on Ubuntu 22.04:**
Note: PXView requires Qt 6.6 or higher. The default apt repository on Ubuntu 22.04 only provides Qt 6.2, so you must install Qt6 manually.

**Method 1: Use aqtinstall (Recommended for CLI)**
```bash
pip3 install aqtinstall
aqt install-qt linux desktop 6.11.0 linux_gcc_64
# Set the Qt path before running cmake (the directory is usually named gcc_64 or linux_gcc_64):
export CMAKE_PREFIX_PATH="$(pwd)/6.11.0/gcc_64"
```

**Method 2: Use the Official Qt Online Installer**
Download from qt.io and install Qt 6.11.x (ensure you select Svg and Concurrent components).
```bash
# Set the Qt path before running cmake:
export CMAKE_PREFIX_PATH="/path/to/Qt/6.11.0/gcc_64"
```

#### Fedora (18, 19):
```bash
sudo yum install git gcc g++ make cmake libtool pkgconfig glib2-devel zlib-devel libudev-devel libusb1-devel python3-devel qt-devel boost-devel libfftw3-devel
```

#### Arch:
```bash
pacman -S base-devel git cmake glib2 zlib libusb python boost qt5 fftw
```

#### Mac:
```bash
install git
install hombrew
brew install gcc
brew install g++
brew install make
brew install cmake
brew install gettext
brew install glib
brew install libusb
brew install zlib
brew install boost
brew install fftw
brew install python3
brew install qt
brew install pkg-config
```

### Step 2: Get the DSView source code
```bash
git clone https://github.com/PXLogic/PXView
```

### Step 3: Building
```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="$(pwd)/6.11.0/gcc_64"
ninja
DESTDIR=$(pwd)/../install.dir ninja install
```

### Step 4: Packaging as AppImage (Linux)
If you want to bundle the compiled binary and all its Qt 6.11 dependencies into a single, portable .AppImage file, you can use linuxdeploy on the install directory:

```bash
# Go back to the project root directory (from the 'build' directory)
cd ..
wget -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
wget -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy*.AppImage

# Explicitly tell linuxdeploy where the downloaded Qt 6.11 is, otherwise it bundles the system's old Qt libraries
export QMAKE="$(pwd)/build/6.11.0/gcc_64/bin/qmake"
export LD_LIBRARY_PATH="$(pwd)/build/6.11.0/gcc_64/lib:$LD_LIBRARY_PATH"
export OUTPUT="PXView-x86_64.AppImage"
./linuxdeploy-x86_64.AppImage --appdir install.dir -e install.dir/usr/bin/PXView -d install.dir/usr/share/applications/pxview.desktop --plugin qt --output appimage
./PXView-x86_64.AppImage
```

**Note on Hardware Access (udev rules):**
Because an AppImage is an isolated environment, the Linux host system's udev daemon cannot automatically load the hardware permission rules from inside the AppImage. 
To allow the AppImage to communicate with USB hardware without needing root permissions, the end-user MUST manually copy the udev rules to their system once:

```bash
sudo cp PXView/px.rules /etc/udev/rules.d/60-px.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

See the following wiki page for more (OS-specific) instructions:
http://sigrok.org/wiki/Building

The latest source code:
https://github.com/PXLogic/PXView
