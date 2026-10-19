# Script de instalação com elevação automática de Administrador
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Solicitando permissão de Administrador (UAC)..." -ForegroundColor Yellow
    Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}

Clear-Host
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "    Instalador do Plugin InternetControl para Veyon" -ForegroundColor Cyan
Write-Host "========================================================`n" -ForegroundColor Cyan

Write-Host "[1/4] Finalizando processos do Veyon para destravar arquivos..." -ForegroundColor Yellow
Stop-Process -Name veyon* -Force -ErrorAction SilentlyContinue
net stop VeyonService 2>$null

Write-Host "[2/4] Copiando internetcontrol.dll para C:\Program Files\Veyon\plugins\..." -ForegroundColor Yellow
Copy-Item "C:\Projetos\plugin-veyon\build\internetcontrol.dll" "C:\Program Files\Veyon\plugins\" -Force

Write-Host "[3/4] Reiniciando serviço do Veyon e configurando para ocultar computador do professor..." -ForegroundColor Yellow
net start VeyonService 2>$null
& "C:\Program Files\Veyon\veyon-cli.exe" config set Master/HideLocalComputer true
& "C:\Program Files\Veyon\veyon-cli.exe" config set Master/HideOwnSession true

Write-Host "[4/4] Verificando se o Veyon reconheceu o plugin:`n" -ForegroundColor Yellow
& "C:\Program Files\Veyon\veyon-cli.exe" plugin list

Write-Host "`n========================================================" -ForegroundColor Green
Write-Host "  CONCLUÍDO COM SUCESSO! Agora abra o Veyon Master." -ForegroundColor Green
Write-Host "========================================================`n" -ForegroundColor Green

Read-Host "Pressione ENTER para fechar esta janela"
