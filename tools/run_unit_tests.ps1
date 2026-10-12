# Build and run the unit tests on this PC with the Visual Studio C++ compiler.
#
#     powershell -File tools/run_unit_tests.ps1
#
# Compiles test/test_core with lib/jukebox_core and the Unity test framework,
# then runs the result. No board is needed and the firmware is not touched.
# The same tests run on the board with: pio test -e waveshare-s3-rgb-matrix

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root '.pio\build\host-tests'

# Unity is downloaded by PlatformIO the first time the tests are built for the board.
$unity = Get-ChildItem (Join-Path $root '.pio\libdeps') -Directory -ErrorAction SilentlyContinue |
    ForEach-Object { Join-Path $_.FullName 'Unity\src' } |
    Where-Object { Test-Path (Join-Path $_ 'unity.c') } |
    Select-Object -First 1
if (-not $unity) {
    Write-Error 'Unity not found. Fetch it once with: pio pkg install -e waveshare-s3-rgb-matrix -l throwtheswitch/Unity'
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    Write-Error 'Visual Studio was not found (vswhere.exe is missing).'
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    Write-Error 'No Visual Studio installation with the C++ tools was found.'
}
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

New-Item -ItemType Directory -Force $out | Out-Null
$core = Join-Path $root 'lib\jukebox_core'
$sources = @((Join-Path $root 'test\test_core\test_main.cpp'), (Join-Path $unity 'unity.c')) +
    (Get-ChildItem $core -Filter *.cpp | ForEach-Object { $_.FullName })
$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$exe = Join-Path $out 'unit_tests.exe'

# vcvars64.bat sets up the compiler for one cmd session, so compile inside it.
$compile = "`"$vcvars`" >nul 2>nul && cl /nologo /std:c++17 /EHsc /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS " +
    "/I`"$core`" /I`"$unity`" /Fo`"$out\\`" /Fe`"$exe`" $quoted"
cmd /c $compile
if ($LASTEXITCODE -ne 0) {
    Write-Error 'The unit tests did not compile.'
}

& $exe
exit $LASTEXITCODE
