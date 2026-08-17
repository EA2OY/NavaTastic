$inputData = @"
protocol=https
host=github.com
path=EA2OY/NavaTastic.git
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

$releaseId = "371184462"
Write-Host "Obteniendo lista de assets existentes en release $releaseId..."
$release = Invoke-RestMethod -Uri "https://api.github.com/repos/EA2OY/NavaTastic/releases/$releaseId" -Headers $headers -Method Get

foreach ($asset in $release.assets) {
    Write-Host "Borrando asset antiguo: $($asset.name) (ID: $($asset.id))..."
    Invoke-RestMethod -Uri "https://api.github.com/repos/EA2OY/NavaTastic/releases/assets/$($asset.id)" -Headers $headers -Method Delete
}

Write-Host "Subiendo nuevos binarios y PDFs a GitHub Release..."

$filesToUpload = @()

# Binarios de Rama 2 Routers
$filesToUpload += Get-ChildItem "distribucion\Rama 2 Routers\LIPO\UF2\*.uf2"
$filesToUpload += Get-ChildItem "distribucion\Rama 2 Routers\LIPO\OTA\*.zip"

# Binarios de Rama 1 Clientes
$filesToUpload += Get-ChildItem "distribucion\Rama 1 Clientes\LIPO\UF2\*.uf2"
$filesToUpload += Get-ChildItem "distribucion\Rama 1 Clientes\LIPO\OTA\*.zip"

# PDFs
$filesToUpload += Get-ChildItem "docs\pdf\*.pdf"

$uploadUrlBase = "https://uploads.github.com/repos/EA2OY/NavaTastic/releases/$releaseId/assets?name="

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
$finalRelease = Invoke-RestMethod -Uri "https://api.github.com/repos/EA2OY/NavaTastic/releases/$releaseId" -Headers $headers -Method Get
Write-Host "Total Assets Subidos: $($finalRelease.assets.Count)"
