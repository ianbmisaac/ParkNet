# ============================================================================
#  Dibuja el diagram.json de ParkNet a un PNG, usando las MISMAS coordenadas
#  y rutas que usa Wokwi, para revisar el cableado de un vistazo.
# ============================================================================
param(
    [string]$File = "C:\Users\pexji\Downloads\wokwi\diagram.json",
    [string]$Out  = "C:\Users\pexji\AppData\Local\Temp\opencode\esquema.png"
)
Add-Type -AssemblyName System.Drawing

$BBleft = 400.0; $BBtop = 340.0
function ColX([double]$n) { $BBleft + 22.07 + ($n - 1) * 9.59 }
$ROW = @{
    tp = $BBtop + 8.4; tn = $BBtop + 18.3
    a  = $BBtop + 47.2; b = $BBtop + 56.75; c = $BBtop + 66.3
    d  = $BBtop + 75.85; e = $BBtop + 85.4
    f  = $BBtop + 114.4; g = $BBtop + 123.4; h = $BBtop + 132.9
    i  = $BBtop + 142.4; j = $BBtop + 152.0
}
$size = @{
 'wokwi-breadboard' = @(654,208); 'wokwi-arduino-uno' = @(274,202); 'wokwi-pir-motion-sensor' = @(91,92)
 'wokwi-led' = @(40,50); 'wokwi-servo' = @(170,120); 'wokwi-max7219-matrix' = @(75,75)
 'wokwi-lcd1602' = @(183,88); 'wokwi-9v' = @(47,26); 'wokwi-custom-chip' = @(86,66)
}
$pines = @{}
$d = Get-Content $File -Raw | ConvertFrom-Json
foreach ($p in $d.parts) {
    $L = [double]$p.left; $T = [double]$p.top
    switch ($p.type) {
        'wokwi-arduino-uno' {
            $pines["uno:GND.1"] = @(115.5, 529.0); $pines["uno:13"] = @(125.0, 529.0)
            $pines["uno:12"] = @(134.5, 529.0); $pines["uno:11"] = @(144.0, 529.0)
            $pines["uno:10"] = @(153.5, 529.0); $pines["uno:9"] = @(163.0, 529.0)
            $pines["uno:8"] = @(173.0, 529.0);  $pines["uno:7"] = @(189.0, 529.0)
            $pines["uno:6"] = @(198.5, 529.0);  $pines["uno:5V"] = @(160.0, 711.5)
            $pines["uno:A0"] = @(208.0, 711.5);  $pines["uno:A1"] = @(217.5, 711.5)
            $pines["uno:A2"] = @(227.0, 711.5);  $pines["uno:A3"] = @(236.5, 711.5)
            $pines["uno:A4"] = @(246.0, 711.5);  $pines["uno:A5"] = @(255.5, 711.5)
            break }
        'wokwi-pir-motion-sensor' {
            $pines["$($p.id):VCC"] = @(($L + 36.2), ($T + 92.0))
            $pines["$($p.id):OUT"] = @(($L + 45.9), ($T + 92.0))
            $pines["$($p.id):GND"] = @(($L + 55.6), ($T + 92.0)); break }
        'wokwi-led' {
            $pines["$($p.id):A"] = @(($L + 25.0), ($T + 42.0))
            $pines["$($p.id):C"] = @(($L + 15.0), ($T + 42.0)); break }
        'wokwi-servo' {
            $pines["$($p.id):V+"]  = @(($L + 0.0), ($T + 31.0))
            $pines["$($p.id):GND"] = @(($L + 0.0), ($T + 50.0))
            $pines["$($p.id):PWM"] = @(($L + 0.0), ($T + 69.0)); break }
        'wokwi-9v' {
            $pines["$($p.id):+"] = @(($L + 13.0), ($T + 16.0))
            $pines["$($p.id):-"] = @(($L + 30.0), ($T + 16.0)); break }
        'wokwi-custom-chip' {
            $pines["$($p.id):IN+"]  = @(($L + 4.0), ($T + 22.0))
            $pines["$($p.id):IN-"]  = @(($L + 4.0), ($T + 40.0))
            $pines["$($p.id):OUT+"] = @(($L + 98.0), ($T + 14.0))
            $pines["$($p.id):OUT-"] = @(($L + 93.0), ($T + 36.0)); break }
        'wokwi-max7219-matrix' {
            $pines["$($p.id):V+"]  = @(($L + 92.5), ($T + 19.2))
            $pines["$($p.id):GND"] = @(($L + 92.5), ($T + 28.2))
            $pines["$($p.id):DIN"] = @(($L + 92.5), ($T + 37.8))
            $pines["$($p.id):CS"]  = @(($L + 92.5), ($T + 47.4))
            $pines["$($p.id):CLK"] = @(($L + 92.5), ($T + 57.0))
            $pines["$($p.id):V+.2"]  = @(($L - 20.5), ($T + 19.2))
            $pines["$($p.id):GND.2"] = @(($L - 20.5), ($T + 28.2)); break }
    }
}
function HolePos([string]$ref) {
    if ($ref -match '^bb:(\d+)([tb])\.([a-j])$') { return @((ColX ([double]$Matches[1])), ($ROW[$Matches[3]])) }
    if ($ref -match '^bb:tp\.(\d+)$') { return @((ColX ([double]$Matches[1])), $ROW.tp) }
    if ($ref -match '^bb:tn\.(\d+)$') { return @((ColX ([double]$Matches[1])), $ROW.tn) }
    return $null
}
# --- lienzo
$SC = 1.35
$W = 1300 * $SC; $H = 800 * $SC
$bmp = New-Object System.Drawing.Bitmap ([int]$W), ([int]$H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::FromArgb(30,30,30))
function X([double]$v) { return [float]($v * $SC) }
# protoboard
$penBb = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(90,90,90)), 1
$g.FillRectangle((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(60,60,60))), (X 400), (X 340), (X 654), (X 208))
for ($c = 1; $c -le 63; $c++) {
    $x = X (ColX $c)
    $g.DrawLine($penBb, $x, (X 340), $x, (X 548))
}
$g.DrawLine((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(200,60,60)), 2), (X 400), (X $ROW.tp), (X 1054), (X $ROW.tp))
$g.DrawLine((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(60,120,220)), 2), (X 400), (X $ROW.tn), (X 1054), (X $ROW.tn))
# piezas
$colores = @{ 'wokwi-led' = [System.Drawing.Color]::LimeGreen; 'wokwi-pir-motion-sensor' = [System.Drawing.Color]::SteelBlue
              'wokwi-servo' = [System.Drawing.Color]::DarkGray; 'wokwi-max7219-matrix' = [System.Drawing.Color]::Black
              'wokwi-arduino-uno' = [System.Drawing.Color]::Teal; 'wokwi-breadboard' = [System.Drawing.Color]::Gray
              'wokwi-lcd1602' = [System.Drawing.Color]::ForestGreen; 'wokwi-9v' = [System.Drawing.Color]::Crimson
              'wokwi-custom-chip' = [System.Drawing.Color]::SlateGray }
$font = New-Object System.Drawing.Font "Segoe UI", 9
$fontB = New-Object System.Drawing.Font "Segoe UI", 11, ([System.Drawing.FontStyle]::Bold)
foreach ($p in $d.parts) {
    if ($p.type -eq 'wokwi-text') {
        $txt = ($p.attrs.text -split "`n")[0]
        $g.DrawString($txt, $font, ([System.Drawing.Brushes]::White), (X ([double]$p.left)), (X ([double]$p.top)))
        continue
    }
    $s = $size[$p.type]
    $br = New-Object System.Drawing.SolidBrush ($colores[$p.type])
    $g.FillRectangle($br, (X ([double]$p.left)), (X ([double]$p.top)), (X $s[0]), (X $s[1]))
    $g.DrawRectangle((New-Object System.Drawing.Pen ([System.Drawing.Color]::White)), (X ([double]$p.left)), (X ([double]$p.top)), (X $s[0]), (X $s[1]))
    if ($p.type -ne 'wokwi-breadboard') {
        $g.DrawString($p.id, $fontB, ([System.Drawing.Brushes]::White), (X ([double]$p.left + 2)), (X ([double]$p.top + 2)))
    }
}
# cables
$cc = @{ 'gold' = [System.Drawing.Color]::Gold; 'green' = [System.Drawing.Color]::LimeGreen
         'red' = [System.Drawing.Color]::Red; 'black' = [System.Drawing.Color]::Silver
         'orange' = [System.Drawing.Color]::Orange; 'blue' = [System.Drawing.Color]::DeepSkyBlue
         'purple' = [System.Drawing.Color]::Magenta }
foreach ($c in $d.connections) {
    $p0 = $pines[$c[0]]
    $t  = if ($pines.ContainsKey($c[1])) { $pines[$c[1]] } else { HolePos $c[1] }
    if (-not $p0 -or -not $t) { continue }
    $x = [double]$p0[0]; $y = [double]$p0[1]
    $pts = New-Object System.Collections.ArrayList
    [void]$pts.Add((New-Object System.Drawing.PointF (X $x), (X $y)))
    foreach ($r in @($c[3])) {
        if ($r -match '^v(-?[\d\.]+)$') { $y = $y + [double]$Matches[1] }
        elseif ($r -match '^h(-?[\d\.]+)$') { $x = $x + [double]$Matches[1] }
        [void]$pts.Add((New-Object System.Drawing.PointF (X $x), (X $y)))
    }
    $pen = New-Object System.Drawing.Pen ($cc[$c[2]], 2.5)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLines($pen, $pts.ToArray())
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output ("PNG: " + $Out)
