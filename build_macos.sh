[ -d build ] || mkdir build
cd build
cmake ..
make -j8

# package the macOS app bundle
cd ..
[ -d "build/SHADERed.app" ] || cp -R Misc/macBundleTemplate/SHADERed.app build/bin
cp -a bin/ build/bin/SHADERed.app/Contents/MacOS
cp -a build/bin/SHADERed build/bin/SHADERed.app/Contents/MacOS