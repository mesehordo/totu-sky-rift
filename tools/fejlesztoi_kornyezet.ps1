# Fejlesztői környezet az F3 A/B méréshez (.NET 10 SDK, WinLibs GCC, CMake) – PowerShell.
# Használat:  . .\tools\fejlesztoi_kornyezet.ps1     (pontosvesszővel/ponttal betöltve a PATH a munkamenetben marad)
# Telepítve 2026-11-11, winget: Microsoft.DotNet.SDK.10, BrechtSanders.WinLibs.POSIX.UCRT, Kitware.CMake.

$pk = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
$mingw = Join-Path $pk "BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
$cmakeDir = Get-ChildItem (Join-Path $pk "Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe") -Directory -Filter "cmake-*" |
    Sort-Object Name -Descending | Select-Object -First 1
$dotnet = Join-Path $env:ProgramFiles "dotnet"

foreach ($p in @($dotnet, $mingw, (Join-Path $cmakeDir.FullName "bin"))) {
    if (-not (Test-Path $p)) { Write-Warning "Hiányzik: $p" ; continue }
    if (($env:PATH -split ";") -notcontains $p) { $env:PATH = "$p;$env:PATH" }
}
$env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"   # a .NET ne küldjön használati adatot
$env:DOTNET_NOLOGO = "1"

Write-Host ("dotnet " + (& dotnet --version))
Write-Host ((& g++ --version | Select-Object -First 1))
Write-Host ((& cmake --version | Select-Object -First 1))
