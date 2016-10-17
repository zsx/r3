Build with CMake

Building with cmake supports building out of tree to clean the source tree intact.

Native build (Unix Makefiles):
#mkdir build
#cmake [options] /path-to-ren-c/make
Options support:
R3_WITH_TCC: user natives depend on this, When this is enabled, CMAKE_BUILD_TYPE has to be defined as well
R3_EXTERNAL_FFI: build with libffi provided by the system
R3_OS_ID: the OS_ID with which Rebol 3 is to build
#make -j8 r3-core

Cross-compile for Windows from Linux (mingw64)
Just change the cmake step to include the toolchain file, e.g. for 32-bit windows executable:
#cmake [options] /path-to-ren-c/make -DCMAKE_TOOLCHAIN_FILE=/path-to-ren-c/make/Toolchain-cross-mingw32-linux.cmake

Cross-compile for Android from Linux
1. Preparation: 
1.1 install 'android-ndk' 
1.2 install 'android-cmake' 
1.3 checkout the latest ren-c 

2. compile a TCC cross-compiler: 
2.1 #mkdir build; cd build 
2.2 #cmake -DTCC_BUILD_ARM_EABIHF=1 -DTCC_ARM_VERSION=7 /path-to-ren-c/external/tcc 
2.3 #make ;it should produce an exe arm-linux-gnueabifh-tcc 

3. Compile Rebol 3: 
3.1 #mkdir build; cd build 
3.2 #copy arm-linux-gnueabifh-tcc (from step 2.3) ./cross-tcc 
3.3 #copy your-r3-make ./r3-make
3.4 #export ANDROID_NDK=/opt/android-ndk 
3.5 #export ANDROID_NATIVE_API_LEVEL=16 
3.6 #cmake -DR3_WITH_TCC=1 -DCMAKE_BUILD_TYPE=RELEASE -DR3_OS_ID=0.13.2 -DCMAKE_TOOLCHAIN_FILE=/opt/android-cmake/android.toolchain.cmake /path-to-ren-c/make
3.7 #make r3-core
