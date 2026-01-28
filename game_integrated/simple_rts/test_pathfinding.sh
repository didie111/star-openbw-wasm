#!/bin/bash
echo "=== 경로 찾기 테스트 빌드 ==="
g++ -std=c++11 test_pathfinding.cpp -o test_pathfinding.exe
if [ $? -eq 0 ]; then
    echo "빌드 성공!"
    echo ""
    ./test_pathfinding.exe
else
    echo "빌드 실패!"
fi
