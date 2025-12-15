@echo off
setlocal
cd /d "%~dp0"
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES=qt.*=true;qt.qml=true;qqml.*=true;qt.rhi.*=true
set QT_OPENGL=angle
set QT_ANGLE_PLATFORM=d3d11
set QT_QPA_PLATFORM=windows
echo Starting with ANGLE (OpenGL ES via D3D11)...
echo Logging to runlog_angle.txt
"%~dp0standard_of_iron.exe" 1> "%~dp0runlog_angle.txt" 2>&1
echo ExitCode: %ERRORLEVEL%>> "%~dp0runlog_angle.txt"
type "%~dp0runlog_angle.txt"
pause
