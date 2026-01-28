# 간단한 RTS 빌드 스크립트

Write-Host "=== Simple RTS 빌드 ===" -ForegroundColor Green

# g++ 확인
if (!(Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Host "오류: g++가 설치되지 않았습니다!" -ForegroundColor Red
    Write-Host "MinGW 또는 MSYS2를 설치하세요." -ForegroundColor Yellow
    exit 1
}

# 빌드
Write-Host "컴파일 중..." -ForegroundColor Cyan
g++ -std=c++17 -O2 -o simple_rts.exe simple_rts.cpp

if ($LASTEXITCODE -eq 0) {
    Write-Host "빌드 성공!" -ForegroundColor Green
    Write-Host ""
    Write-Host "실행: .\simple_rts.exe" -ForegroundColor Yellow
} else {
    Write-Host "빌드 실패!" -ForegroundColor Red
    exit 1
}
