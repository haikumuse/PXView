rm -rf package
mkdir package
cd package
cp ../install.dir/bin/DSView.exe .
cp -r ../install.dir/share/DSView/* .
cp -r ../install.dir/share/libsigrokdecode4DSL/* .
cp -r ../window/python/* .
../window/copy-deps.sh DSView.exe /mingw64
#qt
mkdir plugins
cp -r /mingw64/share/qt5/plugins/* .
../window/copy-deps.sh imageformats/qsvg.dll /mingw64
../window/copy-deps.sh imageformats/qjpeg.dll /mingw64