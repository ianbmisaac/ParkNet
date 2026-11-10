<#
.SYNOPSIS
    Compila el proyecto ParkNet para Wokwi o para el UNO R4 WiFi real.

.DESCRIPTION
    -Target wokwi : compila para arduino:avr:uno (ATmega328P).
                    Es el firmware que carga el simulador de Wokwi.
    -Target r4    : compila para arduino:renesas_uno:unor4wifi (placa real).
    -Target both  : compila para ambos.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Target r4
    .\build.ps1 -Target r4 -Flash
#>
param(
    [ValidateSet('wokwi', 'r4', 'both')]
    [string]$Target = 'wokwi',

    [switch]$Flash
)

$ErrorActionPreference = 'Stop'

$Root      = $PSScriptRoot
$SketchDir = Join-Path $Root 'parknet'

# arduino-cli puede no estar en el PATH de una terminal ya abierta (VS Code
# guarda el PATH del sistema en el momento en que arranco). Usamos fallback.
$cli = (Get-Command arduino-cli -ErrorAction SilentlyContinue).Source
if (-not $cli) {
    $fallback = Join-Path $env:LOCALAPPDATA 'Programs\arduino-cli\arduino-cli.exe'
    if (Test-Path $fallback) {
        $cli = $fallback
        Write-Host "arduino-cli no esta en el PATH; usando $cli" -ForegroundColor DarkGray
    } else {
        Write-Host "arduino-cli no encontrado. Instalalo en $env:LOCALAPPDATA\Programs\arduino-cli" -ForegroundColor Red
        exit 1
    }
}

$targets = @{
    'wokwi' = @{ fqbn = 'arduino:avr:uno';                 out = 'build';    label = 'Wokwi (ATmega328P / UNO)' }
    'r4'    = @{ fqbn = 'arduino:renesas_uno:unor4wifi';   out = 'build-r4'; label = 'UNO R4 WiFi (placa real)' }
}

$list = if ($Target -eq 'both') { @('wokwi', 'r4') } else { @($Target) }

foreach ($t in $list) {
    $cfg = $targets[$t]
    Write-Host ""
    Write-Host "==> Compilando para $($cfg.label)" -ForegroundColor Cyan
    Write-Host "    FQBN: $($cfg.fqbn)"

    $outDir = Join-Path $SketchDir $cfg.out
    & $cli compile --fqbn $cfg.fqbn --output-dir $outDir $SketchDir
    if ($LASTEXITCODE -ne 0) {
        Write-Host "    ERROR de compilacion" -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "    Salida: $outDir" -ForegroundColor Green

    if ($Flash) {
        if ($t -ne 'r4') {
            Write-Host "    --Flash solo aplica a la placa real (-Target r4). Se omite." -ForegroundColor Yellow
            continue
        }
        Write-Host "    Subiendo a la placa..." -ForegroundColor Cyan
        & $cli upload --fqbn $cfg.fqbn --input-dir $outDir $SketchDir
        if ($LASTEXITCODE -ne 0) {
            Write-Host "    ERROR al subir a la placa" -ForegroundColor Red
            exit $LASTEXITCODE
        }
        Write-Host "    Flasheo OK" -ForegroundColor Green
    }
}

Write-Host ""
if ($Target -ne 'r4') {
    Write-Host "Listo. En VS Code: F1 -> 'Wokwi: Start Simulator'" -ForegroundColor Cyan
}
Write-Host "El diagrama se valida con:  wokwi-cli lint `"$Root`"" -ForegroundColor DarkGray
