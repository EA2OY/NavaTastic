# ============================================================
# NAVARICO build.ps1 - compila variantes Navarrico desde el repo unificado
# ============================================================
# Uso:
#   .\build.ps1                                          -> compila los 12 envs por defecto
#   .\build.ps1 -EnvName navarrico_promicro_e22p_r2ig    -> compila uno solo
#   .\build.ps1 -EnvName navarrico_xiao_e22p_r1ig -Distribuir
#   .\build.ps1 -Paridad                                 -> fija BUILD_EPOCH al 12/08/2026 (verificacion)
#
# Los envs estan definidos en variants/nrf52840/navarrico.ini.
# NAVARICO_BUILD_EPOCH solo se usa para reproducir builds previos byte a byte;
# en compilacion normal (sin -Paridad) el comportamiento es el original (medianoche de hoy).
# ============================================================
param(
    [string]$EnvName = "",
    [switch]$Distribuir,
    [switch]$Paridad,
    [string]$BuildTime = "",
    [string]$BuildDate = ""
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

if ($Paridad) {
    # Epoch Unix del 12/08/2026 00:00 (UTC+2, Madrid) = dia del build "Eclipse Edition" de Rama 2
    $env:NAVARICO_BUILD_EPOCH = [DateTimeOffset]::new(2026, 8, 12, 0, 0, 0, [TimeSpan]::FromHours(2)).ToUnixTimeSeconds().ToString()
    # Version embebida de los builds originales (SHA de git 54e0d8d de los 24 repos viejos)
    $env:NAVARICO_APP_VERSION = "2.7.26.54e0d8d"
    Write-Host "NAVARICO_BUILD_EPOCH = $env:NAVARICO_BUILD_EPOCH (12/08) - MODO PARIDAD"
    Write-Host "NAVARICO_APP_VERSION = $env:NAVARICO_APP_VERSION (54e0d8d)"
} else {
    Remove-Item Env:NAVARICO_BUILD_EPOCH -ErrorAction SilentlyContinue
    Remove-Item Env:NAVARICO_APP_VERSION -ErrorAction SilentlyContinue
}
# Marca temporal embebida (Crypto/RNG + RadioLib usan __TIME__/__DATE__): para paridad se pasa
# la marca exacta de la referencia (la extrae verificar_paridad.ps1). Sin valores: marca de hoy.
if ($BuildTime) { $env:NAVARICO_BUILD_TIME = $BuildTime }
if ($BuildDate) { $env:NAVARICO_BUILD_DATE = $BuildDate }

# Localizar pio (PATH primero; si no, ruta tipica de instalacion)
$pio = Get-Command pio -ErrorAction SilentlyContinue
if ($pio) {
    $pio = $pio.Source
} else {
    $pioPath = "C:\Users\Jesus\.platformio\penv\Scripts\pio.exe"
    if (Test-Path -LiteralPath $pioPath) { $pio = $pioPath } else { throw "pio no encontrado en PATH ni en $pioPath" }
}

Push-Location $root
try {
    if ($EnvName) {
        & $pio run -e $EnvName
        if ($LASTEXITCODE -ne 0) { throw "Build fallo: $EnvName" }
        if ($Distribuir) { & (Join-Path $root "distribuir.ps1") -EnvName $EnvName }
    } else {
        & $pio run
        if ($LASTEXITCODE -ne 0) { throw "Build fallo" }
        if ($Distribuir) { & (Join-Path $root "distribuir.ps1") -Todo }
    }
} finally {
    Pop-Location
    Remove-Item Env:NAVARICO_BUILD_EPOCH -ErrorAction SilentlyContinue
    Remove-Item Env:NAVARICO_APP_VERSION -ErrorAction SilentlyContinue
    Remove-Item Env:NAVARICO_BUILD_TIME -ErrorAction SilentlyContinue
    Remove-Item Env:NAVARICO_BUILD_DATE -ErrorAction SilentlyContinue
}
