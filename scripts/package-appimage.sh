#!/bin/bash
set -e

# AppImage packaging script for Linux
# Usage: ./scripts/package-appimage.sh [APP_NAME] [APP_DIR]

APP_NAME="${1:-standard_of_iron}"
APP_DIR="${2:-build/bin}"
QML_SOURCES_PATHS="${3:-ui/qml}"

echo "=== Downloading linuxdeploy tools ==="
wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-*.AppImage

echo "=== Preparing AppDir ==="
mkdir -p AppDir/usr/bin
cp "${APP_DIR}/${APP_NAME}" AppDir/usr/bin/

mkdir -p AppDir/usr/share/applications
cp dist/linux/standard_of_iron.desktop AppDir/usr/share/applications/

mkdir -p AppDir/usr/share/icons/hicolor/1024x1024/apps
cp dist/linux/standard_of_iron.png AppDir/usr/share/icons/hicolor/1024x1024/apps/

echo "=== Copying assets ==="
rsync -a assets/ AppDir/usr/bin/assets/

echo "=== Building AppImage ==="
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS}"
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage

echo "=== AppImage creation complete ==="
ls -lh *.AppImage
