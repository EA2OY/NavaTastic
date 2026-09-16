# ============================================================
# NAVARICO distribuir.ps1 - copia los binarios compilados a distribucion/
# ============================================================
# ESTRUCTURA DE SALIDA (mismo esquema que el Desktop del operador):
#
#   GENERAL (los 12 envs *ig: ramas de flota, clave publica del Master Node):
#     <destino>\Rama <1|2> <Clientes|Routers>\<LIPO|NIMH>\<UF2|OTA>\<Nombre>.uf2|.zip
#     NIMH SOLO Faketec y XiaoKitI2c (sin +E22P) - norma del operador
#
#   PROPIA (los 12 envs *ip: infraestructura propia, claves del operador):
#     <destino>\Rama <1|2> <Clientes|Routers>\<UF2|OTA>\<Nombre>.uf2|.zip
#     SIN separacion LIPO/NIMH: la quimica se decide en el despliegue, no en el binario
#
# NORMA 13: cada version va a su PROPIA carpeta, y NUNCA se sobrescriben ni se borran
#   los binarios de versiones anteriores. Por eso el destino por defecto es una carpeta
#   NUEVA con la version en el nombre.
#
# NUNCA se distribuye un env `labaudit` (son de banco, 869.545 MHz).
#
# La VERSION y el NOMBRE PUBLICO se LEEN DEL CODIGO (no estan escritos aqui).
#   - Nombre publico: NAVATASTIC_BUILD en src/modules/NavaCLIModule.h  (p. ej. "V5.2")
#   - Version de proyecto: docs/Manual_uso_NavaTastic.md, ultima fila del changelog
# Asi no vuelve a pasar lo de la V5.1, cuyos ficheros se llamaban "4.3.4" por estar
# hardcodeado en este script.
# ============================================================
param(
    [string]$EnvName = "",
    [switch]$Todo,
    [switch]$Propia,          # incluir tambien los 12 envs Propia (*ip)
    [string]$Destino = "",    # carpeta destino; por defecto Desktop\NavaTastic <V> <ver> <DDMMAA>
    [string]$NombreVersion = "", # p. ej. "V5.2"; por defecto se lee del codigo
    [string]$VersionProyecto = "" # p. ej. "4.3.9"; por defecto se lee del changelog
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# ---------- 1. Leer la version DEL CODIGO (nada hardcodeado) ----------
if (-not $NombreVersion) {
    $hdr = Join-Path $root "src\modules\NavaCLIModule.h"
    $m = Select-String -Path $hdr -Pattern '#define\s+NAVATASTIC_BUILD\s+"([^"]+)"'
    if (-not $m) { throw "No se pudo leer NAVATASTIC_BUILD de $hdr" }
    $NombreVersion = $m.Matches[0].Groups[1].Value
}
if (-not $VersionProyecto) {
    $man = Join-Path $root "docs\Manual_uso_NavaTastic.md"
    $m = Select-String -Path $man -Pattern '^\|\s*\*\*(4\.\d+\.\d+)\s'
    if (-not $m) { throw "No se pudo leer la version de proyecto del changelog en $man" }
    # La ultima fila del changelog es la version mas reciente
    $VersionProyecto = ($m | Select-Object -Last 1).Matches[0].Groups[1].Value
}
$selloVersion = "$NombreVersion $VersionProyecto"   # p. ej. "V5.2 4.3.9"
Write-Host "Version publica : $NombreVersion"
Write-Host "Version proyecto: $VersionProyecto"
Write-Host "Sello en nombres: $selloVersion"

if (-not $Destino) {
    $fecha = (Get-Date).ToString("ddMMyy")
    $Destino = "C:\Users\Jesus\Desktop\NavaTastic $NombreVersion $VersionProyecto $fecha"
}
Write-Host "Destino         : $Destino"
Write-Host ""

# ---------- 2. Mapa de entornos ----------
# Nombre historico del fichero (sin el sello de version, que se anade despues)
$placas = [ordered]@{
    "promicro_e22p"   = "Promicro NRF52+E22P"
    "faketec_sx1262"  = "Faketec"
    "seed_sx1262"     = "Seed Solar Node P1"
    "t114_sx1262"     = "Heltec T114"
    "xiao_kit_sx1262" = "XiaoKitI2c"
    "xiao_e22p"       = "XiaoKitI2c+E22P"
}
# NIMH solo Faketec + XiaoKitI2c (sin +E22P) - norma del operador
$nimhPlacas = @("faketec_sx1262", "xiao_kit_sx1262")

$map = @{}
foreach ($p in $placas.Keys) {
    foreach ($rama in @("r2", "r1")) {
        $etiquetaRama = if ($rama -eq "r2") { "Rama 2 Routers" } else { "Rama 1 Clientes" }
        $sufijo = if ($rama -eq "r2") { "R2" } else { "R1" }
        # GENERAL
        $e = "navarrico_${p}_${rama}ig"
        $map[$e] = @{
            Rama = $etiquetaRama; Placa = $placas[$p]; Modo = "GENERAL"
            Sufijo = "${sufijo}IG"; Nimh = ($nimhPlacas -contains $p)
        }
        # PROPIA
        $e = "navarrico_${p}_${rama}ip"
        $map[$e] = @{
            Rama = $etiquetaRama; Placa = $placas[$p]; Modo = "PROPIA"
            Sufijo = "${sufijo}IP"; Nimh = $false
        }
    }
}
Write-Host ("Entornos en el mapa: {0} (12 General + 12 Propia)" -f $map.Count)
Write-Host ""

# ---------- 3. Que entornos procesar ----------
$envs = @()
if ($EnvName) {
    $envs = @($EnvName)
} elseif ($Todo) {
    $envs = @($map.Keys | Where-Object { $_ -like "*ig" })
    if ($Propia) { $envs += @($map.Keys | Where-Object { $_ -like "*ip" }) }
} else {
    throw "Usa -EnvName <env>, -Todo (solo General) o -Todo -Propia (General + Propia)"
}
if ($envs -match "labaudit") { throw "PROHIBIDO distribuir un env labaudit (son de banco)" }

# ---------- 4. Copiar ----------
$copiados = 0
$faltan = @()
foreach ($e in $envs) {
    if (-not $map.ContainsKey($e)) { Write-Warning "Env no esta en el mapa de distribucion: $e"; continue }
    $info = $map[$e]
    $buildDir = Join-Path $root (".pio\build\$e")
    if (-not (Test-Path -LiteralPath $buildDir)) { $faltan += "$e (sin build)"; continue }

    # GENERAL separa por quimica; PROPIA no
    $caras = if ($info.Modo -eq "GENERAL" -and $info.Nimh) { @("LIPO", "NIMH") } elseif ($info.Modo -eq "GENERAL") { @("LIPO") } else { @("") }

    foreach ($cara in $caras) {
        foreach ($tipo in @("UF2", "OTA")) {
            $sub = @($info.Rama)
            if ($cara) { $sub += $cara }
            $sub += $tipo
            $dir = Join-Path $Destino ($sub -join "\")
            New-Item -ItemType Directory -Path $dir -Force | Out-Null

            # Los .pio/build acumulan artefactos de builds anteriores: SIEMPRE el mas reciente
            $patron = if ($tipo -eq "UF2") { "*.uf2" } else { "*.zip" }
            $src = Get-ChildItem -LiteralPath $buildDir -Filter $patron -File |
                Where-Object { $_.Name -notlike "*ota*" -and $_.Name -notlike "*factory*" } |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if (-not $src) { $faltan += "$e ($patron)"; continue }

            $ext = if ($tipo -eq "UF2") { ".uf2" } else { ".zip" }
            $nombre = "$($info.Placa) NavTastic 2.7.26 $selloVersion $($info.Sufijo)$ext"
            $dest = Join-Path $dir $nombre
            Copy-Item -LiteralPath $src.FullName -Destination $dest -Force
            $h = (Get-FileHash -LiteralPath $dest -Algorithm MD5).Hash
            Write-Host ("OK  {0}" -f $dest.Substring($Destino.Length + 1))
            Write-Host ("    MD5 {0}   (build {1})" -f $h, $src.LastWriteTime.ToString("dd/MM HH:mm"))
            $copiados++
        }
    }
}

# ---------- 5. Resumen ----------
Write-Host ""
Write-Host "============================================"
Write-Host ("Copiados: {0} ficheros" -f $copiados)
Write-Host ("Destino : {0}" -f $Destino)
if ($faltan.Count) {
    Write-Host ("PENDIENTES DE COMPILAR ({0}):" -f $faltan.Count) -ForegroundColor Yellow
    $faltan | Sort-Object -Unique | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
} else {
    Write-Host "Sin pendientes: todos los entornos pedidos estaban compilados." -ForegroundColor Green
}
Write-Host "============================================"
