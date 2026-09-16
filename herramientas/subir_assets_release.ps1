# ============================================================
# NAVARICO 16/09/2026 - Publica una Release en GitHub con los binarios GENERAL y los
# manuales en PDF. Reescrito para no repetir los errores de la version anterior:
#
#   - La VERSION y el NOMBRE ya NO estan escritos a mano: se leen del codigo
#     (NAVATASTIC_BUILD en src/modules/NavaCLIModule.h) y del changelog
#     (ultima fila de docs/Manual_uso_NavaTastic.md). Las notas de la release se
#     construyen con ESA fila del changelog, no con un texto viejo.
#   - Sube SOLO las rutas General: 12 .uf2 + 12 .zip de LIPO + los .bin de las
#     Heltec V3/V4 (APP y FACTORY) + los 2 manuales en PDF. NUNCA rutas *IP*
#     (ramas de infraestructura propia): ver docs/REGLAS_Y_VERDADES.md 4ter.
#   - Si un asset ya existe con el mismo nombre, lo SALTA (antes los borraba todos
#     de golpe, que es peligroso). Para reemplazar todo: -Reemplazar.
#
# USO:
#   .\subir_assets_release.ps1                       # lee la version del codigo y sube
#   .\subir_assets_release.ps1 -Origen "C:\...\NavaTastic V5.2 4.3.9 160926"
#   .\subir_assets_release.ps1 -QueSi               # solo ensena lo que subiria
#   .\subir_assets_release.ps1 -SoloNotas           # escribe las notas en %TEMP% y sale
#   .\subir_assets_release.ps1 -Reemplazar          # borra los assets previos y sube
#
# ANTES DE LANZARLO: que compile NO es prueba. La release no se publica sin
# verificacion en banco (/nava status debe decir la version nueva en un nodo real).
# ============================================================
param(
    [string]$Tag = "",
    [string]$Repo = "EA2OY/NavaTastic",
    [string]$Origen = "",
    [string]$Rama = "main",
    [switch]$QueSi,
    [switch]$SoloNotas,
    [switch]$Reemplazar
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Origen) { $Origen = Join-Path $root "distribucion" }

# ---------- 1. Version desde el CODIGO ----------
$hdr = Join-Path $root "src\modules\NavaCLIModule.h"
$m = Select-String -Path $hdr -Pattern '#define\s+NAVATASTIC_BUILD\s+"([^"]+)"'
if (-not $m) { throw "No se pudo leer NAVATASTIC_BUILD de $hdr" }
$NombreVersion = $m.Matches[0].Groups[1].Value          # p. ej. "V5.2"
if (-not $Tag) { $Tag = "v" + ($NombreVersion -replace '^[Vv]', '') }   # "v5.2"

# ---------- 2. Notas desde el CHANGELOG (ES + EN, como en la release V5.1) ----------
$man = Join-Path $root "docs\Manual_uso_NavaTastic.md"
$filas = Select-String -Path $man -Pattern '^\|\s*\*\*(4\.\d+\.\d+)\s' -Encoding UTF8
if (-not $filas) { throw "No se encontro el changelog en $man" }
# El changelog lleva el bloque ES y luego el EN: se coge la version MAS ALTA y, de ella,
# la primera fila (ES) y la ultima (EN). Asi no hay numeros escritos a mano aqui.
$maxVer = ($filas | ForEach-Object { [version]$_.Matches[0].Groups[1].Value } | Sort-Object -Descending | Select-Object -First 1)
$delVersion = @($filas | Where-Object { [version]$_.Matches[0].Groups[1].Value -eq $maxVer })
$VersionProyecto = $maxVer.ToString()

function Get-CeldaTexto($fila) {
    $c = $fila.Line -split '\|'
    if ($c.Count -ge 3) { return $c[2].Trim() }
    return ""
}
$novedadesES = Get-CeldaTexto $delVersion[0]
$novedadesEN = if ($delVersion.Count -gt 1) { Get-CeldaTexto $delVersion[-1] } else { "" }
if (-not $novedadesES) { throw "No se pudo leer el texto del changelog de la version $VersionProyecto" }
$fecha = if ($delVersion[0].Line -match '\((\d{2}/\d{2}/\d{4})\)') { $Matches[1] } else { (Get-Date).ToString("dd/MM/yyyy") }

$bloqueEN = if ($novedadesEN) { "`n**EN** -- What is new for the user:`n$novedadesEN`n" } else { "" }

$notas = @"
**NavaTastic Eclipse $NombreVersion** · $fecha · firmware sobre Meshtastic 2.7.26

**ES** — Novedades para el usuario:
$novedadesES
$bloqueEN
### Contenido de esta release / What is in this release

- **12 binarios ``.uf2``** para flasheo por USB y **12 paquetes ``.zip``** para actualización OTA, de las 6 placas nRF52840 (Promicro NRF52+E22P, Faketec, Seed Solar Node P1, Heltec T114, XiaoKitI2c y XiaoKitI2c+E22P), en **Rama 1 Clientes** y **Rama 2 Routers**.
- **8 ficheros para las Heltec V3 y V4** (ESP32-S3) en ``.bin``: **APP** (actualización) y **FACTORY** (instalación desde cero).
- **Manual de uso** y **manual de administración remota** en PDF.

> Los ``.uf2`` se copian a la unidad que aparece al pulsar dos veces RESET; los ``.zip`` se instalan desde la App oficial de Meshtastic. La química de la batería (LiPo o NiMH) se elige al desplegar, no en el binario.
"@

Write-Host "Release      : $Tag  ($NombreVersion / v$VersionProyecto)"
Write-Host "Origen       : $Origen"
Write-Host ""

# -SoloNotas: escribe el texto de las notas en un fichero y sale. Sirve para revisarlas o
# para actualizar las notas de una release YA publicada sin volver a subir binarios.
if ($SoloNotas) {
    $destino = Join-Path $env:TEMP "navatastic_notas_release.md"
    [System.IO.File]::WriteAllText($destino, $notas, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "Notas escritas en: $destino"
    exit 0
}

# ---------- 3. Que se sube ----------
if (-not (Test-Path -LiteralPath $Origen)) { throw "No existe la carpeta de origen: $Origen" }
# Estructura nueva (16/09/2026): <origen>\Infraestructura general\Rama <1|2> ...
# Se admite tambien la estructura antigua (las ramas directamente en la raiz) para poder
# subir assets de carpetas de releases anteriores.
$raices = @()
foreach ($r in @("Rama 2 Routers", "Rama 1 Clientes")) {
    $nueva = Join-Path $Origen "Infraestructura general\$r"
    if (Test-Path -LiteralPath $nueva) { $raices += $nueva }
    $vieja = Join-Path $Origen $r
    if (Test-Path -LiteralPath $vieja) { $raices += $vieja }
}
if ($raices.Count -eq 0) { throw "No encuentro las ramas dentro de $Origen" }
Write-Host ("Ramas encontradas: {0}" -f $raices.Count)
$candidatos = @()
foreach ($r in $raices) {
    $candidatos += Get-ChildItem -LiteralPath (Join-Path $r "LIPO\UF2") -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @(".uf2", ".bin") }
    $candidatos += Get-ChildItem -LiteralPath (Join-Path $r "LIPO\OTA") -File -Filter "*.zip" -ErrorAction SilentlyContinue
}
foreach ($p in @("Manual_NavaTastic.pdf", "Manual_uso_NavaTastic.pdf")) {
    $f = Join-Path $root "docs\pdf\$p"
    if (Test-Path -LiteralPath $f) { $candidatos += Get-Item -LiteralPath $f }
}
$filesToUpload = $candidatos | Where-Object { $_ } | Sort-Object Name
if (-not $filesToUpload -or $filesToUpload.Count -eq 0) { throw "No hay nada que subir en $Origen" }

Write-Host ("Ficheros a subir: {0}" -f $filesToUpload.Count)
$filesToUpload | ForEach-Object { Write-Host ("  {0}  ({1} KB)" -f $_.Name, [math]::Round($_.Length / 1KB)) }
if ($QueSi) { Write-Host ""; Write-Host "MODO -QueSi: no se sube nada." ; exit 0 }

# ---------- 4. Token del gestor de credenciales de git ----------
$cred = @('protocol=https', 'host=github.com', '') | git credential fill
$tokenLine = ($cred | Where-Object { $_ -like "password=*" }) | Select-Object -First 1
$token = if ($tokenLine) { ($tokenLine -replace "password=", "").Trim() } else { "" }
if (-not $token) { throw "No se pudo obtener el token de Git Credential Manager." }

$headers = @{
    "Authorization" = "token $token"
    "User-Agent"    = "PowerShell-NavaTastic"
    "Accept"        = "application/vnd.github.v3+json"
}

Write-Host ""
Write-Host "Buscando la release '$Tag' en $Repo..."
$release = $null
try {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/tags/$Tag" -Headers $headers -Method Get -ErrorAction Stop
} catch { $release = $null }

if (-not $release) {
    Write-Host "No existe: se crea la release $Tag (apuntando a la rama $Rama)."
    $payload = @{
        tag_name         = $Tag
        target_commitish = $Rama
        name             = "NavaTastic Eclipse $NombreVersion"
        body             = $notas
        draft            = $false
        prerelease       = $false
    } | ConvertTo-Json
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases" -Headers $headers `
        -ContentType "application/json; charset=utf-8" -Method Post -Body ([System.Text.Encoding]::UTF8.GetBytes($payload))
    Write-Host "Release creada. ID: $($release.id)"
} else {
    Write-Host "Ya existe la release. ID: $($release.id)"
    if ($Reemplazar) {
        foreach ($a in $release.assets) {
            Write-Host "  borrando asset previo: $($a.name)"
            Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/assets/$($a.id)" -Headers $headers -Method Delete
        }
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/$($release.id)" -Headers $headers -Method Get
    }
}

$releaseId = $release.id
$yaSubidos = @($release.assets | ForEach-Object { $_.name })
$uploadUrl = "https://uploads.github.com/repos/$Repo/releases/$releaseId/assets?name="

# ---------- 5. Subir ----------
$ok = 0; $saltados = 0; $fallos = 0
foreach ($file in $filesToUpload) {
    $cleanName = $file.Name -replace " ", "."
    if ($yaSubidos -contains $cleanName) {
        Write-Host ("SALTADO (ya existe): {0}" -f $cleanName) -ForegroundColor Yellow
        $saltados++
        continue
    }
    $contentType = "application/octet-stream"
    if ($file.Extension -eq ".zip") { $contentType = "application/zip" }
    elseif ($file.Extension -eq ".pdf") { $contentType = "application/pdf" }

    $h = @{
        "Authorization" = "token $token"
        "User-Agent"    = "PowerShell-NavaTastic"
        "Content-Type"  = $contentType
    }
    try {
        $uri = $uploadUrl + [System.Uri]::EscapeDataString($cleanName)
        $resp = Invoke-RestMethod -Uri $uri -Headers $h -Method Post -InFile $file.FullName
        Write-Host ("OK  {0}  (id {1})" -f $cleanName, $resp.id)
        $ok++
    } catch {
        Write-Host ("FALLO: {0}  -> {1}" -f $cleanName, $_.Exception.Message) -ForegroundColor Red
        $fallos++
    }
}

$final = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/$releaseId" -Headers $headers -Method Get
Write-Host ""
Write-Host "============================================"
Write-Host ("Subidos: {0} | saltados: {1} | fallos: {2}" -f $ok, $saltados, $fallos)
Write-Host ("Release {0}: {1} assets en total" -f $Tag, $final.assets.Count)
Write-Host ("URL: {0}" -f $final.html_url)
Write-Host "============================================"
if ($fallos -gt 0) { exit 1 }
