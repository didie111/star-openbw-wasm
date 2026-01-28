param(
  [string]$Root = "..",
  [string]$OutDir = ".",
  [string]$LogFile = "build_web_out_utf8.txt"
)

$ErrorActionPreference = "Stop"

Write-Host "[WEB] Building OpenBW to WASM" -ForegroundColor Cyan

# Requires: Emscripten SDK activated in this terminal.
# Example (once installed):
#   emsdk_env.ps1

$srcs = @(
  (Join-Path $Root "game_integrated\player_game_complete.cpp"),
  (Join-Path $Root "game_integrated\sdl2.cpp")
)

$includes = @(
  "-I$Root",
  "-I$Root\ui",
  "-I$Root\game_integrated"
)

$mapsHostPath = (Resolve-Path (Join-Path $Root "maps")).Path
$mapsHostPath = $mapsHostPath -replace "\\", "/"

Write-Host ("[WEB] mapsHostPath = " + $mapsHostPath)

$outDirAbs0 = (Resolve-Path $OutDir).Path
if ([System.IO.Path]::IsPathRooted($LogFile)) {
  $logFileAbs = $LogFile
} else {
  $logFileAbs = (Join-Path $outDirAbs0 $LogFile)
}

if (-not ("Win32.ShortPath" -as [type])) {
  Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

namespace Win32 {
  public static class ShortPath {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern int GetShortPathName(string lpszLongPath, StringBuilder lpszShortPath, int cchBuffer);

    public static string Get(string longPath) {
      if (string.IsNullOrEmpty(longPath)) return longPath;
      var sb = new StringBuilder(4096);
      int n = GetShortPathName(longPath, sb, sb.Capacity);
      if (n <= 0) return longPath;
      return sb.ToString();
    }
  }
}
"@ -Language CSharp
}

function Get-ShortPath([string]$p) {
  $pp = (Resolve-Path $p).Path
  return [Win32.ShortPath]::Get($pp)
}

function To-PosixPath([string]$p) {
  return ($p -replace "\\", "/")
}

function Replace-File([string]$src, [string]$dst) {
  $tmp = $dst + ".tmp"
  for ($i = 0; $i -lt 10; $i++) {
    try {
      Copy-Item -Force -Path $src -Destination $tmp
      Move-Item -Force -Path $tmp -Destination $dst
      return
    } catch {
      Start-Sleep -Milliseconds 200
    }
  }
  throw "Failed to update output file (locked?): $dst"
}

# NOTE:
# - We rely on SDL2 port from Emscripten for window/input.
# - Output will be: openbw.js + openbw.wasm
# - We export openbw_web_start so app.js can call it.

$flags = @(
  "-std=c++17",
  "-O3",
  "-fexceptions",
  "-sUSE_SDL=2",
  "-sUSE_SDL_IMAGE=2",
  '-sSDL2_IMAGE_FORMATS=["png"]',
  "-sUSE_SDL_MIXER=2",
  "-sALLOW_MEMORY_GROWTH=1",
  "-sASSERTIONS=1",
  "-sNO_DISABLE_EXCEPTION_CATCHING",
  "-sEXPORTED_FUNCTIONS=[_openbw_web_start]",
  "-sEXPORTED_RUNTIME_METHODS=[cwrap,FS]",
  "-sMODULARIZE=0",
  "-sEXPORT_NAME=Module"
)

$emArgs = $srcs + $includes + $flags + @(
  "--preload-file",
  (Join-Path $Root "game_integrated\images") + "@/images",
  "--preload-file",
  (Join-Path $Root "game_integrated\images") + "@/data/images",
  "--preload-file",
  (Join-Path $Root "maps") + "@/maps",
  "--preload-file",
  (Join-Path $Root "maps") + "@/data/maps",
  "-o",
  (Join-Path $OutDir "openbw.js")
)

Write-Host ("em++ " + ($emArgs -join " "))

# Invoke via cmd using 8.3 short paths to avoid Unicode/quoting issues and ensure the log is plain text.
$rootAbs = (Resolve-Path $Root).Path
$rootShort = Get-ShortPath $rootAbs
$srcShort = Get-ShortPath (Join-Path $rootAbs "game_integrated\player_game_complete.cpp")
$sdl2SrcShort = Get-ShortPath (Join-Path $rootAbs "game_integrated\sdl2.cpp")
$uiShort = Get-ShortPath (Join-Path $rootAbs "ui")
$giShort = Get-ShortPath (Join-Path $rootAbs "game_integrated")
$mapsShort = Get-ShortPath (Join-Path $rootAbs "maps")
$imagesShort = Get-ShortPath (Join-Path $rootAbs "game_integrated\images")
$outDirAbs = (Resolve-Path $OutDir).Path
$outDirShort = Get-ShortPath $outDirAbs
$buildDir = Join-Path $env:TEMP "openbw_web_build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$buildDirShort = Get-ShortPath $buildDir
$outJsShort = (Join-Path $buildDirShort "openbw.js")

$emsdkBat = "C:\Users\82103\Downloads\test2.396t\emsdk-main\emsdk_env.bat"
$emsdkBatShort = Get-ShortPath $emsdkBat

$cmdLine = @(
  "set EMSDK_QUIET=1",
  ('call "{0}" >nul' -f $emsdkBatShort),
  ('em++ "{0}" "{1}" -I"{2}" -I"{3}" -I"{4}" -std=c++17 -O3 -fexceptions -sNO_DISABLE_EXCEPTION_CATCHING -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS=[\"png\"] -sUSE_SDL_MIXER=2 -sALLOW_MEMORY_GROWTH=1 -sASSERTIONS=1 -sEXPORTED_FUNCTIONS=[_openbw_web_start] -sEXPORTED_RUNTIME_METHODS=[cwrap,FS] -sMODULARIZE=0 -sEXPORT_NAME=Module --preload-file "{5}@/images" --preload-file "{5}@/data/images" --preload-file "{6}@/maps" --preload-file "{6}@/data/maps" -o "{7}"' -f $srcShort, $sdl2SrcShort, $rootShort, $uiShort, $giShort, $imagesShort, $mapsShort, $outJsShort)
) -join " && "

$prevEap2 = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
  Push-Location $buildDir
  $cmdAll = "$cmdLine > `"$logFileAbs`" 2>&1"
  $success = $false
  for ($try = 0; $try -lt 6; $try++) {
    cmd /c $cmdAll | Out-Null
    if ($LASTEXITCODE -eq 0) {
      $success = $true
      break
    }
    Start-Sleep -Milliseconds (200 * ($try + 1))
  }
  if (-not $success) {
    throw "em++ invocation failed after retries"
  }
} finally {
  try { Pop-Location } catch { }
  $ErrorActionPreference = $prevEap2
}

if ($LASTEXITCODE -ne 0) {
  throw "em++ failed with exit code $LASTEXITCODE. See $LogFile"
}

Replace-File (Join-Path $buildDir "openbw.js") (Join-Path $OutDir "openbw.js")
Replace-File (Join-Path $buildDir "openbw.wasm") (Join-Path $OutDir "openbw.wasm")

if (Test-Path (Join-Path $buildDir "openbw.data")) {
  Replace-File (Join-Path $buildDir "openbw.data") (Join-Path $OutDir "openbw.data")
}

Write-Host "[WEB] Done: $OutDir\openbw.js" -ForegroundColor Green
