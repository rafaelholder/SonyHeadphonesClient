<#
.SYNOPSIS
    Builds the Windows installer (SonyHeadphonesClient-Setup-<version>-x64.exe) with Inno Setup 6.

.DESCRIPTION
    Compiles installer\SonyHeadphonesClient.iss with ISCC, passing the client version (from client/CMakeLists.txt),
    the git short hash and the path of an already-built SonyHeadphonesClient.exe. Output: build\installer\.

    Inno Setup: winget install --id JRSoftware.InnoSetup --exact

.EXAMPLE
    pwsh -File tooling\build-installer.ps1
    pwsh -File tooling\build-installer.ps1 -Exe build\client\MinSizeRel\SonyHeadphonesClient.exe
#>
[CmdletBinding()]
param(
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\client\Release\SonyHeadphonesClient.exe'),
    [string]$Iscc
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')

if (-not $Iscc) {
    $candidates = @(
        (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source,
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
    ) | Where-Object { $_ -and (Test-Path $_) }
    $Iscc = $candidates | Select-Object -First 1
}
if (-not $Iscc) { throw 'ISCC.exe (Inno Setup 6) não encontrado. Instale: winget install --id JRSoftware.InnoSetup --exact' }

$exePath = Resolve-Path $Exe
$cmake = Get-Content (Join-Path $root 'client\CMakeLists.txt') -Raw
if ($cmake -notmatch 'set\(CLIENT_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\)') { throw 'CLIENT_VERSION não encontrado em client/CMakeLists.txt' }
$version = $Matches[1]
$hash = (git -C $root rev-parse --short HEAD).Trim()

Write-Host "Inno Setup: $Iscc"
Write-Host "Versão:     $version ($hash)"
Write-Host "Executável: $exePath"
& $Iscc /Q "/DAppVersion=$version" "/DGitHash=$hash" "/DSourceExe=$exePath" (Join-Path $root 'installer\SonyHeadphonesClient.iss')
if ($LASTEXITCODE -ne 0) { throw "ISCC falhou (código $LASTEXITCODE)" }

$setup = Join-Path $root "build\installer\SonyHeadphonesClient-Setup-$version-x64.exe"
$sha = (Get-FileHash $setup -Algorithm SHA256).Hash
Write-Host "Instalador: $setup"
Write-Host "SHA-256:    $sha"
