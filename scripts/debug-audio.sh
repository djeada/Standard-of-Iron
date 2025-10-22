#!/bin/bash
# Script to run the game with GLib debugging and Qt multimedia logging enabled

cd "$(dirname "$0")/.." || exit 1

# Build first
echo "Building in debug mode..."
make clean
make debug -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "========================================"
echo "Starting game with audio debugging..."
echo "========================================"
echo ""
echo "GLib errors will be logged but not fatal"
echo "Qt multimedia logging enabled"
echo ""

# Run with GLib debugging (but don't make warnings fatal - they're from GStreamer internals)
# Use GST_DEBUG for more GStreamer info if needed
G_SLICE=always-malloc \
QT_LOGGING_RULES="qt.multimedia.*=true" \
GST_DEBUG=2 \
./build/standard_of_iron
