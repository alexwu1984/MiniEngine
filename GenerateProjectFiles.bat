cmake -G "Visual Studio 16 2019" -A x64 -B build  -DCMAKE_BUILD_TYPE=Debug

IF %ERRORLEVEL% NEQ 0 (
    PAUSE
) ELSE (
    START build/MiniEngineTutorial.sln
)