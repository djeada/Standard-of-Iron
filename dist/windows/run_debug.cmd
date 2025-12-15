@echo off
setlocal
cd /d "%~dp0"
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES=qt.*=true;qt.qml=true;qqml.*=true;qt.rhi.*=true
set QT_OPENGL=desktop
set QT_QPA_PLATFORM=windows
echo Starting with Desktop OpenGL...
echo Logging to runlog.txt
"%~dp0standard_of_iron.exe" 1> "%~dp0runlog.txt" 2>&1
echo ExitCode: %ERRORLEVEL%>> "%~dp0runlog.txt"
type "%~dp0runlog.txt"
pause
