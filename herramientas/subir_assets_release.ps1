param(
    [string]$Tag = "v4.3.4",
    [string]$Repo = "EA2OY/NavaTastic"
)

$inputData = @"
protocol=https
host=github.com
path=$Repo.git
"@
$cred = $inputData | git credential fill 2>&1
$token = ($cred | Where-Object { $_ -like "password=*" }) -replace "password=",""

if (-not $token) {
    throw "Error: No se pudo obtener el token de Git Credential Manager."
}

$headers = @{
    "Authorization" = "token $token"
    "User-Agent" = "PowerShell-NavaTastic"
    "Accept" = "application/vnd.github.v3+json"
}

Write-Host "Verificando si existe la Release para el tag '$Tag' en $Repo..."
$release = try {
    Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/tags/$Tag" -Headers $headers -Method Get -ErrorAction Stop
} catch {
    $null
}

if (-not $release) {
    Write-Host "Creando nueva Release para el tag $Tag..."
    $releaseNotes = @"
## NavaTastic V5 (v4.3.4) — Sincronización Bidireccional de App Oficial, Hop-Aware Timing y Resiliencia NAV6

Esta versión introduce la arquitectura **NavaTastic V5**, elevando la estabilidad, resiliencia y supervivencia de repetidores solares en montaña a un nuevo estándar:

### 🌟 Principales Novedades de NavaTastic V5:
1. **Sincronización Bidireccional Transparente de la App Oficial**: Sincronización continua de los 12 ajustes cotidianos (rol, MQTT, telemetría, intervalos de baliza, coordenadas fijas, canales 0-7, LoRa preset y PIN BLE) hacia `/resilience.bin` V6 (`NAV6`).
2. **Hop-Aware Timing y Desacople de Traceroute**: Jitter adaptativo de 5 a 13s, retardo en malla de 300ms (0 saltos), 1.5s (1 salto) y 3.5s (>=2 saltos). Traceroute desacoplado con sondeo a los 8 segundos.
3. **Persistencia Física LoRa y Canal 0 Primario**: Comandos `/nava set_preset`, `/nava set_lora`, `/nava set_freq`, `/nava ch_set 0` con validación estricta y blindaje ante reinicios.
4. **Protocolo Botón del Pánico**: Evacuación simultánea de toda la cordillera en $T$ minutos con canal prioritario `ALERT` y comando `/nava panic_ok`.
5. **Ventana de Gracia Pre-Reboot**: 6 segundos de drenaje garantizado de cola de radio antes de cualquier reinicio o reset.
6. **Capacidad Ampliada de Auto-Favoritos**: Soporte para hasta 32 routers vecinos directos en memoria de resiliencia.

### 📦 Contenido de esta Release:
- 12 binarios `.uf2` para flasheo directo por USB (DFU).
- 12 paquetes `.zip` para actualización inalámbrica OTA desde la App oficial de Meshtastic.
- Manuales y documentación técnica en PDF actualizados a V5.
"@

    $createPayload = @{
        tag_name = $Tag
        target_commitish = "main"
        name = "NavaTastic V5 (v4.3.4)"
        body = $releaseNotes
        draft = $false
        prerelease = $false
    } | ConvertTo-Json

    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases" -Headers $headers -Method Post -Body $createPayload
    Write-Host "Release creada con éxito! ID: $($release.id)"
} else {
    Write-Host "Release existente encontrada. ID: $($release.id)"
}

$releaseId = $release.id

# Borrar assets antiguos si existen
if ($release.assets -and $release.assets.Count -gt 0) {
    foreach ($asset in $release.assets) {
        Write-Host "Borrando asset previo: $($asset.name) (ID: $($asset.id))..."
        Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/assets/$($asset.id)" -Headers $headers -Method Delete
    }
}

Write-Host "Recopilando nuevos binarios y PDFs para subir a GitHub Release..."

$filesToUpload = @()

# Binarios de Rama 2 Routers
$filesToUpload += Get-ChildItem "distribucion\Rama 2 Routers\LIPO\UF2\*.uf2"
$filesToUpload += Get-ChildItem "distribucion\Rama 2 Routers\LIPO\OTA\*.zip"

# Binarios de Rama 1 Clientes
$filesToUpload += Get-ChildItem "distribucion\Rama 1 Clientes\LIPO\UF2\*.uf2"
$filesToUpload += Get-ChildItem "distribucion\Rama 1 Clientes\LIPO\OTA\*.zip"

# PDFs
$filesToUpload += Get-ChildItem "docs\pdf\*.pdf"

$uploadUrlBase = "https://uploads.github.com/repos/$Repo/releases/$releaseId/assets?name="

foreach ($file in $filesToUpload) {
    $cleanName = $file.Name -replace " ", "."
    Write-Host "Subiendo: $cleanName ($([math]::Round($file.Length / 1KB, 1)) KB)..."
    
    $contentType = "application/octet-stream"
    if ($file.Extension -eq ".zip") { $contentType = "application/zip" }
    elseif ($file.Extension -eq ".pdf") { $contentType = "application/pdf" }

    $uploadHeaders = @{
        "Authorization" = "token $token"
        "User-Agent" = "PowerShell-NavaTastic"
        "Content-Type" = $contentType
    }

    $uri = $uploadUrlBase + [System.Uri]::EscapeDataString($cleanName)
    $response = Invoke-RestMethod -Uri $uri -Headers $uploadHeaders -Method Post -InFile $file.FullName
    Write-Host "  -> OK (ID: $($response.id))"
}

Write-Host "Verificando release actualizado..."
$finalRelease = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/$releaseId" -Headers $headers -Method Get
Write-Host "Total Assets Subidos con Exito: $($finalRelease.assets.Count)"
