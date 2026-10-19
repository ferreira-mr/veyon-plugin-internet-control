@echo off
setlocal

:: Verifica permissoes de Administrador
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Solicitando permissao de Administrador...
    powershell -NoProfile -Command "Start-Process '%~dpnx0' -Verb RunAs"
    exit /b
)

echo.
echo ========================================================
echo    LIBERACAO DE EMERGENCIA DA INTERNET (Aluno)
echo ========================================================
echo.

echo [1/5] Removendo regras de firewall do plugin...
netsh advfirewall firewall delete rule name=Veyon_Internet_Block_v4 >nul 2>&1
netsh advfirewall firewall delete rule name=Veyon_Internet_Block_v6 >nul 2>&1
netsh advfirewall firewall delete rule name=Veyon_Internet_Block >nul 2>&1

echo [2/5] Removendo politicas de bloqueio dos navegadores...
for %%K in ("Microsoft\Edge" "Google\Chrome" "BraveSoftware\Brave" "Opera" "Opera Software\Opera" "Opera Software\Opera GX") do (
    reg delete "HKLM\SOFTWARE\Policies\%%~K" /f /reg:64 >nul 2>&1
    reg delete "HKLM\SOFTWARE\Policies\%%~K" /f /reg:32 >nul 2>&1
    reg delete "HKCU\SOFTWARE\Policies\%%~K" /f >nul 2>&1
)
reg delete "HKLM\SOFTWARE\Policies\Mozilla\Firefox\WebsiteFilter" /f /reg:64 >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Mozilla\Firefox\WebsiteFilter" /f /reg:32 >nul 2>&1
reg delete "HKCU\SOFTWARE\Policies\Mozilla\Firefox\WebsiteFilter" /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Mozilla\Firefox" /v DisablePrivateBrowsing /f /reg:64 >nul 2>&1
reg delete "HKCU\SOFTWARE\Policies\Mozilla\Firefox" /v DisablePrivateBrowsing /f >nul 2>&1
reg add "HKLM\SOFTWARE\VeyonInternetControl" /v Blocked /t REG_DWORD /d 0 /f >nul 2>&1

echo [3/5] Limpando arquivo hosts (entradas antigas do plugin)...
powershell -NoProfile -Command "$p='C:\Windows\System32\drivers\etc\hosts'; $c=[IO.File]::ReadAllText($p); $n=[regex]::Replace($c,'(?s)\r?\n?# --- VEYON INTERNET CONTROL START ---.*?# --- VEYON INTERNET CONTROL END ---\r?\n?',''); if($n -ne $c){[IO.File]::WriteAllText($p,$n)}"

echo [4/5] Limpando cache DNS e atualizando politicas...
ipconfig /flushdns >nul 2>&1
gpupdate /target:computer /force >nul 2>&1

echo [5/5] Concluido.
echo.
echo Feche e abra novamente o navegador se alguma pagina continuar bloqueada.
echo Log do plugin: C:\ProgramData\VeyonInternetControl\internetcontrol.log
echo.
pause
