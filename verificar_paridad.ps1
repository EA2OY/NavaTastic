# ============================================================
# NAVARICO verificar_paridad.ps1 - prueba que el repo unificado
# produce binarios byte-idénticos a los 12 builds originales (General).
# ============================================================
# Uso:
#   .\verificar_paridad.ps1
#   (usa por defecto la carpeta oficial del operador:
#    C:\Users\Jesus\Desktop\NavaTastic 4.3 120826)
#   .\verificar_paridad.ps1 -RefDir "C:\otra\carpeta"
#
# Como funciona:
#   1) Compila los 12 envs con NAVARICO_BUILD_EPOCH = 12/08/2026 (dia de los builds originales),
#      NAVARICO_APP_VERSION = 2.7.26.54e0d8d, y la marca temporal (__TIME__/__DATE__) extraida
#      automaticamente de cada binario de referencia (Crypto/RNG y RadioLib embeben la marca).
#   2) APP_ENV canonico (custom_meshtastic_app_env) y rutas de libdeps remapeadas
#      (custom_meshtastic_libdeps_map -> -ffile-prefix-map).
#   3) Compara MD5 del .uf2 (ESTRICTO: debe coincidir) y del .zip OTA (informativo: el zip
#      puede diferir solo por el nombre interno del env).
# Resultado esperado: 12/12 OK en .uf2 -> unificacion probada sin cambiar un byte de firmware.
# ============================================================
param(
    [string]$RefDir = "C:\Users\Jesus\Desktop\NavaTastic 4.3 120826"
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
if (-not (Test-Path -LiteralPath $RefDir)) { throw "No existe la carpeta de referencia: $RefDir" }

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

# Extrae la marca temporal embebida ("HH:MM:SSMon DD YYYY") de un binario de referencia.
# Busqueda DIRECTA en el fichero: los UF2 de PIO para nRF52 llevan solo los primeros bloques
# codificados y el resto crudo, asi que reconstruir la imagen completa desalinea la busqueda.
function Get-Stamp([string]$Uf2Path) {
    $data = [IO.File]::ReadAllBytes($Uf2Path)
    $pat = [Text.Encoding]::ASCII.GetBytes("Aug")
    for ($i = 0; $i -le $data.Length - $pat.Length; $i++) {
        $m = $true
        for ($j = 0; $j -lt $pat.Length; $j++) { if ($data[$i + $j] -ne $pat[$j]) { $m = $false; break } }
        if ($m) {
            if ($i -ge 8) {
                $time = -join ($data[($i - 8)..($i - 1)] | ForEach-Object { [char]$_ })
                if ($time -match '^\d\d:\d\d:\d\d$') {
                    # fecha: todos los builds de referencia son del 12/08/2026
                    return @{ Time = $time; Date = "Aug 12 2026" }
                }
            }
        }
    }
    throw "No se encontro la marca temporal en $Uf2Path"
}

$ok = 0; $fail = @()
foreach ($e in $map.Keys | Sort-Object) {
    $info = $map[$e]
    $refUf2 = Join-Path $RefDir ($info.Rama + "\LIPO\UF2\" + $info.Nombre + ".uf2")
    if (-not (Test-Path -LiteralPath $refUf2)) { Write-Host "FALTA referencia: $refUf2"; $fail += "$e (sin referencia)"; continue }
    try {
        $stamp = Get-Stamp $refUf2
    } catch {
        Write-Host "ERROR leyendo marca de $refUf2 : $($_.Exception.Message)"
        $fail += "$e (marca no leida)"
        continue
    }
    Write-Host "`n== $e =="
    Write-Host "   Marca de referencia: $($stamp.Time) $($stamp.Date)"
    # 1) Compilar en modo paridad con la marca de la referencia
    & (Join-Path $root "build.ps1") -EnvName $e -Paridad -BuildTime $stamp.Time -BuildDate $stamp.Date
    if ($LASTEXITCODE -ne 0) { $fail += "$e (build fallo)"; continue }
    # 2) Comparar UF2 (estricto)
    $buildUf2 = Get-ChildItem -LiteralPath (Join-Path $root ".pio\build\$e") -Filter *.uf2 -File | Select-Object -First 1
    $h1 = (Get-FileHash -LiteralPath $buildUf2.FullName -Algorithm MD5).Hash
    $h2 = (Get-FileHash -LiteralPath $refUf2 -Algorithm MD5).Hash
    if ($h1 -eq $h2) { Write-Host "  UF2: OK  $h1"; $ok++ } else { Write-Host "  UF2: DIFF build=$h1 ref=$h2"; $fail += $e }
    # 3) Zip OTA (informativo)
    $buildZip = Get-ChildItem -LiteralPath (Join-Path $root ".pio\build\$e") -Filter *.zip -File | Select-Object -First 1
    if ($buildZip) {
        $refZip = Join-Path $RefDir ($info.Rama + "\LIPO\OTA\" + $info.Nombre + ".zip")
        if (Test-Path -LiteralPath $refZip) {
            $z1 = (Get-FileHash -LiteralPath $buildZip.FullName -Algorithm MD5).Hash
            $z2 = (Get-FileHash -LiteralPath $refZip -Algorithm MD5).Hash
            if ($z1 -eq $z2) { Write-Host "  OTA zip: OK $z1" } else { Write-Host "  OTA zip: difiere (esperado si solo cambia el nombre del env dentro del zip)" }
        }
    }
}

Write-Host "`n=========================================="
Write-Host ("RESULTADO: {0}/12 UF2 byte-idénticos" -f $ok)
if ($fail.Count -gt 0) { Write-Host ("DIFF: {0}" -f ($fail -join ", ")); exit 1 } else { Write-Host "PARIDAD 12/12 - unificacion probada"; exit 0 }
