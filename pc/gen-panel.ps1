# =====================================================================
#  gen-panel.ps1 - genera parknet_panel.h
#
#  Convierte el dashboard.html (el panel del PC) en un array de bytes
#  PROGMEM para que el propio Arduino pueda servir EXACTAMENTE la
#  misma pagina en http://192.168.4.1 (o la IP que tenga en tu red).
#
#  Se ejecuta solo desde build.ps1 antes de compilar.
# =====================================================================
param(
  [string]$Root = "C:\Users\pexji\Downloads\wokwi"
)

$html = Join-Path $Root "dashboard.html"
$dest = Join-Path $Root "parknet\parknet_panel.h"

if (-not (Test-Path $html)) { Write-Host "No encuentro $html" -ForegroundColor Red; exit 1 }

# --- leemos el HTML como bytes (UTF-8) -------------------------------
$bytes = [System.IO.File]::ReadAllBytes($html)

# --- lo escapamos como cadena de C ----------------------------------
#  \xXX es "voraz" en C++ (\x41A seria un solo caracter), asi que usamos
#  escapes octales de 3 digitos (\101) que son seguros.
$sb = New-Object System.Text.StringBuilder
foreach ($b in $bytes) {
  switch ($b) {
    34      { [void]$sb.Append('\"')  }
    92      { [void]$sb.Append('\\')  }
    10      { [void]$sb.Append('\n')   }
    13      { [void]$sb.Append('\r')   }
    9       { [void]$sb.Append('\t')   }
    default {
      if ($b -ge 32 -and $b -le 126) { [void]$sb.Append([char]$b) }
      else { [void]$sb.Append(('\{0:X3}' -f $b)) }
    }
  }
}
$cuerpo = $sb.ToString()

# --- escribimos la cabecera -----------------------------------------
$salida = @"
#ifndef PARKNET_PANEL_H
#define PARKNET_PANEL_H
// ============================================================
//  parknet_panel.h - GENERADO AUTOMATICAMENTE por gen-panel.ps1
//  NO EDITAR A MANO: edita dashboard.html y vuelve a compilar.
//  Es el panel web del PC, servido tal cual por el Arduino.
// ============================================================
#include <Arduino.h>

const char PANEL_HTML[] PROGMEM =
"$cuerpo";

const unsigned int PANEL_HTML_LEN = $($bytes.Length);

#endif
"@

[System.IO.File]::WriteAllText($dest, $salida, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("  panel: {0} bytes -> {1}" -f $bytes.Length, (Split-Path $dest -Leaf)) -ForegroundColor DarkGray
