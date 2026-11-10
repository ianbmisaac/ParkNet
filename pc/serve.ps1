<#
    ParkNet - servidor local del dashboard.

    Todo ocurre en memoria, en este mismo proceso:
      - El Arduino real se habla por el puerto serie COM3.
      - El simulador de Wokwi se habla por RFC2217 (localhost:4000).
      - El navegador solo hace peticiones HTTP: funciona en cualquier
        navegador y no necesita permisos de puerto.

    Endpoints:
        GET  /api/health?src=com|wokwi
        GET  /api/state?src=com|wokwi
        POST /api/send?src=com|wokwi     body = comando JSON
        POST /api/open?src=com           reintenta abrir el puerto COM3
#>
param(
    [string]$Root = $(if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }),
    [int]$Port = 8765,
    [int]$WokwiPort = 4000,
    [string]$ComPort = "",
    [int]$Baud = 115200
)

$ErrorActionPreference = 'Continue'

# ------------------------------------------------------------- puerto COM
# Si no se indica, buscamos el Arduino entre los puertos del sistema: en el
# PC de la universidad puede salir en COM5, COM7... en vez de COM3.
if (-not $ComPort) {
    $candidatos = @()
    try {
        $candidatos = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'COM(\d+)' } |
            ForEach-Object { "COM" + [regex]::Match($_.Name, 'COM(\d+)').Groups[1].Value } |
            Where-Object { $_ } | Select-Object -Unique
    } catch { }
    if ($candidatos -contains 'COM3') { $ComPort = 'COM3' }
    elseif ($candidatos.Count -ge 1)   { $ComPort = $candidatos[0] }
    else                              { $ComPort = 'COM3' }
}

# ------------------------------------------------------------- estado en memoria
$script:ser        = $null
$script:serRx      = New-Object System.Text.StringBuilder
$script:comState   = $null
$script:comError   = ''
$script:rawLog     = New-Object System.Collections.ArrayList
$script:comSuspendido = $false
$script:tcp        = $null
$script:stream     = $null
$script:rx2        = New-Object System.Text.StringBuilder
$script:iac        = 0
$script:wokwiState = $null

# =========================================================== Arduino real (COM3)
function Connect-Com {
    if ($script:comSuspendido) { return $false }
    try {
        if ($script:ser -and $script:ser.IsOpen) { return $true }
        $p = New-Object System.IO.Ports.SerialPort $ComPort, $Baud, "None", 8, "One"
        $p.ReadTimeout = 200
        # En el UNO R4 el USB CDC solo transmite si el host afirma DTR.
        $p.DtrEnable = $true
        $p.RtsEnable = $true
        $p.Open()
        $script:ser = $p
        $script:serRx.Clear() | Out-Null
        $script:comError = ''
        Write-Host "  [com] puerto $ComPort abierto a $Baud baud"
        return $true
    } catch {
        $script:ser = $null
        $script:comError = $_.Exception.Message
        return $false
    }
}

# Cierra el COM3 para que otro programa (Arduino IDE,/platformio) lo pueda usar.
function Release-Com {
    if ($script:ser) {
        try { $script:ser.Close(); $script:ser.Dispose() } catch { }
        $script:ser = $null
        Write-Host "  [com] puerto $ComPort liberado"
    }
    $script:comSuspendido = $true
}

function Pump-Com {
    if (-not (Connect-Com)) { return }
    try {
        while ($script:ser.BytesToRead -gt 0) {
            $s = $script:ser.ReadExisting()
            if (-not $s) { break }
            [void]$script:serRx.Append($s)
            $todo = $script:serRx.ToString()
            $idx = $todo.LastIndexOf("`n")
            if ($idx -lt 0) { continue }
            $lineas = $todo.Substring(0, $idx) -split "`r?`n"
            $script:serRx.Clear() | Out-Null
            [void]$script:serRx.Append($todo.Substring($idx + 1))
            foreach ($l in $lineas) {
                $t = $l.Trim()
                if ($t -eq '') { continue }
                # log crudo: todo lo que dice la placa (incluido el "[TEST]")
                [void]$script:rawLog.Add($t)
                while ($script:rawLog.Count -gt 600) { $script:rawLog.RemoveAt(0) }
                if ($t.StartsWith('{') -and $t.EndsWith('}')) { $script:comState = $t }
            }
        }
    } catch {
        $script:ser = $null
        $script:comError = $_.Exception.Message
    }
}

function Send-Com([string]$texto) {
    if (-not (Connect-Com)) { return $false }
    try {
        $script:ser.Write($texto + "`n")
        return $true
    } catch {
        $script:ser = $null
        $script:comError = $_.Exception.Message
        return $false
    }
}

# =========================================================== Simulador (RFC2217)
function Connect-Wokwi {
    try {
        if ($script:tcp -and $script:tcp.Connected) { return $true }
        $script:tcp = New-Object System.Net.Sockets.TcpClient
        $script:tcp.Connect('127.0.0.1', $WokwiPort)
        $script:stream = $script:tcp.GetStream()
        $script:iac = 0
        $script:rx2.Clear() | Out-Null
        return $true
    } catch { $script:tcp = $null; $script:stream = $null; return $false }
}

function Remove-Iac([byte[]]$buf, [int]$n) {
    $sb = New-Object System.Text.StringBuilder
    $i = 0
    while ($i -lt $n) {
        $b = $buf[$i]
        if ($script:iac -eq 2) { if ($b -eq 0xFF) { $script:iac = 3 }; $i++; continue }
        if ($script:iac -eq 3) { $script:iac = 0; $i++; continue }
        if ($b -eq 0xFF) { $script:iac = 1; $i++; continue }
        if ($script:iac -eq 1) {
            if ($b -eq 0xFA) { $script:iac = 2 } else { $script:iac = 0 }
            $i++; continue
        }
        [void]$sb.Append([System.Text.Encoding]::GetEncoding(28591).GetString($buf, $i, 1))
        $i++
    }
    return $sb.ToString()
}

function Pump-Wokwi {
    if (-not (Connect-Wokwi)) { return }
    try {
        while ($script:stream.DataAvailable) {
            $buf = New-Object byte[] 8192
            $n = $script:stream.Read($buf, 0, $buf.Length)
            if ($n -le 0) { break }
            [void]$script:rx2.Append((Remove-Iac $buf $n))
            $todo = $script:rx2.ToString()
            $idx = $todo.LastIndexOf("`n")
            if ($idx -lt 0) { continue }
            $lineas = $todo.Substring(0, $idx) -split "`r?`n"
            $script:rx2.Clear() | Out-Null
            [void]$script:rx2.Append($todo.Substring($idx + 1))
            foreach ($l in $lineas) {
                $t = $l.Trim()
                if ($t.StartsWith('{') -and $t.EndsWith('}')) { $script:wokwiState = $t }
            }
        }
    } catch { $script:tcp = $null; $script:stream = $null }
}

function Send-Wokwi([string]$texto) {
    if (-not (Connect-Wokwi)) { return $false }
    try {
        $data = [System.Text.Encoding]::ASCII.GetBytes($texto + "`n")
        $script:stream.Write($data, 0, $data.Length)
        $script:stream.Flush()
        return $true
    } catch { $script:tcp = $null; $script:stream = $null; return $false }
}

# =========================================================== servidor web
$mime = @{
    ".html" = "text/html; charset=utf-8"; ".js" = "application/javascript; charset=utf-8"
    ".css"  = "text/css; charset=utf-8";       ".json" = "application/json; charset=utf-8"
    ".png"  = "image/png"; ".jpg" = "image/jpeg"; ".svg" = "image/svg+xml"
    ".ico"  = "image/x-icon"; ".txt" = "text/plain; charset=utf-8"
}

function Send-Json($ctx, $obj, [int]$code = 200) {
    $json = if ($obj -is [string]) { $obj } else { ($obj | ConvertTo-Json -Compress) }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $ctx.Response.StatusCode = $code
    $ctx.Response.ContentType = "application/json; charset=utf-8"
    $ctx.Response.ContentLength64 = $bytes.Length
    $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $ctx.Response.Close()
}
function Get-Src($ctx) {
    $q = $ctx.Request.QueryString['src']
    if ($q -eq 'wokwi') { return 'wokwi' }
    return 'com'
}

$listener = New-Object System.Net.HttpListener
# Escuchar en TODAS las interfaces (http://+:8765/) para que se pueda abrir
# desde el movil o desde otro equipo de la red. Ese prefijo necesita una
# reserva de URL (una sola vez, en consola de administrador):
#     netsh http add urlacl url=http://+:8765/ user=Everyone
# Si no hay reserva, se cae a http://localhost:8765/ (solo en este PC).
$redAbierta = $false
try {
    $listener.Prefixes.Add("http://+:$Port/")
    $listener.Start()
    $redAbierta = $true
} catch {
    Write-Host ("  AVISO: no se pudo abrir el puerto en la red: " + $_.Exception.Message)
    Write-Host ("  -> para poder entrar desde el movil ejecuta (admin):")
    Write-Host ("     netsh http add urlacl url=http://+:$Port/ user=Everyone")
    $listener = New-Object System.Net.HttpListener
    $listener.Prefixes.Add("http://localhost:$Port/")
    $listener.Start()
}
Write-Host ""
Write-Host "  ParkNet - servidor local"
Write-Host "  Dashboard   : http://localhost:$Port/dashboard.html"
if ($redAbierta) {
    $ipLocal = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
                Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254*' } |
                Select-Object -First 1).IPAddress
    if ($ipLocal) {
        Write-Host "  Desde el movil: http://$ipLocal`:$Port/dashboard.html"
    }
}
Write-Host "  Simulador   : localhost:$WokwiPort (RFC2217)"
Write-Host "  Arduino real: $ComPort a $Baud baud"
Write-Host "  Ctrl+C para parar"
Write-Host ""
Connect-Com | Out-Null

while ($true) {
    try {
        $ctx = $listener.GetContext()
        $path = [System.Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath)
        $src  = Get-Src $ctx

        if ($path -eq '/api/health') {
            if ($src -eq 'wokwi') {
                Pump-Wokwi
                Send-Json $ctx ([ordered]@{
                    src = 'wokwi'; conectado = [bool]$script:wokwiState
                    puerto = "localhost:$WokwiPort"; error = ''
                })
            } else {
                Pump-Com
                Send-Json $ctx ([ordered]@{
                    src = 'com'; conectado = [bool]($script:ser -and $script:ser.IsOpen)
                    puerto = $ComPort; error = $script:comError
                })
            }
            continue
        }

        if ($path -eq '/api/raw') {
            Pump-Com
            $lineas = @($script:rawLog | ForEach-Object { $_ })
            Send-Json $ctx ([ordered]@{ lineas = $lineas })
            continue
        }

        if ($path -eq '/api/state') {
            if ($src -eq 'wokwi') {
                Pump-Wokwi
                if ($script:wokwiState) { Send-Json $ctx $script:wokwiState }
                else { Send-Json $ctx '{"error":"sin estado del simulador"}' 503 }
            } else {
                Pump-Com
                if ($script:comState) { Send-Json $ctx $script:comState }
                else { Send-Json $ctx '{"error":"sin estado del Arduino; el puerto puede estar ocupado"}' 503 }
            }
            continue
        }

        if ($path -eq '/api/send') {
            $sr = New-Object System.IO.StreamReader($ctx.Request.InputStream, [System.Text.Encoding]::UTF8)
            $body = $sr.ReadToEnd(); $sr.Close()
            $ok = if ($src -eq 'wokwi') { Send-Wokwi $body } else { Send-Com $body }
            if ($ok) { Send-Json $ctx '{"ok":true}' }
            else { Send-Json $ctx ([ordered]@{ ok = $false; error = $script:comError }) 503 }
            continue
        }

        if ($path -eq '/api/open') {
            $script:comSuspendido = $false
            if ($script:ser -and $script:ser.IsOpen) {
                Send-Json $ctx '{"ok":true,"msg":"ya estaba abierto"}'
            } else {
                $ok = Connect-Com
                if ($ok) { Send-Json $ctx '{"ok":true}' }
                else { Send-Json $ctx ([ordered]@{ ok = $false; error = $script:comError }) 503 }
            }
            continue
        }

        if ($path -eq '/api/release') {
            Release-Com
            Send-Json $ctx '{"ok":true,"msg":"COM3 liberado"}'
            continue
        }

        $rel = $path.TrimStart('/')
        if ($rel -eq '' -or $rel -eq 'index.html') { $rel = 'dashboard.html' }
        $file = Join-Path $Root $rel
        if ((Test-Path -LiteralPath $file -PathType Leaf) -and
            $file.StartsWith($Root, [System.StringComparison]::OrdinalIgnoreCase)) {
            $bytes = [System.IO.File]::ReadAllBytes($file)
            $ext = [System.IO.Path]::GetExtension($file).ToLower()
            $ctx.Response.ContentType = if ($mime.ContainsKey($ext)) { $mime[$ext] } else { 'application/octet-stream' }
            $ctx.Response.ContentLength64 = $bytes.Length
            $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
        } else {
            $ctx.Response.StatusCode = 404
            $b = [System.Text.Encoding]::UTF8.GetBytes('404')
            $ctx.Response.ContentLength64 = $b.Length
            $ctx.Response.OutputStream.Write($b, 0, $b.Length)
        }
        $ctx.Response.Close()
    } catch {
        try { $ctx.Response.Abort() } catch { }
    }
}
