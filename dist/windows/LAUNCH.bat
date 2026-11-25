@echo off
echo ============================================
echo    Standard of Iron - Game Launcher
echo ============================================
echo.
echo Starting game...
echo.
cd /d "%~dp0"

REM The game supervises its own startup: it tries the graphics driver first
REM and, if that fails at any stage, retries once with the bundled software
REM renderer. /wait returns the launch's real result, so success is reported
REM only for a launch that reached its first gameplay frame (or ran and exited
REM normally), never because some older process with the same name is around.
set "SOI_STARTUP_LOG=%LOCALAPPDATA%\standard_of_iron\startup.log"
start "" /wait "%~dp0standard_of_iron.exe"
set "SOI_EXIT=%ERRORLEVEL%"

if "%SOI_EXIT%"=="0" (
    echo Game closed normally.
    echo.
    echo If you encounter problems, try these troubleshooting scripts:
    echo   - run_debug.cmd            ^(Standard OpenGL mode with logging^)
    echo   - run_debug_softwaregl.cmd ^(Software mode - works on all systems^)
    echo.
    echo See README.txt for more information.
) else (
    echo.
    echo The game stopped with error code %SOI_EXIT%.
    echo.
    echo TROUBLESHOOTING:
    echo   1. Read the startup log, it names the failing stage of each attempt:
    echo      %SOI_STARTUP_LOG%
    echo   2. Try running: run_debug.cmd
    echo      ^(Captures detailed logs in runlog.txt^)
    echo   3. If that fails, try: run_debug_softwaregl.cmd
    echo      ^(CPU-based rendering, works on all systems^)
    echo   4. See README.txt for more help
    echo.
)
pause
