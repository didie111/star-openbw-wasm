@echo off
chcp 65001 >nul
cls
echo.
echo ========================================
echo   필요한 DLL 파일 확인
echo ========================================
echo.

set "FOUND_ALL=1"

echo [1] SDL2.dll 확인...
if exist SDL2.dll (
    echo     ✓ 있음
) else (
    echo     ✗ 없음
    set "FOUND_ALL=0"
)

echo.
echo [2] libgcc_s_seh-1.dll 확인...
if exist libgcc_s_seh-1.dll (
    echo     ✓ 있음
) else (
    echo     ✗ 없음
    set "FOUND_ALL=0"
)

echo.
echo [3] libstdc++-6.dll 확인...
if exist libstdc++-6.dll (
    echo     ✓ 있음
) else (
    echo     ✗ 없음
    set "FOUND_ALL=0"
)

echo.
echo [4] libwinpthread-1.dll 확인...
if exist libwinpthread-1.dll (
    echo     ✓ 있음
) else (
    echo     ✗ 없음
    set "FOUND_ALL=0"
)

echo.
echo ========================================

if "%FOUND_ALL%"=="1" (
    echo   모든 DLL 파일이 있습니다!
    echo ========================================
    echo.
    echo OpenBW_Game.exe를 실행할 수 있습니다.
) else (
    echo   일부 DLL 파일이 없습니다!
    echo ========================================
    echo.
    echo DLL 파일을 복사하시겠습니까? (Y/N)
    choice /C YN /N
    if errorlevel 2 goto :end
    
    echo.
    echo DLL 복사 중...
    
    if not exist SDL2.dll (
        if exist C:\msys64\mingw64\bin\SDL2.dll (
            copy /Y C:\msys64\mingw64\bin\SDL2.dll . >nul
            echo SDL2.dll 복사 완료
        )
    )
    
    if not exist libgcc_s_seh-1.dll (
        if exist C:\msys64\mingw64\bin\libgcc_s_seh-1.dll (
            copy /Y C:\msys64\mingw64\bin\libgcc_s_seh-1.dll . >nul
            echo libgcc_s_seh-1.dll 복사 완료
        )
    )
    
    if not exist libstdc++-6.dll (
        if exist C:\msys64\mingw64\bin\libstdc++-6.dll (
            copy /Y C:\msys64\mingw64\bin\libstdc++-6.dll . >nul
            echo libstdc++-6.dll 복사 완료
        )
    )
    
    if not exist libwinpthread-1.dll (
        if exist C:\msys64\mingw64\bin\libwinpthread-1.dll (
            copy /Y C:\msys64\mingw64\bin\libwinpthread-1.dll . >nul
            echo libwinpthread-1.dll 복사 완료
        )
    )
    
    echo.
    echo 복사 완료!
)

:end
echo.
pause
