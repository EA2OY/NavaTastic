# ============================================================
# NAVARICO verificar_paridad.ps1 - prueba que el repo unificado
# produce binarios byte-idénticos a los 12 builds originales (General).
# ============================================================
# Uso:
#   .\verificar_paridad.ps1 `
#       -RefR2IG "C:\Firmware Navarrico 4.3\Rama 2 Infraestructura\Infraestructura General" `
#       -RefR1IG "C:\Firmware Navarrico 4.3\Rama 1 Clientes en Infraestructura\Infraestructura General"
#
# Como funciona:
#   1) Compila los 12 envs con NAVARICO_BUILD_EPOCH = 12/08/2026 (día de los builds originales)
#      y APP_ENV canónico (custom_meshtastic_app_env) -> el binario embebe los mismos metadatos.
#   2) Compara MD5 del .uf2 (estricto) y del .zip OTA (el zip puede diferir SOLO en el nombre
#      interno del env; el firmware que contiene debe ser idéntico).
# Resultado esperado: 12/12 OK en .uf2 -> unificación probada sin cambiar un byte de firmware.
# ============================================================
param(
    [string]$RefR2IG = "",
    [string]$RefR1IG = ""
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
if (-not $RefR2IG -or -not $RefR1IG) { throw "Indica -RefR2IG y -RefR1IG (carpetas Infraestructura General de Rama 2 y Rama 1)" }

$map = @{
    "navarrico_promicro_e22p_r2ig"   = @{ Rama = "Rama 2 Routers"; Nombre = "Promicro NRF52+E22P NavTastic 2.7.26 R2IG"; Ref = $RefR2IG }
    "navarrico_faketec_sx1262_r2ig"  = @{ Rama = "Rama 2 Routers"; Nombre = "Faketec NavTastic 2.7.26 R2IG"; Ref = $RefR2IG }
    "navarrico_seed_sx1262_r2ig"     = @{ Rama = "Rama 2 Routers"; Nombre = "Seed Solar Node P1 NavTastic 2.7.26 R2IG"; Ref = $RefR2IG }
    "navarrico_t114_sx1262_r2ig"     = @{ Rama = "Rama 2 Routers"; Nombre = "Heltec T114 NavTastic 2.7.26 R2IG"; Ref = $RefR2IG }
    "navarrico_xiao_kit_sx1262_r2ig" = @{ Rama = "Rama 2 Routers"; Nombre = "XiaoKitI2c NavTastic 2.7.26 R2IG"; Ref = $RefR2IG }
    "navarrico_xiao_e22p_r2ig"       = @{ Rama = "Rama 2 Routers"; Nombre = "XiaoKitI2c+E22P NavTastic 2.7.26 R2IG"; Ref = $RefR2IG }
    "navarrico_promicro_e22p_r1ig"   = @{ Rama = "Rama 1 Clientes"; Nombre = "Promicro NRF52+E22P NavTastic 2.7.26 R1IG"; Ref = $RefR1IG }
    "navarrico_faketec_sx1262_r1ig"  = @{ Rama = "Rama 1 Clientes"; Nombre = "Faketec NavTastic 2.7.26 R1IG"; Ref = $RefR1IG }
    "navarrico_seed_sx1262_r1ig"     = @{ Rama = "Rama 1 Clientes"; Nombre = "Seed Solar Node P1 NavTastic 2.7.26 R1IG"; Ref = $RefR1IG }
    "navarrico_t114_sx1262_r1ig"     = @{ Rama = "Rama 1 Clientes"; Nombre = "Heltec T114 NavTastic 2.7.26 R1IG"; Ref = $RefR1IG }
    "navarrico_xiao_kit_sx1262_r1ig" = @{ Rama = "Rama 1 Clientes"; Nombre = "XiaoKitI2c NavTastic 2.7.26 R1IG"; Ref = $RefR1IG }
    "navarrico_xiao_e22p_r1ig"       = @{ Rama = "Rama 1 Clientes"; Nombre = "XiaoKitI2c+E22P NavTastic 2.7.26 R1IG"; Ref = $RefR1IG }
}

$ok = 0; $fail = @()
foreach ($e in $map.Keys | Sort-Object) {
    $info = $map[$e]
    Write-Host "`n== $e =="
    # 1) Compilar en modo paridad
    & (Join-Path $root "build.ps1") -EnvName $e -Paridad
    if ($LASTEXITCODE -ne 0) { $fail += "$e (build fallo)"; continue }
    # 2) Comparar UF2 (estricto)
    $buildUf2 = Get-ChildItem -LiteralPath (Join-Path $root ".pio\build\$e") -Filter *.uf2 -File | Select-Object -First 1
    $refUf2 = Join-Path $info.Ref ("UF2\" + $info.Nombre + ".uf2")
    $h1 = (Get-FileHash -LiteralPath $buildUf2.FullName -Algorithm MD5).Hash
    $h2 = (Get-FileHash -LiteralPath $refUf2 -Algorithm MD5).Hash
    if ($h1 -eq $h2) { Write-Host "  UF2: OK  $h1"; $ok++ } else { Write-Host "  UF2: DIFF build=$h1 ref=$h2"; $fail += $e }
    # 3) Zip OTA (informativo: puede diferir solo por el nombre interno del env)
    $buildZip = Get-ChildItem -LiteralPath (Join-Path $root ".pio\build\$e") -Filter *.zip -File | Select-Object -First 1
    if ($buildZip) {
        $refZip = Join-Path $info.Ref ("OTA\" + $info.Nombre + ".zip")
        if (Test-Path -LiteralPath $refZip) {
            $z1 = (Get-FileHash -LiteralPath $buildZip.FullName -Algorithm MD5).Hash
            $z2 = (Get-FileHash -LiteralPath $refZip -Algorithm MD5).Hash
            if ($z1 -eq $z2) { Write-Host "  OTA zip: OK $z1" } else { Write-Host "  OTA zip: difiere (esperado si solo cambia el nombre del env dentro del zip)  build=$z1 ref=$z2" }
        }
    }
}

Write-Host "`n=========================================="
Write-Host ("RESULTADO: {0}/12 UF2 byte-idénticos" -f $ok)
if ($fail.Count -gt 0) { Write-Host ("DIFF: {0}" -f ($fail -join ", ")); exit 1 } else { Write-Host "PARIDAD 12/12 - unificacion probada"; exit 0 }
