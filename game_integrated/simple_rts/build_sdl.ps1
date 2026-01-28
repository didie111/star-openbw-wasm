# Simple RTS SDL2 버전 빌드 스크립트

Write-Host "=== Simple RTS C++ SDL2 버전 빌드 ===" -ForegroundColor Green
Write-Host ""

# SDL2 파일 복사
Write-Host "SDL2 파일 복사 중..." -ForegroundColor Cyan
Copy-Item -Path "..\SDL2.dll" -Destination "." -Force -ErrorAction SilentlyContinue

# MinGW 경로 찾기
$mingwPaths = @(
    "C:\msys64\mingw64\bin",
    "C:\mingw64\bin",
    "C:\MinGW\bin",
    "C:\Program Files\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\bin"
)

$gppPath = $null
foreach ($path in $mingwPaths) {
    if (Test-Path "$path\g++.exe") {
        $gppPath = "$path\g++.exe"
        $env:PATH = "$path;$env:PATH"
        Write-Host "MinGW 발견: $path" -ForegroundColor Green
        break
    }
}

if (-not $gppPath) {
    Write-Host "오류: g++를 찾을 수 없습니다!" -ForegroundColor Red
    Write-Host ""
    Write-Host "해결 방법:" -ForegroundColor Yellow
    Write-Host "1. MSYS2 설치: https://www.msys2.org/" -ForegroundColor Yellow
    Write-Host "2. MSYS2 터미널에서 실행:" -ForegroundColor Yellow
    Write-Host "   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "또는 HTML 버전을 사용하세요: index.html" -ForegroundColor Yellow
    pause
    exit 1
}

# SDL2 include 경로 찾기
$sdl2Include = $null
$sdl2Lib = $null

$sdl2Paths = @(
    "C:\msys64\mingw64",
    "C:\mingw64",
    "..\SDL2-2.0.22"
)

foreach ($path in $sdl2Paths) {
    if (Test-Path "$path\include\SDL2\SDL.h") {
        $sdl2Include = "$path\include\SDL2"
        $sdl2Lib = "$path\lib"
        Write-Host "SDL2 발견: $path" -ForegroundColor Green
        break
    }
}

if (-not $sdl2Include) {
    Write-Host "오류: SDL2를 찾을 수 없습니다!" -ForegroundColor Red
    Write-Host ""
    Write-Host "해결 방법:" -ForegroundColor Yellow
    Write-Host "MSYS2 터미널에서 실행:" -ForegroundColor Yellow
    Write-Host "   pacman -S mingw-w64-x86_64-SDL2" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "또는 HTML 버전을 사용하세요: index.html" -ForegroundColor Yellow
    pause
    exit 1
}

# 빌드
Write-Host ""
Write-Host "컴파일 중..." -ForegroundColor Cyan
Write-Host "명령: g++ -std=c++11 -I`"$sdl2Include`" -L`"$sdl2Lib`" simple_game_sdl.cpp -lmingw32 -lSDL2main -lSDL2 -o simple_game.exe" -ForegroundColor Gray

& $gppPath -std=c++11 -I"$sdl2Include" -L"$sdl2Lib" simple_game_sdl.cpp -lmingw32 -lSDL2main -lSDL2 -o simple_game.exe 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "빌드 성공!" -ForegroundColor Green
    Write-Host ""
    Write-Host "실행: .\simple_game.exe" -ForegroundColor Yellow
    Write-Host ""
    
    # 자동 실행 여부 묻기
    $response = Read-Host "지금 실행하시겠습니까? (Y/N)"
    if ($response -eq 'Y' -or $response -eq 'y') {
        Write-Host ""
        Write-Host "게임 시작..." -ForegroundColor Green
        .\simple_game.exe
    }
} else {
    Write-Host ""
    Write-Host "빌드 실패!" -ForegroundColor Red
    Write-Host ""
    Write-Host "HTML 버전을 대신 사용하세요: index.html" -ForegroundColor Yellow
    pause
}
