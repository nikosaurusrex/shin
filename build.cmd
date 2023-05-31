@echo off
setlocal

rem code statistics
@echo.
@echo -----------
@echo.

set Wildcard=*.h *.cpp

@echo TODOS FOUND:
findstr -n -i -l "TODO" %Wildcard%

@echo.

@echo STATICS FOUND:
findstr -n -i -l "static" %Wildcard%

@echo.
@echo -----------
@echo.

if not exist build mkdir build
pushd build

del *.pdb > NUL 2> NUL

set BASE_FILES=../shin.cpp 

set CFLAGS=-I../extern/include -std=c++20 -nostdlib++ -mno-stack-arg-probe -maes -fuse-ld=lld -D_CRT_SECURE_NO_WARNINGS
set LDFLAGS=-l../extern/libs/freetype/freetype_static -l../extern/libs/glfw/glfw3_mt -l../extern/libs/glew/glew32s -l../extern/libs/OpenGL32 -luser32 -lgdi32 -lshell32

where /q clang || (
  echo ERROR: "clang" not found
  exit /b 1
)

call clang -g -Wall -o shin.exe %BASE_FILES% %CFLAGS% %LDFLAGS%

popd
