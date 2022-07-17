set HL2SDK_DIR=""

if %HL2SDK_DIR%==""  (
    echo Warning: Please set hl2sdk-csgo path in build.bat before running.
    exit /b
)

cmake -G "Visual Studio 16 2019" -A Win32 -B "vs2019" -DHL2SDK-CSGO:PATH=%HL2SDK_DIR%
cmake --build vs2019 --config Release
