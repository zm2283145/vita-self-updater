#Requires -Version 7.0
param(
    [string]$HostCompiler = 'C:/msys64/mingw64/bin/g++.exe',
    [string]$VitaSdk = $env:VITASDK,
    [string]$BuildDirectory = "$PSScriptRoot/build/tests"
)

$ErrorActionPreference = 'Stop'
$build = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Force -Path $build | Out-Null

if (-not (Test-Path -LiteralPath $HostCompiler -PathType Leaf)) {
    throw "Host C++ compiler not found: $HostCompiler"
}
$env:PATH = "$(Split-Path -Parent $HostCompiler);$env:PATH"
$core = Join-Path $PSScriptRoot 'src/vita_updater_core.cpp'
$include = Join-Path $PSScriptRoot 'include'
$hostExe = Join-Path $build 'updater_core_test.exe'
$hostLibrary = Join-Path $build 'vita_updater_core_test.dll'

& $HostCompiler -std=c++20 -Wall -Wextra -Werror -pedantic `
    -I $include $core (Join-Path $PSScriptRoot 'tests/updater_core_test.cpp') `
    -o $hostExe
if ($LASTEXITCODE -ne 0) { throw "Host test compilation failed: $LASTEXITCODE" }
& $hostExe
if ($LASTEXITCODE -ne 0) { throw "Host tests failed: $LASTEXITCODE" }

& $HostCompiler -std=c++20 -Wall -Wextra -Werror -pedantic -shared -static `
    -I $include $core -o $hostLibrary
if ($LASTEXITCODE -ne 0) { throw "Host library compilation failed: $LASTEXITCODE" }
python (Join-Path $PSScriptRoot 'tests/updater_crypto_test.py') $hostLibrary
if ($LASTEXITCODE -ne 0) { throw "Crypto tests failed: $LASTEXITCODE" }
python (Join-Path $PSScriptRoot 'tests/updater_head_test.py') `
    (Join-Path $PSScriptRoot 'tools/make-head-bin.py') `
    (Join-Path $PSScriptRoot 'tools/head.bin')
if ($LASTEXITCODE -ne 0) { throw "Package-header tests failed: $LASTEXITCODE" }
python (Join-Path $PSScriptRoot 'tests/signing_tools_test.py')
if ($LASTEXITCODE -ne 0) { throw "Signing-tool tests failed: $LASTEXITCODE" }

if (-not $VitaSdk) {
    throw 'VITASDK is not set. Pass -VitaSdk or set VITASDK.'
}
$vitaCompiler = Join-Path $VitaSdk 'bin/arm-vita-eabi-g++.exe'
$env:PATH = "$(Split-Path -Parent $vitaCompiler);$env:PATH"
$vitaFlags = @(
    '-std=c++20', '-O2', '-DNDEBUG', '-fno-exceptions', '-fno-rtti',
    '-fno-short-enums', '-Wall', '-Wextra', '-Werror',
    '-DVITA_UPDATER_USE_SODIUM=1',
    '-DVITA_UPDATER_CURRENT_VERSION="0.6.0"',
    '-DVITA_UPDATER_PUBLIC_KEY_HEX="d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"',
    '-I', $include, '-c'
)
foreach ($source in @(
    'src/vita_updater_core.cpp',
    'src/vita_updater_crypto_sodium.cpp',
    'src/vita_updater_platform.cpp',
    'src/vita_updater_runtime.cpp',
    'src/vita_updater_helper.cpp'
)) {
    $path = Join-Path $PSScriptRoot $source
    $object = Join-Path $build "$([IO.Path]::GetFileNameWithoutExtension($path)).vita.o"
    & $vitaCompiler @vitaFlags $path -o $object
    if ($LASTEXITCODE -ne 0) {
        throw "Vita compile check failed for $source`: $LASTEXITCODE"
    }
}

Write-Output "PASS: host tests and Vita Release compile checks"
