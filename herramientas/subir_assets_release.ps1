param(
    [string]$Tag = "v4.3.4",
    [string]$Repo = "EA2OY/NavaTastic"
)

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$cred = @('protocol=https', 'host=github.com', '') | git credential fill
$tokenLine = ($cred | Where-Object { $_ -like "password=*" }) | Select-Object -First 1
$token = if ($tokenLine) { ($tokenLine -replace "password=","").Trim() } else { "" }

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
## NavaTastic V5 (v4.3.4) — Saneamiento Integral Clean Slate, Clave Única MasterNode y Auto-Navadmin

Esta versión consolida la arquitectura definitiva **NavaTastic V5 (v4.3.4)** con todas las correcciones integradas:

### 🛡️ Novedades y Correcciones Críticas en V5:
1. **Saneamiento Automático de Memoria (Clean Slate)**: Al actualizar o arrancar, cualquier archivo `/resilience.bin` antiguo o no conforme se purga de raíz y se recrea limpio con los 760 bytes exactos del formato `NAV6`.
2. **Clave Única MasterNode en Toda la Flota**: Unificación de la clave de administración remota de fábrica (`USERPREFS_USE_ADMIN_KEY_0` de MasterNode) en los 12 entornos.
3. **Auto-Aprovisionamiento Inteligente de Navadmin en Slot 1**: Ya no se requiere hacer Factory Reset tras instalar sobre firmware oficial; `Navadmin` se aprovisiona automáticamente en el primer arranque respetando cualquier canal previo del usuario.
4. **Protocolo Botón del Pánico Endurecido**: Persistencia atómica de parámetros LoRa en Flash antes del reboot ($T=0$), cálculo de prueba de rollback relativo al arranque fresco y avisos de texto claro en cascada por `Navadmin` con prioridad `ALERT`.
5. **Invarianza Estricta de Offsets y Sanitización Criptográfica**: Blindaje estructural de memoria y filtro `navaKeyIsValid` para purgar cualquier residuo de memoria sin entropía.
6. **Sincronización Bidireccional Transparente de la App Oficial**: Sincronización continua de los 12 ajustes cotidianos (rol, MQTT, telemetría, intervalos de baliza, coordenadas fijas, canales 0-7, LoRa preset y PIN BLE) hacia `/resilience.bin`.
7. **Hop-Aware Timing y Desacople de Traceroute**: Jitter adaptativo y retardos graduados por saltos para evitar colisiones en la cordillera.
8. **Nombre Persistente y Modo Natural**: `/nava set_name "[Largo]" "[Corto]"` (resistente a resets) y `/nava set_name flush` para liberar al modo natural.
9. **Telemetrías No Saturantes a 12 Horas (43200s)**: Por defecto para sensores de batería, clima, energía y salud.

### 📦 Contenido de esta Release:
- 12 binarios `.uf2` para flasheo directo por USB (DFU).
- 12 paquetes `.zip` para actualización inalámbrica OTA desde la App oficial de Meshtastic.
- Documentación técnica y manuales en PDF actualizados.
"@

    $createPayload = @{
        tag_name = $Tag
        target_commitish = "main"
        name = "NavaTastic V5 (v4.3.4)"
        body = $releaseNotes
        draft = $false
        prerelease = $false
    } | ConvertTo-Json

    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases" -Headers $headers -ContentType "application/json; charset=utf-8" -Method Post -Body ([System.Text.Encoding]::UTF8.GetBytes($createPayload))
    Write-Host "Release creada con éxito! ID: $($release.id)"
} else {
    Write-Host "Release existente encontrada. ID: $($release.id)"
}

if (-not $release -or -not $release.id) {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/tags/$Tag" -Headers $headers -Method Get
}

if (-not $release -or -not $release.id) {
    throw "Error fatal: No se pudo crear ni obtener la Release en GitHub para el tag $Tag."
}

$releaseId = $release.id
Write-Host "Usando Release ID: $releaseId"

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

$uploadUrlBase = if ($release.upload_url) { $release.upload_url -replace '\{\?name,label\}', '?name=' } else { "https://uploads.github.com/repos/$Repo/releases/$releaseId/assets?name=" }

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
