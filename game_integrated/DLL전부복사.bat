@echo off
chcp 65001 >nul
cls
echo.
echo ========================================
echo   필요한 모든 DLL 복사
echo ========================================
echo.

set "SOURCE=C:\msys64\mingw64\bin"

echo 컴파일러 DLL 복사 중...
echo.

REM 컴파일러가 필요한 DLL들
copy /Y "%SOURCE%\libgmp-10.dll" . >nul 2>&1
if exist libgmp-10.dll (echo [OK] libgmp-10.dll) else (echo [FAIL] libgmp-10.dll)

copy /Y "%SOURCE%\libmpfr-6.dll" . >nul 2>&1
if exist libmpfr-6.dll (echo [OK] libmpfr-6.dll) else (echo [FAIL] libmpfr-6.dll)

copy /Y "%SOURCE%\libmpc-3.dll" . >nul 2>&1
if exist libmpc-3.dll (echo [OK] libmpc-3.dll) else (echo [FAIL] libmpc-3.dll)

copy /Y "%SOURCE%\libisl-23.dll" . >nul 2>&1
if exist libisl-23.dll (echo [OK] libisl-23.dll) else (echo [FAIL] libisl-23.dll)

REM 런타임 DLL들
copy /Y "%SOURCE%\libgcc_s_seh-1.dll" . >nul 2>&1
if exist libgcc_s_seh-1.dll (echo [OK] libgcc_s_seh-1.dll) else (echo [FAIL] libgcc_s_seh-1.dll)

copy /Y "%SOURCE%\libstdc++-6.dll" . >nul 2>&1
if exist libstdc++-6.dll (echo [OK] libstdc++-6.dll) else (echo [FAIL] libstdc++-6.dll)

copy /Y "%SOURCE%\libwinpthread-1.dll" . >nul 2>&1
if exist libwinpthread-1.dll (echo [OK] libwinpthread-1.dll) else (echo [FAIL] libwinpthread-1.dll)

REM SDL2 DLL
copy /Y "%SOURCE%\SDL2.dll" . >nul 2>&1
if exist SDL2.dll (echo [OK] SDL2.dll) else (echo [FAIL] SDL2.dll)

echo.
echo ========================================
echo   복사 완료!
echo ========================================
echo.
echo 이제 빌드_PATH수정.bat을 실행하세요.
echo.
pause
