# build.ps1 - Compilação do Plugin InternetControl para Veyon
param(
    [switch]$Deploy,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$msysBash = "C:\msys64\usr\bin\bash.exe"
if (-not (Test-Path $msysBash)) {
    Write-Error "MSYS2 bash não encontrado em $msysBash."
    exit 1
}

$buildDir = "C:\Projetos\plugin-veyon\build"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Limpando diretório de build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Write-Host "==> Configurando e compilando plugin InternetControl com MinGW-w64 e Qt6..." -ForegroundColor Cyan

# Executa dentro do ambiente MINGW64 do MSYS2
$bashCommand = "export MSYSTEM=MINGW64 && source /etc/profile && cd /c/Projetos/plugin-veyon/build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && ninja"

& $msysBash -lc $bashCommand

if ($LASTEXITCODE -ne 0) {
    Write-Error "Falha na compilação!"
    exit $LASTEXITCODE
}

$dllPath = "C:\Projetos\plugin-veyon\build\internetcontrol.dll"
if (Test-Path $dllPath) {
    Write-Host "==> Sucesso! DLL gerada em: $dllPath" -ForegroundColor Green
    $size = (Get-Item $dllPath).Length / 1KB
    Write-Host ("Tamanho da DLL: {0:N2} KB" -f $size) -ForegroundColor Green

    if ($Deploy) {
        $targetDir = "C:\Program Files\Veyon\plugins"
        Write-Host "==> Instalando DLL em $targetDir..." -ForegroundColor Cyan
        $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
        if ($isAdmin) {
            Copy-Item -Path $dllPath -Destination $targetDir -Force
            Write-Host "==> Plugin instalado em $targetDir\internetcontrol.dll com sucesso!" -ForegroundColor Green
        } else {
            try {
                Copy-Item -Path $dllPath -Destination $targetDir -Force -ErrorAction Stop
                Write-Host "==> Plugin instalado em $targetDir\internetcontrol.dll com sucesso!" -ForegroundColor Green
            } catch {
                Write-Warning "Permissão insuficiente para gravar em 'C:\Program Files\Veyon\plugins'."
                Write-Host "Para concluir a instalação, abra o PowerShell como Administrador e execute:" -ForegroundColor Yellow
                Write-Host "Copy-Item '$dllPath' '$targetDir\' -Force" -ForegroundColor White
            }
        }
    } else {
        Write-Host "Dica: Em um terminal como Administrador, execute: .\build.ps1 -Deploy" -ForegroundColor Yellow
    }
} else {
    Write-Error "Arquivo internetcontrol.dll não foi encontrado após o build."
    exit 1
}
