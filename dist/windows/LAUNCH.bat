@echo off
echo ============================================
echo    Standard of Iron - Game Launcher
echo ============================================
echo.
echo Starting game...
echo.
cd /d "%~dp0"

REM Start the game (runs independently, logs via debug scripts if needed)
start "" "%~dp0standard_of_iron.exe"

REM Wait a moment for process to initialize (3 seconds for slower systems)
timeout /t 3 /nobreak > nul

REM Check if process is running
tasklist /FI "IMAGENAME eq standard_of_iron.exe" 2>NUL | find /I /N "standard_of_iron.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo Game started successfully!
    echo.
    echo If you encounter problems, try these troubleshooting scripts:
    echo   - run_debug.cmd            ^(Standard OpenGL mode with logging^)
    echo   - run_debug_angle.cmd      ^(ANGLE mode - compatible with most GPUs^)
    echo   - run_debug_softwaregl.cmd ^(Software mode - works on all systems^)
    echo.
    echo See README.txt for more information.
) else (
    echo.
    echo ERROR: Game failed to start!
    echo.
    echo TROUBLESHOOTING:
    echo   1. Try running: run_debug.cmd
    echo      ^(Captures detailed logs in runlog.txt^)
    echo   2. If that fails, try: run_debug_softwaregl.cmd
    echo      ^(CPU-based rendering, works on all systems^)
    echo   3. See README.txt for more help
    echo.
)
pause
