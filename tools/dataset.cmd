@echo off
rem Builds DatasetGen and renders the training set for the sound -> seed net into tools\train\dataset.bin
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0.."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build --target DatasetGen || exit /b 1
if not exist tools\train mkdir tools\train
build\DatasetGen.exe tools\train\dataset.bin %1
