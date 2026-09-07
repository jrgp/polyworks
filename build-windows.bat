@echo off
setlocal

rem Pass -DwxWidgets_ROOT_DIR=C:\path\to\wxWidgets if CMake can't find it automatically.
cmake -S . -B build
if errorlevel 1 goto :err
cmake --build build --config Release
if errorlevel 1 goto :err
echo Build complete: build\bin\polyworks.exe
goto :eof
:err
echo Build failed.
exit /b 1
