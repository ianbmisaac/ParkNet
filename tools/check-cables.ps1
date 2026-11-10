# ============================================================================
#  Verificador de CRUCES DE CABLES del diagram.json de ParkNet.
#
#  Reproduce el enrutado real de Wokwi:  v(dy) -> h(hasta la columna) -> v
#  y comprueba, par a par, si dos cables se cruzan o se solapan.
# ============================================================================
param(
    [string]$File = "C:\Users\pexji\Downloads\wokwi\diagram.json"
)

$BBleft = 400.0; $BBtop = 340.0
function ColX([double]$n) { $BBleft + 22.07 + ($n - 1) * 9.59 }
$ROW = @{
    tp = $BBtop + 8.4;  tn = $BBtop + 18.3
    a  = $BBtop + 47.2;  b = $BBtop + 56.75; c = $BBtop + 66.3
    d  = $BBtop + 75.85; e = $BBtop + 85.4
    f  = $BBtop + 114.4; g = $BBtop + 123.4; h = $BBtop + 132.9
    i  = $BBtop + 142.4; j = $BBtop + 152.0
}

$d = Get-Content $File -Raw | ConvertFrom-Json

# ---- posiciones de los pines de cada pieza -------------------------------
$pines = @{}
foreach ($p in $d.parts) {
    $L = [double]$p.left
    $T = [double]$p.top
    switch ($p.type) {
        'wokwi-arduino-uno' {
            $pines["uno:GND.1"] = @(115.5, 529.0)
            $pines["uno:13"] = @(125.0, 529.0)
            $pines["uno:12"] = @(134.5, 529.0)
            $pines["uno:11"] = @(144.0, 529.0)
            $pines["uno:10"] = @(153.5, 529.0)
            $pines["uno:9"] = @(163.0, 529.0)
            $pines["uno:8"] = @(173.0, 529.0)
            $pines["uno:7"] = @(189.0, 529.0)
            $pines["uno:6"] = @(198.5, 529.0)
            $pines["uno:5V"] = @(160.0, 711.5)
            $pines["uno:GND.2"] = @(169.5, 711.5)
            $pines["uno:A0"] = @(208.0, 711.5)
            $pines["uno:A1"] = @(217.5, 711.5)
            $pines["uno:A2"] = @(227.0, 711.5)
            $pines["uno:A3"] = @(236.5, 711.5)
            break
        }
        'wokwi-pir-motion-sensor' {
            $pines["$($p.id):VCC"] = @(($L + 36.2), ($T + 92.0))
            $pines["$($p.id):OUT"] = @(($L + 45.9), ($T + 92.0))
            $pines["$($p.id):GND"] = @(($L + 55.6), ($T + 92.0))
            break
        }
        'wokwi-led' {
            $pines["$($p.id):A"] = @(($L + 25.0), ($T + 42.0))
            $pines["$($p.id):C"] = @(($L + 15.0), ($T + 42.0))
            break
        }
        'wokwi-servo' {
            $pines["$($p.id):V+"]  = @(($L + 0.0), ($T + 31.0))
            $pines["$($p.id):GND"] = @(($L + 0.0), ($T + 50.0))
            $pines["$($p.id):PWM"] = @(($L + 0.0), ($T + 69.0))
            break
        }
        'wokwi-max7219-matrix' {
            $pines["$($p.id):V+"]  = @(($L + 92.5), ($T + 19.2))
            $pines["$($p.id):GND"] = @(($L + 92.5), ($T + 28.2))
            $pines["$($p.id):DIN"] = @(($L + 92.5), ($T + 37.8))
            $pines["$($p.id):CS"]  = @(($L + 92.5), ($T + 47.4))
            $pines["$($p.id):CLK"] = @(($L + 92.5), ($T + 57.0))
            $pines["$($p.id):V+.2"]  = @(($L - 20.5), ($T + 19.2))
            $pines["$($p.id):GND.2"] = @(($L - 20.5), ($T + 28.2))
            break
        }
    }
}
# ---- huecos de la protoboard ---------------------------------------------
function HolePos([string]$ref) {
    if ($ref -match '^bb:(\d+)([tb])\.([a-j])$') {
        return @((ColX ([double]$Matches[1])), ($ROW[$Matches[3]]))
    }
    if ($ref -match '^bb:tp\.(\d+)$') { return @((ColX ([double]$Matches[1])), $ROW.tp) }
    if ($ref -match '^bb:tn\.(\d+)$') { return @((ColX ([double]$Matches[1])), $ROW.tn) }
    return $null
}

# ---- polilinea de cada cable ---------------------------------------------
$cables = @()
$fallos = 0
foreach ($c in $d.connections) {
    $from = $c[0]; $to = $c[1]
    $p = $pines[$from]; $t = HolePos $to
    if (-not $p) { Write-Output ("  ?? pin desconocido: " + $from); $fallos++; continue }
    if (-not $t) { Write-Output ("  ?? hueco desconocido: " + $to); $fallos++; continue }
    # ruta: se aplican los tramos explicitos v/h en orden; si hay "*" se anade
    # el tramo automatico (horizontal hasta la columna + vertical al hueco)
    $x = [double]$p[0]; $y = [double]$p[1]
    $pts = @(@($x, $y))
    $auto = $false
    foreach ($r in @($c[3])) {
        if ($r -eq '*') { $auto = $true; continue }
        if ($r -match '^v(-?[\d\.]+)$') { $y = $y + [double]$Matches[1] }
        elseif ($r -match '^h(-?[\d\.]+)$') { $x = $x + [double]$Matches[1] }
        $pts += @(@($x, $y))
    }
    if ($auto) {
        $pts += @(@($t[0], $y))
        $pts += @(@($t[0], $t[1]))
    }
    $cables += [PSCustomObject]@{
        id  = "$from -> $to"
        col = $c[2]
        pts = $pts
    }
}
Write-Output ("  cables analizados: " + $cables.Count + "   (sin resolver: " + $fallos + ")")

# ---- interseccion de dos segmentos ---------------------------------------
function SegHit($a1, $a2, $b1, $b2) {
    $d1x = $a2[0] - $a1[0]; $d1y = $a2[1] - $a1[1]
    $d2x = $b2[0] - $b1[0]; $d2y = $b2[1] - $b1[1]
    $den = $d1x * $d2y - $d1y * $d2x
    if ([Math]::Abs($den) -lt 1e-9) { return $false }
    $sx = $b1[0] - $a1[0]; $sy = $b1[1] - $a1[1]
    $t = ($sx * $d2y - $sy * $d2x) / $den
    $u = ($sx * $d1y - $sy * $d1x) / $den
    if ($t -lt 0 -or $t -gt 1 -or $u -lt 0 -or $u -gt 1) { return $false }
    return $true
}
function PtNear($p, $q) {
    return ([Math]::Abs($p[0] - $q[0]) -lt 1.5) -and ([Math]::Abs($p[1] - $q[1]) -lt 1.5)
}

$hits = @()
for ($i = 0; $i -lt $cables.Count; $i++) {
    for ($j = $i + 1; $j -lt $cables.Count; $j++) {
        $A = $cables[$i]; $B = $cables[$j]
        $touch = $false
        for ($k = 0; $k -lt 4; $k++) {
            for ($m = 0; $m -lt 4; $m++) {
                if (PtNear $A.pts[$k] $B.pts[$m]) { $touch = $true }
            }
        }
        if ($touch) { continue }
        for ($k = 0; $k -lt ($A.pts.Count - 1); $k++) {
            for ($m = 0; $m -lt ($B.pts.Count - 1); $m++) {
                if (SegHit $A.pts[$k] $A.pts[$k+1] $B.pts[$m] $B.pts[$m+1]) {
                    $hits += [PSCustomObject]@{ a = $A.id; b = $B.id; ca = $A.col; cb = $B.col }
                }
            }
        }
    }
}
if ($hits.Count -eq 0) {
    Write-Output "  OK: NINGUN cable cruza ni se solapa con otro"
} else {
    Write-Output ("  CRUCES: " + $hits.Count)
    $hits | ForEach-Object { Write-Output ("    [" + $_.ca + "] " + $_.a + "   X   [" + $_.cb + "] " + $_.b) }
}
