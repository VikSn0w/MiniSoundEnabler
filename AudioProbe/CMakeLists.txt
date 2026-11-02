# AudioProbe — Mac Mini G4 audio test app (Mac OS 9, PowerPC)
#
#   cmake -S . -B build \
#     -DCMAKE_TOOLCHAIN_FILE=$HOME/Retro68-toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
#   cmake --build build
#
# Copy build/AudioProbe.bin (MacBinary) or AudioProbe.dsk to the Mac.

cmake_minimum_required(VERSION 3.10)
project(AudioProbe C)

add_application(AudioProbe AudioProbe.c)

# NameRegistry.h / DriverServices.h stand-ins shared with the extension
target_include_directories(AudioProbe PRIVATE ../Retro68/include ../src)
target_compile_options(AudioProbe PRIVATE -Wall --param=min-pagesize=0)
target_link_libraries(AudioProbe NameRegistryLib DriverServicesLib SoundLib)
