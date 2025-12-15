@echo off
setlocal
cd /d "%~dp0"
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES=qt.*=true;qt.qml=true;qqml.*=true;qt.quick.*=true;qt.rhi.*=true
set QT_OPENGL=software
set QT_QPA_PLATFORM=windows
echo Starting with Software OpenGL...
echo Logging to runlog_software.txt
"%~dp0standard_of_iron.exe" 1> "%~dp0runlog_software.txt" 2>&1
echo ExitCode: %ERRORLEVEL%>> "%~dp0runlog_software.txt"
type "%~dp0runlog_software.txt"
pause
