@echo off
setlocal
cd /d "%~dp0"

set "CMAKE_COMMAND="
for %%I in (cmake.exe) do set "CMAKE_COMMAND=%%~$PATH:I"

if not defined CMAKE_COMMAND (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do (
            if not defined CMAKE_COMMAND set "CMAKE_COMMAND=%%I"
        )
    )
)

if not defined CMAKE_COMMAND (
    echo CMake was not found.
    echo.
    echo Open the Visual Studio Installer, modify Visual Studio 2022, and install:
    echo   Desktop development with C++
    echo   C++ CMake tools for Windows
    echo.
    pause
    exit /b 1
)

echo Generating the Visual Studio 2022 solution...
"%CMAKE_COMMAND%" --preset windows-vs
if errorlevel 1 (
    echo.
    echo Solution generation failed. Review the error messages above.
    pause
    exit /b 1
)

set "SOLUTION=build\windows-vs\SimplePlatformer.sln"
if not exist "%SOLUTION%" (
    echo.
    echo CMake completed, but %SOLUTION% was not created.
    pause
    exit /b 1
)

echo Opening %SOLUTION%...
start "" "%SOLUTION%"
endlocal
