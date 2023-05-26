@echo off
setlocal

rem code statistics
@echo.
@echo -----------
@echo.

set Wildcard=*.h *.cpp *.inl *.c

@echo TODOS FOUND:
findstr -s -n -i -l "TODO" %Wildcard%

@echo.

@echo STATICS FOUND:
findstr -s -n -i -l "static" %Wildcard%

@echo.
@echo -----------
@echo.

rem compilation
where /q cl || (
  echo ERROR: "cl" not found - please run this from the MSVC x64 native tools command prompt.
  exit /b 1
)

if not exist build mkdir build
pushd build

del *.pdb > NUL 2> NUL

set BASE_FILES=../shin_win.cpp 

set CFLAGS=/nologo /W3 /Z7 /GS- /Gs999999 /std:c++20
set LDFLAGS=/incremental:no /opt:icf /opt:ref /subsystem:windows

call cl -Od -Feshin_debug_msvc.exe %CFLAGS% %BASE_FILES% /link %LDFLAGS%
call cl -O2 -Feshin_release_msvc.exe %CFLAGS% %BASE_FILES% /link %LDFLAGS%

where /q clang || (
  echo WARNING: "clang" not found - to run the fastest version of refterm, please install CLANG.
  exit /b 1
)

rem call clang -O3 -o shin.exe %BASE_FILES% -nostdlib -nostdlib++ -mno-stack-arg-probe -maes -fuse-ld=lld -Wl,-subsystem:windows

popd
