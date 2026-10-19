# instalar_plugin.ps1 - Instalador do Plugin InternetControl
# Auto-elevação para Administrador se necessário
$currentPrincipal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Solicitando permissão de Administrador..." -ForegroundColor Yellow
    Start-Process powershell.exe -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

Set-Location $PSScriptRoot

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   Instalador do Plugin InternetControl para Veyon" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "[1/4] Finalizando processos e serviços do Veyon..." -ForegroundColor Yellow
Stop-Service -Name "VeyonService" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "veyon-*" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

$sourceDll = Join-Path $PSScriptRoot "build\internetcontrol.dll"
if (-not (Test-Path $sourceDll)) {
    $sourceDll = Join-Path $PSScriptRoot "internetcontrol.dll"
}

if (-not (Test-Path $sourceDll)) {
    Write-Error "Arquivo internetcontrol.dll não encontrado em $sourceDll!"
    Read-Host "Pressione Enter para sair..."
    exit 1
}

$destDir = "C:\Program Files\Veyon\plugins"
Write-Host "[2/4] Copiando internetcontrol.dll para $destDir..." -ForegroundColor Yellow
Copy-Item -Path $sourceDll -Destination (Join-Path $destDir "internetcontrol.dll") -Force

Write-Host "[3/4] Reiniciando serviço e configurando ocultação do PC do professor..." -ForegroundColor Yellow
Start-Service -Name "VeyonService" -ErrorAction SilentlyContinue
& "C:\Program Files\Veyon\veyon-cli.exe" config set Master/HideLocalComputer true
& "C:\Program Files\Veyon\veyon-cli.exe" config set Master/HideOwnSession true

Write-Host "[4/4] Verificando plugins carregados:" -ForegroundColor Yellow
& "C:\Program Files\Veyon\veyon-cli.exe" plugin list

Write-Host ""
Write-Host "========================================================" -ForegroundColor Green
Write-Host " Instalação concluída com sucesso! Pode abrir o Veyon." -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Green
Write-Host ""
Read-Host "Pressione Enter para fechar..."
