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