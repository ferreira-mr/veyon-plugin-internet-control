@echo off
setlocal enabledelayedexpansion

:: Verifica permissoes de Administrador
net session >nul 2>&1
if errorlevel 1 (
    echo Solicitando permissao de Administrador...
    powershell -NoProfile -Command "Start-Process '%~dpnx0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"

set "DEST=C:\Program Files\Veyon\plugins\internetcontrol.dll"
set "SRC="
if exist "%~dp0build\internetcontrol.dll" set "SRC=%~dp0build\internetcontrol.dll"
if exist "%~dp0internetcontrol.dll" set "SRC=%~dp0internetcontrol.dll"
if not defined SRC (
    echo ERRO: internetcontrol.dll nao encontrada nesta pasta!
    pause
    exit /b 1
)

echo.
echo ========================================================
echo    Instalador do Plugin InternetControl para Veyon
echo ========================================================
echo.

echo [1/4] Finalizando servico e processos do Veyon...
call :StopVeyon

echo [2/4] Instalando internetcontrol.dll...
call :InstallDll
if errorlevel 1 (
    echo.
    echo ERRO: Nao foi possivel substituir a DLL diretamente.
    echo O arquivo pode estar bloqueado pelo Windows Defender ou aberto por outro processo.
    echo Reinicie o computador e tente novamente.
    net start VeyonService >nul 2>&1
    pause
    exit /b 1
)

echo [3/4] Reiniciando servico Veyon...
net start VeyonService >nul 2>&1
"C:\Program Files\Veyon\veyon-cli.exe" config set Master/HideLocalComputer true >nul 2>&1
"C:\Program Files\Veyon\veyon-cli.exe" config set Master/HideOwnSession true >nul 2>&1

echo [4/4] Verificando plugin carregado:
"C:\Program Files\Veyon\veyon-cli.exe" plugin list | findstr /I "InternetControl"

echo.
echo ========================================================
echo Concluido com sucesso! Pode abrir o Veyon Master.
echo ========================================================
echo.
pause
exit /b 0


:StopVeyon
net stop VeyonService /y >nul 2>&1
sc stop VeyonService >nul 2>&1

:: Finaliza todos os processos do Veyon
taskkill /F /T /IM veyon-master.exe >nul 2>&1
taskkill /F /T /IM veyon-server.exe >nul 2>&1
taskkill /F /T /IM veyon-worker.exe >nul 2>&1
taskkill /F /T /IM veyon-service.exe >nul 2>&1
taskkill /F /T /IM veyon-configurator.exe >nul 2>&1
taskkill /F /T /IM veyon-cli.exe >nul 2>&1
taskkill /F /T /IM veyon-auth-helper.exe >nul 2>&1

:: Aguarda 2 segundos para liberar os arquivos na memoria
timeout /t 2 /nobreak >nul
exit /b 0


:InstallDll
:: Tentativa 1: copia direta
for /l %%i in (1,1,3) do (
    copy /Y "%SRC%" "%DEST%" >nul 2>&1 && goto :InstallOk
    timeout /t 1 /nobreak >nul
)

:: Tentativa 2: se a DLL antiga estiver em uso, renomeia dentro da mesma pasta (NTFS permite renomear DLL em uso)
echo    Aviso: DLL em uso pelo sistema - contornando bloqueio do Windows...
set "TEMP_OLD=%DEST%.old_%RANDOM%"
move /Y "%DEST%" "!TEMP_OLD!" >nul 2>&1 || ren "%DEST%" "internetcontrol.old_%RANDOM%" >nul 2>&1

:: Copia a nova DLL
copy /Y "%SRC%" "%DEST%" >nul 2>&1 && goto :InstallOk

:: Tentativa 3: Se ainda falhar, forcar finalizacao adicional e tentar novamente
call :StopVeyon
copy /Y "%SRC%" "%DEST%" >nul 2>&1 && goto :InstallOk
exit /b 1

:InstallOk
:: Tenta limpar arquivos antigos temporarios
del /F /Q "C:\Program Files\Veyon\plugins\*.old_*" >nul 2>&1
echo    DLL instalada com sucesso!
exit /b 0
