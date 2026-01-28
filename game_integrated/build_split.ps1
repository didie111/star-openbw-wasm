# 컴파일러 경로 설정
$gpp = "C:/msys64/ucrt64/bin/g++.exe"

if (-not (Test-Path $gpp)) {
    Write-Host "ERROR: g++를 찾을 수 없습니다. MSYS2 UCRT64가 설치되어 있는지 확인해주세요." -ForegroundColor Red
    exit 1
}

Write-Host "개별 파일 컴파일 중..." -ForegroundColor Yellow

# 개별 파일 컴파일
& $gpp -c player_input.cpp -o player_input.o -I.. -I. -I../ui -I/ucrt64/include/SDL2 -std=c++17 -DOPENBW_ENABLE_UI -DOPENBW_NO_SDL_IMAGE -DOPENBW_NO_SDL_MIXER -O1 -fno-optimize-sibling-calls
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gpp -c game_logic.cpp -o game_logic.o -I.. -I. -I../ui -I/ucrt64/include/SDL2 -std=c++17 -DOPENBW_ENABLE_UI -DOPENBW_NO_SDL_IMAGE -DOPENBW_NO_SDL_MIXER -O1 -fno-optimize-sibling-calls
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gpp -c sdl2.cpp -o sdl2.o -I.. -I. -I../ui -I/ucrt64/include/SDL2 -std=c++17 -DOPENBW_ENABLE_UI -DOPENBW_NO_SDL_IMAGE -DOPENBW_NO_SDL_MIXER -O1 -fno-optimize-sibling-calls
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# 메인 파일 컴파일
& $gpp -c main.cpp -o main.o -I.. -I. -I../ui -I/ucrt64/include/SDL2 -std=c++17 -DOPENBW_ENABLE_UI -DOPENBW_NO_SDL_IMAGE -DOPENBW_NO_SDL_MIXER -O1 -fno-optimize-sibling-calls
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# 링크
Write-Host "링크 중..." -ForegroundColor Yellow

& $gpp -o OpenBW_Game_NEW.exe main.o player_input.o game_logic.o sdl2.o -L/ucrt64/lib -lmingw32 -lSDL2main -lSDL2 -static-libgcc -static-libstdc++ -Wl,-stack,8388608

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n빌드 성공!" -ForegroundColor Green
    
    # 기존 실행 파일 백업
    if (Test-Path "OpenBW_Game.exe") {
        if (Test-Path "OpenBW_Game_OLD.exe") {
            Remove-Item "OpenBW_Game_OLD.exe" -Force
        }
        Rename-Item "OpenBW_Game.exe" "OpenBW_Game_OLD.exe"
    }
    
    # 새 실행 파일로 교체
    Move-Item "OpenBW_Game_NEW.exe" "OpenBW_Game.exe" -Force
    
    Write-Host "`n새 버전을 실행하려면 다음 명령을 입력하세요:" -ForegroundColor Cyan
    Write-Host ".\OpenBW_Game.exe" -ForegroundColor White -BackgroundColor Black
} else {
    Write-Host "`n빌드 실패!" -ForegroundColor Red
    exit 1
}
