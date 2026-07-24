
:: For building on Windows with Docker GCC setup from https://github.com/AmigaPorts/docker-amiga-gcc
:: This doesn't include the NDK. Ensure NDK exists and path for /ndk is set

start cmd /k docker run --rm -v .:/work -v ../../amigadev/ndk32/:/ndk -it amigadev/crosstools:m68k-amigaos bash