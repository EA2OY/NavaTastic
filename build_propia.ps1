# =====================================================================
# NAVARICO - build_propia.ps1
# Compila un env de Infraestructura PROPIA (R2IP/R1IP) pidiendo las
# claves admin y el PIN BT del operador de forma interactiva.
# NADA se almacena en disco: las claves solo viven en las variables de
# entorno del proceso pio lanzado aqui abajo.
#
# Uso:  .\build_propia.ps1 -EnvName navarrico_promicro_e22p_r2ip
# (sin -EnvName lista los 12 envs Propia disponibles)
# =====================================================================
param(
    [string]$EnvName = ""
)

$pioExe = "C:\Users\Jesus\.platformio\penv\Scripts\pio.exe"

$propiaEnvs = @(
    "navarrico_promicro_e22p_r2ip", "navarrico_faketec_sx1262_r2ip",
    "navarrico_seed_sx1262_r2ip", "navarrico_t114_sx1262_r2ip",
    "navarrico_xiao_kit_sx1262_r2ip", "navarrico_xiao_e22p_r2ip",
    "navarrico_promicro_e22p_r1ip", "navarrico_faketec_sx1262_r1ip",
    "navarrico_seed_sx1262_r1ip", "navarrico_t114_sx1262_r1ip",
    "navarrico_xiao_kit_sx1262_r1ip", "navarrico_xiao_e22p_r1ip"
)

if ($EnvName -eq "") {
    Write-Host "Envs Propia disponibles (usa -EnvName <env>):"
    $propiaEnvs | ForEach-Object { Write-Host "  $_" }
    exit 0
}

if ($propiaEnvs -notcontains $EnvName) {
    Write-Host "ERROR: $EnvName no es un env Propia valido." -ForegroundColor Red
    Write-Host "Usa uno de los 12 envs *ip (sin -EnvName los lista)."
    exit 1
}

Write-Host "Compilando $EnvName (Propia). Las claves se piden SOLO para este proceso," -ForegroundColor Cyan
Write-Host "NO se guardan en ningun fichero." -ForegroundColor Cyan

$k0 = Read-Host "K0 Propia (hex entre llaves: { 0xaa, 0xbb, ... })" | ForEach-Object { $_.Trim() }
$k1 = Read-Host "K1 Propia (hex entre llaves: { 0xcc, 0xdd, ... })" | ForEach-Object { $_.Trim() }
$bt = Read-Host "PIN Bluetooth Propia (6 digitos)" | ForEach-Object { $_.Trim() }

if ($k0 -notmatch "^\s*\{") {
    Write-Host "ERROR: K0 debe ser un array hex entre llaves." -ForegroundColor Red
    exit 1
}
if ($k1 -notmatch "^\s*\{") {
    Write-Host "ERROR: K1 debe ser un array hex entre llaves." -ForegroundColor Red
    exit 1
}
if ($bt -notmatch "^\d{6}$") {
    Write-Host "ERROR: el PIN BT deben ser 6 digitos." -ForegroundColor Red
    exit 1
}

$env:NAVARICO_PROPIA_KEY_0 = $k0
$env:NAVARICO_PROPIA_KEY_1 = $k1
$env:NAVARICO_PROPIA_BT = $bt

Write-Host "Lanzando pio run -e $EnvName ..." -ForegroundColor Cyan
& $pioExe run -e $EnvName
$exitCode = $LASTEXITCODE

Remove-Item Env:\NAVARICO_PROPIA_KEY_0 -ErrorAction SilentlyContinue
Remove-Item Env:\NAVARICO_PROPIA_KEY_1 -ErrorAction SilentlyContinue
Remove-Item Env:\NAVARICO_PROPIA_BT -ErrorAction SilentlyContinue

exit $exitCode
