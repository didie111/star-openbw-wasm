#!/bin/bash
# Build script for simple_game_advanced.cpp
# Run from MSYS2 UCRT64 terminal

echo "Building simple_game_advanced.cpp..."

# Clean old build
rm -f simple_game_advanced.exe

# Build
g++ -std=c++11 simple_game_advanced.cpp -lmingw32 -lSDL2main -lSDL2 -o simple_game_advanced.exe

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    ls -lh simple_game_advanced.exe
    echo ""
    echo "Run with: ./simple_game_advanced.exe"
else
    echo ""
    echo "✗ Build failed!"
    exit 1
fi
