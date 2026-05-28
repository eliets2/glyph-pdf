# toolchain-ucrt64.cmake — MSYS2 ucrt64 native Windows toolchain
# Used to ensure CMake finds the exact .exe binaries regardless of calling shell.
set(CMAKE_C_COMPILER   "C:/msys64/ucrt64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/ucrt64/bin/g++.exe")
set(CMAKE_RC_COMPILER  "C:/msys64/ucrt64/bin/windres.exe")
set(CMAKE_MAKE_PROGRAM "C:/msys64/ucrt64/bin/ninja.exe")
