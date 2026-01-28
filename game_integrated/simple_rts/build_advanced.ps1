# Build script for simple_game_advanced.cpp
# Run this from MSYS2 UCRT64 terminal or ensure g++ is in PATH

Write-Host "Building simple_game_advanced.cpp..." -ForegroundColor Cyan

# Try to find g++ in common MSYS2 locations
$gppPaths = @(
    "C:\msys64\ucrt64\bin\g++.exe",
    "C:\msys64\mingw64\bin\g++.exe",
    "C:\Program Files (x86)\ucrt64\bin\g++.exe"
)

$gpp = $null
foreach ($path in $gppPaths) {
    if (Test-Path $path) {
        $gpp = $path
        Write-Host "Found g++ at: $gpp" -ForegroundColor Green
        break
    }
}

if (-not $gpp) {
    # Try to use g++ from PATH
    try {
        $gpp = (Get-Command g++).Source
        Write-Host "Using g++ from PATH: $gpp" -ForegroundColor Green
    } catch {
        Write-Host "ERROR: g++ not found!" -ForegroundColor Red
        Write-Host "Please run this from MSYS2 UCRT64 terminal:" -ForegroundColor Yellow
        Write-Host "  cd /c/Users/이기준/Documents/OneDrive/바탕\ 화면/openbw-master/game_integrated/simple_rts" -ForegroundColor Yellow
        Write-Host "  g++ -std=c++11 simple_game_advanced.cpp -lmingw32 -lSDL2main -lSDL2 -o simple_game_advanced.exe" -ForegroundColor Yellow
        exit 1
    }
}

# Clean old build
if (Test-Path "simple_game_advanced.exe") {
    Remove-Item "simple_game_advanced.exe" -Force
    Write-Host "Removed old executable" -ForegroundColor Yellow
}

# Build
$buildCmd = "-std=c++11 simple_game_advanced.cpp -lmingw32 -lSDL2main -lSDL2 -o simple_game_advanced.exe"
Write-Host "Compiling..." -ForegroundColor Cyan

& $gpp $buildCmd.Split(' ')

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "Run with: .\simple_game_advanced.exe" -ForegroundColor Cyan
    
    if (Test-Path "simple_game_advanced.exe") {
        $size = (Get-Item "simple_game_advanced.exe").Length / 1KB
        Write-Host "Executable size: $([math]::Round($size, 2)) KB" -ForegroundColor Gray
    }
} else {
    Write-Host "`nBuild failed!" -ForegroundColor Red
    exit 1
}
