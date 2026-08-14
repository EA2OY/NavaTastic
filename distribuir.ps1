# ============================================================
# NAVARICO distribuir.ps1 - copia los binarios compilados a distribucion/
# ============================================================
# Estructura de salida (mismo esquema que el Desktop del operador):
#   distribucion\<Rama>\<LIPO|NIMH>\<UF2|OTA>\<Nombre NavTastic>.uf2|.zip
#   Rama 2 Routers <- envs r2ig ; Rama 1 Clientes <- envs r1ig
#   NIMH SOLO: Faketec y XiaoKitI2c (sin +E22P) - norma del operador
# Los nombres de fichero son los historicos (p. ej. "Faketec NavTastic 2.7.26 R2IG.uf2").
#
# NORMA 0.12 (14/08): los builds nuevos (V2, tras el snapshot baseline) tambien se
#   copian a Desktop\NavaTastic Eclipse Edition V2 con -V2. El Desktop de Eclipse
#   (NavaTastic 4.3 120826) NO se toca nunca.
# ============================================================
param(
    [string]$EnvName = "",
    [switch]$Todo,
    [switch]$V2
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$v2Root = "C:\Users\Jesus\Desktop\NavaTastic Eclipse Edition V2"

# Mapa env -> (Rama, Nombre de fichero historico)
$map = @{
    "navarrico_promicro_e22p_r2ig"   = @{ Rama = "Rama 2 Routers"; Nombre = "Promicro NRF52+E22P NavTastic 2.7.26 R2IG" }
    "navarrico_faketec_sx1262_r2ig"  = @{ Rama = "Rama 2 Routers"; Nombre = "Faketec NavTastic 2.7.26 R2IG" }
    "navarrico_seed_sx1262_r2ig"     = @{ Rama = "Rama 2 Routers"; Nombre = "Seed Solar Node P1 NavTastic 2.7.26 R2IG" }
    "navarrico_t114_sx1262_r2ig"     = @{ Rama = "Rama 2 Routers"; Nombre = "Heltec T114 NavTastic 2.7.26 R2IG" }
    "navarrico_xiao_kit_sx1262_r2ig" = @{ Rama = "Rama 2 Routers"; Nombre = "XiaoKitI2c NavTastic 2.7.26 R2IG" }
    "navarrico_xiao_e22p_r2ig"       = @{ Rama = "Rama 2 Routers"; Nombre = "XiaoKitI2c+E22P NavTastic 2.7.26 R2IG" }
    "navarrico_promicro_e22p_r1ig"   = @{ Rama = "Rama 1 Clientes"; Nombre = "Promicro NRF52+E22P NavTastic 2.7.26 R1IG" }
    "navarrico_faketec_sx1262_r1ig"  = @{ Rama = "Rama 1 Clientes"; Nombre = "Faketec NavTastic 2.7.26 R1IG" }
    "navarrico_seed_sx1262_r1ig"     = @{ Rama = "Rama 1 Clientes"; Nombre = "Seed Solar Node P1 NavTastic 2.7.26 R1IG" }
    "navarrico_t114_sx1262_r1ig"     = @{ Rama = "Rama 1 Clientes"; Nombre = "Heltec T114 NavTastic 2.7.26 R1IG" }
    "navarrico_xiao_kit_sx1262_r1ig" = @{ Rama = "Rama 1 Clientes"; Nombre = "XiaoKitI2c NavTastic 2.7.26 R1IG" }
    "navarrico_xiao_e22p_r1ig"       = @{ Rama = "Rama 1 Clientes"; Nombre = "XiaoKitI2c+E22P NavTastic 2.7.26 R1IG" }
}
# NIMH solo Faketec + XiaoKitI2c (sin +E22P) - norma del operador
$nimh = @("navarrico_faketec_sx1262_r2ig", "navarrico_xiao_kit_sx1262_r2ig", "navarrico_faketec_sx1262_r1ig", "navarrico_xiao_kit_sx1262_r1ig")

$envs = @()
if ($Todo) { $envs = $map.Keys } elseif ($EnvName) { $envs = @($EnvName) } else { throw "Usa -EnvName <env> o -Todo" }

foreach ($e in $envs) {
    if (-not $map.ContainsKey($e)) { throw "Env no conocido: $e (no esta en el mapa de distribucion)" }
    $info = $map[$e]
    $buildDir = Join-Path $root (".pio\build\$e")
    if (-not (Test-Path -LiteralPath $buildDir)) { throw "No existe build para ${e}: $buildDir (compila antes)" }
    $destRama = Join-Path $root ("distribucion\" + $info.Rama)
    $caras = @("LIPO")
    if ($nimh -contains $e) { $caras += "NIMH" }
    foreach ($cara in $caras) {
        foreach ($tipo in @("UF2", "OTA")) {
            $dir = Join-Path $destRama ($cara + "\" + $tipo)
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
            $patron = if ($tipo -eq "UF2") { "*.uf2" } else { "*.zip" }
            # V2: los .pio/build acumulan artefactos de builds anteriores; SIEMPRE el mas reciente
            $src = Get-ChildItem -LiteralPath $buildDir -Filter $patron -File | Where-Object { $_.Name -notlike "*ota*" -and $_.Name -notlike "*factory*" } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if (-not $src) { Write-Warning "Sin $patron en $buildDir (env $e)"; continue }
            $ext = if ($tipo -eq "UF2") { ".uf2" } else { ".zip" }
            $dest = Join-Path $dir ($info.Nombre + $ext)
            Copy-Item -LiteralPath $src.FullName -Destination $dest -Force
            $h = (Get-FileHash -LiteralPath $dest -Algorithm MD5).Hash
            Write-Host ("OK  {0}" -f $dest.Substring($root.Length + 1))
            Write-Host ("    MD5 {0}" -f $h)
            if ($V2) {
                $v2Dir = Join-Path $v2Root ($info.Rama + "\" + $cara + "\" + $tipo)
                New-Item -ItemType Directory -Path $v2Dir -Force | Out-Null
                $v2Dest = Join-Path $v2Dir ($info.Nombre + $ext)
                Copy-Item -LiteralPath $src.FullName -Destination $v2Dest -Force
                Write-Host ("V2  {0}" -f $v2Dest)
            }
        }
    }
}
Write-Host "Distribucion completada en distribucion\"
