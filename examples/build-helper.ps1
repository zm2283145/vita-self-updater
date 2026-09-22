#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$AppName,
    [Parameter(Mandatory)][ValidatePattern('^[A-Z0-9]{9}$')]
    [string]$MainTitleId,
    [Parameter(Mandatory)][ValidatePattern('^[A-Z0-9]{9}$')]
    [string]$HelperTitleId,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_.-]+$')]
    [string]$GitHubOwner,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_.-]+$')]
    [string]$GitHubRepo,
    [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')]
    [string]$Version,
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string]$PublicKeyHex,
    [Parameter(Mandatory)][string]$StagingDirectory,
    [string]$AppVersion = '01.00',
    [string]$VitaSdk = $env:VITASDK,
    [string]$BuildDirectory = "$PSScriptRoot/../build/helper"
)

$ErrorActionPreference = 'Stop'
if (-not $VitaSdk) { throw 'VITASDK is required.' }
if ($AppVersion -notmatch '^[0-9]{2}\.[0-9]{2}$') {
    throw 'AppVersion must use the Vita NN.NN format.'
}
if ($StagingDirectory -notmatch '^ux0:/data/[A-Za-z0-9._/-]+$') {
    throw 'StagingDirectory must be a dedicated path under ux0:/data/.'
}

$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = [IO.Path]::GetFullPath($BuildDirectory)
$objects = Join-Path $build 'obj'
$package = Join-Path $build 'updater'
New-Item -ItemType Directory -Force $objects, (Join-Path $package 'sce_sys/package') |
    Out-Null
$env:PATH = "$(Join-Path $VitaSdk 'bin');$env:PATH"

$defines = @(
    '-DVITA_UPDATER_USE_SODIUM=1',
    "-DVITA_UPDATER_APP_NAME=`"$AppName`"",
    "-DVITA_UPDATER_MAIN_TITLE_ID=`"$MainTitleId`"",
    "-DVITA_UPDATER_HELPER_TITLE_ID=`"$HelperTitleId`"",
    "-DVITA_UPDATER_GITHUB_OWNER=`"$GitHubOwner`"",
    "-DVITA_UPDATER_GITHUB_REPO=`"$GitHubRepo`"",
    "-DVITA_UPDATER_CURRENT_VERSION=`"$Version`"",
    "-DVITA_UPDATER_PUBLIC_KEY_HEX=`"$($PublicKeyHex.ToLowerInvariant())`"",
    "-DVITA_UPDATER_STAGING_DIRECTORY=`"$StagingDirectory`""
)
$flags = @(
    '-std=c++20', '-O3', '-DNDEBUG', '-fno-exceptions', '-fno-rtti',
    '-fshort-enums', '-ffunction-sections', '-fdata-sections',
    '-Wall', '-Wextra', '-Werror', '-I', (Join-Path $root 'include'), '-c'
) + $defines

$sources = @(
    'vita_updater_helper.cpp',
    'vita_updater_core.cpp',
    'vita_updater_crypto_sodium.cpp',
    'vita_updater_platform.cpp'
)
$objectFiles = foreach ($name in $sources) {
    $object = Join-Path $objects ($name -replace '\.cpp$', '.o')
    & arm-vita-eabi-g++ @flags (Join-Path $root "src/$name") -o $object
    if ($LASTEXITCODE -ne 0) { throw "Compiling $name failed." }
    $object
}

$elf = Join-Path $build 'updater.elf'
$velf = Join-Path $build 'updater.velf'
$link = @('-fshort-enums', '-Wl,-q', '-Wl,--gc-sections') + $objectFiles + @(
    '-Wl,--start-group',
    '-larchive', '-lsodium', '-lbz2', '-lzstd', '-lz', '-lvita2d', '-lstdc++',
    '-Wl,--end-group',
    '-lSceCtrl_stub', '-lSceDisplay_stub', '-lSceGxm_stub',
    '-lSceCommonDialog_stub', '-lScePgf_stub', '-lSceAppMgr_stub',
    '-lScePromoterUtil_stub', '-lSceSysmodule_stub', '-lSceProcessmgr_stub',
    '-lSceSysmem_stub', '-lSceLibKernel_stub', '-lSceKernelThreadMgr_stub',
    '-lm', '-o', $elf
)
& arm-vita-eabi-gcc @link
if ($LASTEXITCODE -ne 0) { throw 'Linking the helper failed.' }

& arm-vita-eabi-strip --strip-debug $elf
& vita-elf-create $elf $velf
& vita-make-fself -c $velf (Join-Path $package 'eboot.bin')
& vita-mksfoex -s "TITLE_ID=$HelperTitleId" -s "APP_VER=$AppVersion" `
    "$AppName Updater" (Join-Path $package 'sce_sys/param.sfo')
python (Join-Path $root 'tools/make-head-bin.py') `
    --template (Join-Path $root 'tools/head.bin') `
    --title-id $HelperTitleId `
    --output (Join-Path $package 'sce_sys/package/head.bin')
python (Join-Path $root 'tools/make-head-bin.py') `
    --template (Join-Path $root 'tools/head.bin') `
    --title-id $MainTitleId `
    --output (Join-Path $build 'main-head.bin')
if ($LASTEXITCODE -ne 0) { throw 'Generating package headers failed.' }

Write-Output $package
