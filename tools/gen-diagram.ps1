# ============================================================================
#  Genera el diagram.json del proyecto ParkNet (Wokwi)
#
#  DISENO (3 filas de piezas, nada se pisa y los cables NO se cruzan):
#
#    fila A (arriba)   SERVO
#    fila B (medio)    PANTALLA LCD 1602 (I2C)
#    fila C (bajo)     KY-033  LED 1  LED 2
#    ------------------ PROTOBOARD LARGA ------------------
#    ARDUINO UNO R4 WiFi
#
#  REGLAS DE CABLEADO:
#
#   1. Los cables del header superior del Arduino usan un carril cada uno.
#      El de la columna mas cercana va en el carril mas ALTO y el de la mas
#      lejana en el mas bajo: asi el cable largo queda por encima de todos y
#      no atraviesa los verticales de los demas.
#   2. Cada pin baja en vertical recta a la protoboard. Los pins que
#      comparten columna salen con un escalon horizontal minimo.
#   3. Los cables que bajan de las filas A y B entran en la banda SUPERIOR
#      (fila a) y los que suben del header INFERIOR en las filas c/d, en las
#      columnas de la derecha: los dos grupos no se mezclan.
# ============================================================================
param(
    [string]$Out = "C:\Users\pexji\Downloads\wokwi\diagram.json"
)

$enc = New-Object System.Text.UTF8Encoding($false)
$inv = [System.Globalization.CultureInfo]::InvariantCulture
function Num($v) { ([double]$v).ToString("0.0", $inv) }
function Int($v) { ([int][Math]::Round($v)).ToString() }

# ------------------------------------------------- geometria de la protoboard
$BB = @{ left = 400; top = 340 }
function ColX([int]$n) { $BB.left + 22.07 + ($n - 1) * 9.59 }
$ROW = @{
    tp = $BB.top + 8.4; tn = $BB.top + 18.3
    a  = $BB.top + 47.2; b = $BB.top + 56.75; c = $BB.top + 66.3
    d  = $BB.top + 75.85; e = $BB.top + 85.4
    f  = $BB.top + 114.4; j = $BB.top + 152.0
}
function RailPos([double]$x) {
    $n = [int][Math]::Round(($x - (ColX 1)) / 9.59) + 1
    if ($n -lt 1) { $n = 1 }; if ($n -gt 50) { $n = 50 }
    return $n
}

# offsets de los pines (medidos sobre el render real)
$OFF = @{
    pir = @{ vcc = 36.2; out = 45.9; gnd = 55.6; dy = 92.0 }
    led = @{ a = 25.0; c = 15.0; dy = 42.0 }
    srv = @{ dyPwm = 69.0; dyGnd = 50.0; dyV = 31.0 }
}

# pines del Arduino (medidos sobre el render real)
$PUN = @{
    '13' = @(125.0, 529.0); '12' = @(134.5, 529.0); '11' = @(144.0, 529.0)
    '10' = @(153.5, 529.0); '9' = @(163.0, 529.0);  '8' = @(173.0, 529.0)
    '7'  = @(189.0, 529.0); '6' = @(198.5, 529.0)
    '5V' = @(160.0, 711.5); 'GND.2' = @(169.5, 711.5)
    'A0' = @(208.0, 711.5); 'A1' = @(217.5, 711.5)
    'A2' = @(227.0, 711.5); 'A3' = @(236.5, 711.5)
    'A4' = @(246.0, 711.5); 'A5' = @(255.5, 711.5)
    'GND.1' = @(115.5, 529.0)
}

$UNO = @{ left = 30; top = 520 }

$parts = New-Object System.Collections.ArrayList
$conns = New-Object System.Collections.ArrayList
function Add-Part($id, $type, $left, $top, $attrs) {
    $a = if ($attrs) { $attrs } else { "{}" }
    [void]$parts.Add(('{{ "type": "{0}", "id": "{1}", "top": {2}, "left": {3}, "attrs": {4} }}' -f $type, $id, (Num $top), (Int $left), $a))
}
function Add-Label($id, $left, $top, $text) {
    Add-Part $id "wokwi-text" $left $top ('{{ "text": "{0}" }}' -f $text)
}
# ruta explicita: lista de puntos intermedios (el ultimo es el destino)
function Route($pts) {
    $r = @()
    for ($i = 1; $i -lt $pts.Count; $i++) {
        # "*" = tramo automatico: lo dibuja Wokwi (hacia el destino)
        if ($pts[$i] -eq '*') { $r += '*'; continue }
        $dx = $pts[$i][0] - $pts[$i-1][0]
        $dy = $pts[$i][1] - $pts[$i-1][1]
        if ([Math]::Abs($dx) -gt 0.05) { $r += ("h" + (Num $dx)) }
        if ([Math]::Abs($dy) -gt 0.05) { $r += ("v" + (Num $dy)) }
    }
    return $r
}
function Add-Conn($from, $to, $color, $pts) {
    $r = (@(Route $pts) | ForEach-Object { '"' + $_ + '"' }) -join ', '
    [void]$conns.Add(('[ "{0}", "{1}", "{2}", [ {3} ] ]' -f $from, $to, $color, $r))
}
# ruta en bruto (para cuando un pin esta en una posicion que no conocemos:
# se deja el tramo automatico "*" y Wokwi lo dibuja)
function Add-ConnRaw($from, $to, $color, $route) {
    $r = (@($route) | ForEach-Object { '"' + $_ + '"' }) -join ', '
    [void]$conns.Add(('[ "{0}", "{1}", "{2}", [ {3} ] ]' -f $from, $to, $color, $r))
}
# hueco de la protoboard (banda superior fila a-e / inferior f-j / rails)
function Hole([int]$col, [string]$fila) { return @((ColX $col), $ROW[$fila]) }
function Rail([string]$r, [int]$pos) { return @((ColX $pos), $ROW[$r]) }
function RailPos([double]$x) { return [int]([Math]::Round(($x - (ColX 1)) / 9.59) + 1) }

# ================================================================ 1) base
Add-Part "bb"   "wokwi-breadboard" $BB.left $BB.top "{}"
Add-Label "lblBB" ($BB.left - 10) ($BB.top - 30) "PROTOBOARD LARGA  (+5V arriba / GND abajo)"
Add-Part "uno"  "wokwi-arduino-uno" $UNO.left $UNO.top "{}"
Add-Label "lblUno" ($UNO.left - 10) ($UNO.top - 36) "ARDUINO UNO R4 WiFi"

# ================================================================ 2) FILA C: sensor y LEDs
$YC = 300
$f1 = @(
    @{ t = 'pir'; id = 'ky1';  col = 2;  et = "KY-033" },
    @{ t = 'led'; id = 'led1'; col = 11; et = "LED 1" },
    @{ t = 'led'; id = 'led2'; col = 26; et = "LED 2" }
)
foreach ($c in $f1) {
    if ($c.t -eq 'pir') {
        $left = [math]::Round((ColX $c.col) - $OFF.pir.vcc, 1)
        $top  = [math]::Round($YC - $OFF.pir.dy, 1)
        Add-Part $c.id "wokwi-pir-motion-sensor" $left $top '{ "delayTime": "5" }'
        Add-Label ("lbl_" + $c.id) $left ($top - 30) $c.et
        Add-Conn ($c.id + ":VCC") ("bb:tp." + (RailPos ($left + $OFF.pir.vcc))) "red"   @(
            @(($left + $OFF.pir.vcc), $YC), (Rail 'tp' (RailPos ($left + $OFF.pir.vcc))))
        Add-Conn ($c.id + ":GND") ("bb:tn." + (RailPos ($left + $OFF.pir.gnd))) "black" @(
            @(($left + $OFF.pir.gnd), $YC), (Rail 'tn' (RailPos ($left + $OFF.pir.gnd))))
        Add-Conn ($c.id + ":OUT") ("bb:" + $c.col + "t.a") "gold" @(
            @(($left + $OFF.pir.out), $YC), (Hole $c.col 'a'))
    } else {
        $left = [math]::Round((ColX $c.col) - $OFF.led.a, 1)
        $top  = [math]::Round($YC - $OFF.led.dy, 1)
        Add-Part $c.id "wokwi-led" $left $top '{ "color": "green" }'
        Add-Label ("lbl_" + $c.id) $left ($top - 30) $c.et
        $cat = RailPos ($left + $OFF.led.c)
        Add-Conn ($c.id + ":C") ("bb:tn." + $cat) "black" @(
            @(($left + $OFF.led.c), $YC), (Rail 'tn' $cat))
        Add-Conn ($c.id + ":A") ("bb:" + $c.col + "t.a") "gold" @(
            @(($left + $OFF.led.a), $YC), (Hole $c.col 'a'))
    }
}

# ================================================================ 3) FILA A: servo
$YA = 100
$srvCol  = 48
$srvLeft = [math]::Round((ColX $srvCol), 1)
$srvTop  = [math]::Round($YA - $OFF.srv.dyPwm, 1)
Add-Part "servo1" "wokwi-servo" $srvLeft $srvTop '{ "horn": "single" }'
Add-Label "lbl_servo1" $srvLeft 2 "SERVO"
$srvVY = $srvTop + $OFF.srv.dyV
$srvGY = $srvTop + $OFF.srv.dyGnd
$srvPY = $srvTop + $OFF.srv.dyPwm
Add-Conn "servo1:V+"  "bb:tp.49" "red"    @(
    @($srvLeft, $srvVY), @((ColX 49), $srvVY), (Rail 'tp' 49))
Add-Conn "servo1:GND" "bb:tn.50" "black" @(
    @($srvLeft, $srvGY), @((ColX 50), $srvGY), (Rail 'tn' 50))
Add-Conn "servo1:PWM" ("bb:" + $srvCol + "t.a") "orange" @(
    @($srvLeft, $srvPY), (Hole $srvCol 'a'))

# ================================================================ 4) FILA B: LCD 1602 I2C
$YM = 215
$lcdLeft = 900
$lcdTop  = 200
#   pins:"i2c"  ->  la pieza simula el backpack PCF8574 (GND, VCC, SDA, SCL)
#   i2cAddress    ->  0x27 (el de por defecto de los backpack habituales)
Add-Part "lcd" "wokwi-lcd1602" $lcdLeft $lcdTop '{ "pins": "i2c", "i2cAddress": "0x27", "color": "white", "background": "green" }'
Add-Label "lbl_lcd" $lcdLeft ($lcdTop - 30) "PANTALLA LCD 1602 (I2C)"
#   VCC y GND a los rails; SDA y SCL a dos columnas de la protoboard
#   (col 55 y 56) que despues van a los pines A4 y A5 del Arduino.
Add-ConnRaw "lcd:VCC" "bb:tp.50" "red"    @("*", "h0")
Add-ConnRaw "lcd:GND" "bb:tn.50" "black"  @("*", "h0")
Add-ConnRaw "lcd:SDA" "bb:55t.a" "green"  @("*", "h0")
Add-ConnRaw "lcd:SCL" "bb:56t.a" "blue"   @("*", "h0")
#   y los dos cables del Arduino a esas mismas columnas
Add-Conn "uno:A4" "bb:55t.d" "green" @(
    @($PUN['A4'][0], $PUN['A4'][1]), @($PUN['A4'][0], 675.5), @((ColX 55), 675.5), (Hole 55 'd'))
Add-Conn "uno:A5" "bb:56t.d" "blue"  @(
    @($PUN['A5'][0], $PUN['A5'][1]), @($PUN['A5'][0], 687.5), @((ColX 56), 687.5), (Hole 56 'd'))

# ================================================================ 5) senales del header superior
$sen = @(
    @{ uno = '13'; col = 2  },
    @{ uno = '12'; col = 11 },
    @{ uno = '10'; col = 26 }
)
$n = $sen.Count
$i = 0
foreach ($s in $sen) {
    $lane = 396 + ($i * 17)
    $p = $PUN[$s.uno]
    Add-Conn ("uno:" + $s.uno) ("bb:" + $s.col + "t.a") "gold" @(
        @($p[0], $p[1]), @($p[0], $lane), @((ColX $s.col), $lane), (Hole $s.col 'a'))
    $i++
}
#   A0 -> servo (fila c). El pin A1 queda libre (la LCD usa A4 y A5).
Add-Conn "uno:A0" ("bb:" + $srvCol + "t.c") "orange" @(
    @($PUN['A0'][0], $PUN['A0'][1]), @($PUN['A0'][0], 663.5), @((ColX $srvCol), 663.5), (Hole $srvCol 'c'))

# ================================================================ 6) alimentacion
# ==================================================================
#  REGLA DE ORO: el +5V del servo NO sale del Arduino nunca:
#    1. El +5V del rail rojo viene de UNA FUENTE EXTERNA.
#    2. El Arduino aporta SOLO su GND al rail azul  ->  masa comun.
#    3. El pin 5V del Arduino queda SIN cable.
#
#  Con la pila cuadrada de 9V la cadena es:
#      PILA 9V  ->  BUCK (reductor a 5.0 V)  ->  rail + / rail -
#  El buck entra al rail por la col 39 (por la derecha, sin cruces).
# ==================================================================
$fuenteLeft = 600
$fuenteTop  = 26
Add-Part "fuente" "wokwi-9v" $fuenteLeft $fuenteTop '{ "name": "9V" }'
Add-Label "lbl_fuente" ($fuenteLeft - 40) ($fuenteTop - 30) "PILA 9 V (cuadrada)"

#   el reductor de tension, dibujado como chip personalizado de 4 pines
$buckJson = @'
{ "type": "wokwi-custom-chip", "id": "buck", "top": 12.0, "left": 690, "attrs": { "name": "BUCK 9V->5V", "pins": "IN+:left.0,IN-:left.1,OUT+:right.0,OUT-:right.1", "pinLabels": { "IN+": "+9V", "IN-": "0V", "OUT+": "+5V", "OUT-": "GND" } }, "children": [ { "type": "rect", "props": { "x": 6, "y": 6, "width": 72, "height": 56, "rx": 4, "fill": "#111111", "stroke": "#999999", "strokeWidth": 1.5 } }, { "type": "text", "props": { "x": 42, "y": 30, "fontSize": 11, "fill": "#ffffff", "textAnchor": "middle", "text": "BUCK" } }, { "type": "text", "props": { "x": 42, "y": 50, "fontSize": 10, "fill": "#fdd835", "textAnchor": "middle", "text": "9V a 5V" } } ] }
'@
[void]$parts.Add($buckJson)
Add-Label "lbl_buck" 690 -16 "REDUCTOR BUCK 9V -> 5V (ajustar a 5.0 V)"

#   pila -> buck (pines de la izquierda)
Add-ConnRaw "fuente:+" "buck:IN+" "red"   @("*", "h0")
Add-ConnRaw "fuente:-" "buck:IN-" "black" @("*", "h0")
#   buck -> rieles (por la derecha: col 39, sin cruzar nada)
Add-Conn "buck:OUT+" "bb:tp.39" "red" @(
    @(788, 26), @(786.5, 26), (Rail 'tp' 39))
Add-Conn "buck:OUT-" "bb:tn.39" "black" @(
    @(783, 48), @(800, 48), @(800, 352), (Rail 'tn' 39))
#   El pin 5V del Arduino se queda SIN cable (serian dos fuentes en paralelo).
#   El GND del Arduino SI va al rail azul: esa es la masa comun.
$pg = $PUN['GND.1']
Add-Conn "uno:GND.1" "bb:tn.2" "black" @(
    @($pg[0], $pg[1]), @($pg[0], 381.0), @((ColX 2), 381.0), (Rail 'tn' 2))

# ================================================================ 7) tabla
$tabla = @(
"CONEXIONES COMPLETAS",
"  plaza 1   sensor KY-033 OUT -> pin 13  (col 2)  |  LED 1 -> pin 12  (col 11)",
"  plaza 2   (virtual, se simula desde la web)      |  LED 2 -> pin 10  (col 26)",
"",
"  SERVO      senal PWM -> pin A0 (col 48)  |  +5V al rail rojo  |  GND al rail negro",
"  PANTALLA   LCD 1602 con conversor I2C:",
"             SDA -> pin A4 (col 55)  |  SCL -> pin A5 (col 56)  |  +5V y GND a los rails",
"",
"  SENSORES   VCC al rail rojo (+5V)  |  GND al rail negro  |  OUT al pin del Arduino",
"  LEDS       anodo al pin del Arduino (con resistencia 330-470 ohm)  |  catodo al rail negro",
"",
"  PILA       9 V (cuadrada) -> BUCK ajustado a 5.0 V -> rail rojo/azul (col 39)",
"  PROHIBIDO: conectar la pila de 9 V directo al circuito (mata LCD y sensor).",
"  IMPORTANTE: el ráil rojo lo alimenta la FUENTE EXTERNA (5 V), no el Arduino.",
"  El Arduino sólo lleva su GND al ráil azul (masa común). Su pin 5V va libre.",
"  Así el servo no tira del USB de la placa y no hay caidas de tensión."
) -join "`n"
Add-Part "lblTabla" "wokwi-text" $UNO.left ($UNO.top + 215) ('{{ "text": "{0}" }}' -f ($tabla -replace "`n", "\n" -replace '"', "'"))

# ================================================================
$json = "{`n" + '  "version": 1,' + "`n" + '  "author": "Equipo Fixmol",' + "`n" + '  "editor": "wokwi",' + "`n" +
        '  "parts": [' + ($parts -join ",`n    ") + "]," + "`n" +
        '  "connections": [' + ($conns -join ",`n    ") + "]" + "`n" + "}"

[System.IO.File]::WriteAllText($Out, $json, $enc)
Write-Output ("Diagrama generado: " + $Out)
Write-Output ("  piezas: " + $parts.Count + " | conexiones: " + $conns.Count)
