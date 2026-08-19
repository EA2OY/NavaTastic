# ============================================================
# generar_pdf.ps1 - Convierte los manuales .md de docs\ a PDF
# usando Pandoc + XeLaTeX con la plantilla NavaTastic.
#
# REPO UNIFICADO (14/08/2026): portado desde 4.3
#   (HerramientasPropiasIA\generar_pdf.ps1, SOLO LECTURA alli).
#   Entrada: docs\ (manuales .md) | Salida: docs\pdf\ (gitignored).
#   Plantilla intacta: plantilla_navatastic.tex (copia 1:1 de 4.3).
#   Norma 11/08: solo manuales de firmware y comandos -> NO generar PDFs de
#   contexto: transfer_context, guia_integracion, GUIA_AGENTE_NAVTASTIC,
#   INSTRUCCION_AUDITORIA_CLAUDE (el "tercer PDF que no sirve").
#   La exclusion esta en $Excluir (override con -Excluir).
#
# USO:
#   .\generar_pdf.ps1                     # convierte TODOS los .md de docs\ salvo $Excluir
#   .\generar_pdf.ps1 -Archivo Manual_NavaTastic.md   # solo ese (relativo a docs\)
#   .\generar_pdf.ps1 -Carpeta ".\docs"   # los .md de una subcarpeta
#   .\generar_pdf.ps1 -Salida ".\docs\pdf" -Carpeta ".\docs"
#   .\generar_pdf.ps1 -Excluir @()        # sin exclusiones (genera todo, NO usar por defecto)
#
# Requisitos: Pandoc y MiKTeX instalados (ver docs/transfer_context.md).
# ============================================================

param(
    [string]$Archivo = "",
    [string]$Carpeta = "$PSScriptRoot\..\docs",
    [string]$Salida = "$PSScriptRoot\..\docs\pdf",
    [string]$Plantilla = "$PSScriptRoot\plantilla_navatastic.tex",
    [string[]]$Excluir = @("transfer_context.md", "guia_integracion_navarrico.md", "GUIA_AGENTE_NAVTASTIC.md", "INSTRUCCION_AUDITORIA_CLAUDE.md", "Compilar_NavaTastic.md", "Guia_para_agente_sobre_NavaTastic.md", "BITACORA_TECNICA.md", "PLAN_DE_TRABAJO.md", "PORTING_NUEVO_FORK.md")
)

$ErrorActionPreference = "Continue"

# Localizar pandoc
$pandoc = Get-Command pandoc -ErrorAction SilentlyContinue
if (-not $pandoc) {
    $local = "$env:LOCALAPPDATA\Pandoc\pandoc.exe"
    if (Test-Path $local) { $pandoc = $local } else { throw "Pandoc no encontrado. Instala Pandoc y reinicia la terminal." }
} else {
    $pandoc = $pandoc.Source
}

# Localizar xelatex (MiKTeX)
$xelatex = Get-Command xelatex -ErrorAction SilentlyContinue
if (-not $xelatex) {
    $miktex = "C:\Program Files\MiKTeX\miktex\bin\x64\xelatex.exe"
    if (Test-Path $miktex) { $xelatex = $miktex } else { throw "xelatex (MiKTeX) no encontrado. Instala MiKTeX y reinicia la terminal." }
} else {
    $xelatex = $xelatex.Source
}

# Carpeta de salida: crearla si no existe
if (-not (Test-Path -LiteralPath $Salida)) {
    New-Item -ItemType Directory -Path $Salida -Force | Out-Null
}

# Determinar ficheros a convertir
if ($Archivo) {
    # Aceptar ruta relativa: resolver contra la carpeta por defecto (docs)
    if (-not [System.IO.Path]::IsPathRooted($Archivo) -and -not (Test-Path -LiteralPath $Archivo)) {
        $candidato = Join-Path $Carpeta $Archivo
        if (Test-Path -LiteralPath $candidato) { $Archivo = $candidato }
    }
    $archivos = @($Archivo)
} elseif ($Carpeta) {
    $archivos = Get-ChildItem -Path $Carpeta -Filter "*.md" | Where-Object { $_.Name -notin $Excluir } | ForEach-Object { $_.FullName }
} else {
    $archivos = Get-ChildItem -Path $PSScriptRoot -Filter "*.md" | ForEach-Object { $_.FullName }
}

if ($Excluir) {
    Write-Host "Excluidos (norma 11/08): $($Excluir -join ', ')" -ForegroundColor Yellow
}

if ($archivos.Count -eq 0) {
    Write-Host "No se encontraron archivos .md que convertir." -ForegroundColor Yellow
    exit 0
}

# Plantilla opcional: si no existe, generar sin ella
if (Test-Path -LiteralPath $Plantilla) {
    $templateArgs = @("--template=`"$Plantilla`"")
} else {
    Write-Host "AVISO: No se encontro la plantilla ($Plantilla). Se genera con la plantilla por defecto de Pandoc." -ForegroundColor Yellow
    $templateArgs = @()
}

# Cartel del operador como PRIMERA pagina de los manuales (norma: flyer NavaTastic Eclipse V3).
# Se copia a %TEMP% (ruta sin espacios, segura para \includegraphics de XeLaTeX; la imagen
# queda embebida en el PDF, la copia temporal solo existe durante la generacion).
$flyerHd = Join-Path $PSScriptRoot "..\branding\flyer_navatastic_eclipse_v3_hd.jpg"
if (Test-Path -LiteralPath $flyerHd) {
    $flyerTmp = Join-Path $env:TEMP "navatastic_flyer_hd.jpg"
    Copy-Item -LiteralPath $flyerHd -Destination $flyerTmp -Force
    $flyerVar = $flyerTmp -replace '\\', '/'
    $flyerArgs = @("-V", "flyer=$flyerVar")
} else {
    Write-Host "AVISO: flyer HD no encontrado ($flyerHd) - portadas sin cartel." -ForegroundColor Yellow
    $flyerArgs = @()
}

Write-Host "== Generador de PDF NavaTastic ==" -ForegroundColor Cyan
Write-Host "Pandoc: $pandoc"
Write-Host "XeLaTeX: $xelatex"
Write-Host "Plantilla: $Plantilla"
Write-Host "Salida: $Salida"
Write-Host ""

$ok = 0
$fail = 0
foreach ($md in $archivos) {
    $pdf = Join-Path $Salida ([System.IO.Path]::GetFileNameWithoutExtension($md) + ".pdf")
    Write-Host "-> $([System.IO.Path]::GetFileName($md))" -ForegroundColor White
    try {
        $env:Path = "$([System.IO.Path]::GetDirectoryName($xelatex));$([System.IO.Path]::GetDirectoryName($pandoc));" + $env:Path
        & $pandoc $md -o $pdf --pdf-engine=xelatex @templateArgs @flyerArgs -V colorlinks=true -V linkcolor=navGold -V urlcolor=navNeon -V geometry:margin=2.5cm -V toc=true 2>$null
        if ($LASTEXITCODE -ne 0) { throw "Pandoc fallo con codigo $LASTEXITCODE" }
        $size = [math]::Round((Get-Item $pdf).Length/1KB)
        Write-Host "   OK -> $([System.IO.Path]::GetFileName($pdf)) ($size KB)" -ForegroundColor Green
        $ok++
    } catch {
        Write-Host "   ERROR: $($_.Exception.Message)" -ForegroundColor Red
        $fail++
    }
}

Write-Host ""
Write-Host "Resumen: $ok generados, $fail con errores." -ForegroundColor Cyan
if ($fail -gt 0) { exit 1 } else { exit 0 }
