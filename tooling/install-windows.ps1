<#
.SYNOPSIS
    Installs (or removes) the locally built SonyHeadphonesClient for the current user.

.DESCRIPTION
    - Copies build\client\Release\SonyHeadphonesClient.exe to %LOCALAPPDATA%\Programs\SonyHeadphonesClient
      and checks the SHA-256 of the copy. The exe only imports DLLs that ship with Windows.
    - Creates a Start Menu shortcut.
    - If "Iniciar com o Windows" is already on (HKCU Run value exists), repoints it to the installed exe.
      It never turns autostart on by itself: that stays the user's choice in the tray menu.
    - Settings (%APPDATA%\rafaelholder\SonyHeadphonesClient\settings.ini) are not touched.
    Per-user only, no admin rights.

.EXAMPLE
    pwsh -File tooling\install-windows.ps1
    pwsh -File tooling\install-windows.ps1 -NoStart
    pwsh -File tooling\install-windows.ps1 -Uninstall
#>
[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build\client\Release'),
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'Programs\SonyHeadphonesClient'),
    [switch]$NoStart,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$exeName = 'SonyHeadphonesClient.exe'
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValue = 'SonyHeadphonesClient'
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'SonyHeadphonesClient.lnk'
$installedExe = Join-Path $InstallDir $exeName

function Stop-Client {
    $running = Get-Process -Name 'SonyHeadphonesClient' -ErrorAction SilentlyContinue
    if ($running) {
        Write-Host "Fechando o cliente em execução (PID $($running.Id -join ', '))..."
        $running | Stop-Process -Force -Confirm:$false
        $running | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }
}

if ($Uninstall) {
    Stop-Client
    if (Get-ItemProperty -Path $runKey -Name $runValue -ErrorAction SilentlyContinue) {
        Remove-ItemProperty -Path $runKey -Name $runValue -Confirm:$false
        Write-Host 'Iniciar com o Windows: removido.'
    }
    if (Test-Path $shortcut) { Remove-Item $shortcut -Confirm:$false; Write-Host "Atalho removido: $shortcut" }
    if (Test-Path $InstallDir) { Remove-Item $InstallDir -Recurse -Confirm:$false; Write-Host "Pasta removida: $InstallDir" }
    Write-Host 'Configurações mantidas em %APPDATA%\rafaelholder\SonyHeadphonesClient (apague à mão se quiser).'
    return
}

$sourceExe = Join-Path (Resolve-Path $BuildDir) $exeName
if (-not (Test-Path $sourceExe)) {
    throw "Executável não encontrado: $sourceExe. Compile antes: cmake --build build --config Release --target SonyHeadphonesClient"
}

Stop-Client
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -Path $sourceExe -Destination $installedExe -Force
$sourceHash = (Get-FileHash $sourceExe -Algorithm SHA256).Hash
$installedHash = (Get-FileHash $installedExe -Algorithm SHA256).Hash
if ($sourceHash -ne $installedHash) { throw "Cópia corrompida: $sourceHash != $installedHash" }
Write-Host "Instalado: $installedExe"
Write-Host "SHA-256:   $installedHash"

$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($shortcut)
$link.TargetPath = $installedExe
$link.WorkingDirectory = $InstallDir
$link.Description = 'Controle do fone Sony (fork rafaelholder)'
$link.Save()
Write-Host "Atalho no menu Iniciar: $shortcut"

$current = (Get-ItemProperty -Path $runKey -Name $runValue -ErrorAction SilentlyContinue).$runValue
if ($current) {
    $command = "`"$installedExe`" --tray"
    if ($current -ne $command) {
        Set-ItemProperty -Path $runKey -Name $runValue -Value $command
        Write-Host "Iniciar com o Windows: apontado para a instalação ($command)"
    } else {
        Write-Host 'Iniciar com o Windows: já aponta para a instalação.'
    }
} else {
    Write-Host 'Iniciar com o Windows: desligado (ligue pelo menu da bandeja, se quiser).'
}

if (-not $NoStart) {
    Start-Process -FilePath $installedExe -ArgumentList '--tray' -WorkingDirectory $InstallDir
    Write-Host 'Cliente iniciado na bandeja.'
}
