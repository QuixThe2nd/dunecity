@echo off
if "%JAVA_HOME%"=="" (
    echo Set JAVA_HOME to your JDK before running this script.
    exit /b 1
)
if "%ANDROID_HOME%"=="" (
    if not "%ANDROID_SDK_ROOT%"=="" set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
)
if "%ANDROID_HOME%"=="" (
    echo Set ANDROID_HOME or ANDROID_SDK_ROOT to your Android SDK before running this script.
    exit /b 1
)
set "ANDROID_SDK_ROOT=%ANDROID_HOME%"
if "%ANDROID_NDK_HOME%"=="" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "%ANDROID_HOME%\ndk" 2^>nul') do (
        for /f "tokens=1 delims=." %%V in ("%%D") do (
            if %%V GEQ 26 if exist "%ANDROID_HOME%\ndk\%%D\build\cmake\android.toolchain.cmake" (
                set "ANDROID_NDK_HOME=%ANDROID_HOME%\ndk\%%D"
                goto :found_ndk
            )
        )
    )
)
if "%ANDROID_NDK_HOME%"=="" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "%LOCALAPPDATA%\Android\Sdk\ndk" 2^>nul') do (
        for /f "tokens=1 delims=." %%V in ("%%D") do (
            if %%V GEQ 26 if exist "%LOCALAPPDATA%\Android\Sdk\ndk\%%D\build\cmake\android.toolchain.cmake" (
                set "ANDROID_NDK_HOME=%LOCALAPPDATA%\Android\Sdk\ndk\%%D"
                goto :found_ndk
            )
        )
    )
)
:found_ndk
if "%ANDROID_NDK_HOME%"=="" (
    echo Set ANDROID_NDK_HOME or install a complete Android NDK 26 or newer under an Android SDK ndk directory.
    exit /b 1
)
if not exist "%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" (
    echo ANDROID_NDK_HOME points to an incomplete NDK: %ANDROID_NDK_HOME%
    exit /b 1
)
for %%D in ("%ANDROID_NDK_HOME%") do set "ANDROID_NDK_FOLDER=%%~nxD"
for /f "tokens=1 delims=." %%V in ("%ANDROID_NDK_FOLDER%") do set "ANDROID_NDK_MAJOR=%%V"
if %ANDROID_NDK_MAJOR% LSS 26 (
    echo Android NDK 26 or newer is required: %ANDROID_NDK_HOME%
    exit /b 1
)
set "PATH=%JAVA_HOME%\bin;%ANDROID_HOME%\platform-tools;%ANDROID_HOME%\cmdline-tools\latest\bin;%PATH%"

echo JAVA_HOME=%JAVA_HOME%
echo ANDROID_HOME=%ANDROID_HOME%
echo ANDROID_NDK_HOME=%ANDROID_NDK_HOME%
