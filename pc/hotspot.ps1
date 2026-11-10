# encender-hotspot.cmd - companion script (lo llama el .cmd, no correr directo)
# Prende el hotspot de Windows y lo deja con el nombre/clave que espera ParkNet.
$ErrorActionPreference = "SilentlyContinue"
$null = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType = WindowsRuntime]
$mgr = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager,Windows.Networking.NetworkOperators,ContentType=WindowsRuntime]::CreateFromConnectionProfile([Windows.Networking.Connectivity.NetworkInformation,Windows.Networking.Connectivity,ContentType=WindowsRuntime]::GetInternetConnectionProfile())
if (-not $mgr) { Write-Output "SIN PERFIL DE INTERNET en este PC."; exit 1 }
$cfg = $mgr.GetCurrentAccessPointConfiguration()
# solo cambia config si no es la de ParkNet
if ($cfg.Ssid -ne "HONOR ola" -or $cfg.Passphrase -ne "quesesoo") {
  $cfg.Ssid = "HONOR ola"
  $cfg.Passphrase = "quesesoo"
  $cfg.Band = [Windows.Networking.NetworkOperators.TetheringWiFiBand]::TwoPointFourGigahertz
  $mgr.ConfigureAccessPointAsync($cfg) | Out-Null
  Start-Sleep -Seconds 2
}
if ($mgr.TetheringOperationalState -ne [Windows.Networking.NetworkOperators.TetheringOperationalState]::On) {
  $mgr.StartTetheringAsync() | Out-Null
  Start-Sleep -Seconds 5
}
$check = $mgr.GetCurrentAccessPointConfiguration()
Write-Output ("Hotspot: SSID='" + $check.Ssid + "'  estado=" + $mgr.TetheringOperationalState + "  clientes=" + $mgr.ClientCount)

# descubrir la IP de la placa ParkNet en la red del hotspot (tabla ARP)
Start-Sleep -Seconds 2
ping 192.168.137.255 -n 1 -w 700 | Out-Null
$ipPlaca = (arp -a | Select-String "192.168.137." | Where-Object { $_.Line -notmatch "192\.168\.137\.(1|255) " } |
  ForEach-Object { ($_.Line.Trim() -split "\s+")[0] }) | Where-Object { $_ -like "192.168.137.*" } | Select-Object -First 1
if ($ipPlaca) {
  Write-Output ""
  Write-Output ("PAGINA DE ParkNet (abrir en el telefono):  http://" + $ipPlaca)
  Write-Output ("Abriendola tambien en el navegador del PC...")
  Start-Process ("http://" + $ipPlaca)
} else {
  Write-Output "Placa aun no visible en la red (espera ~30 s tras enchufarla a la corriente y re-ejecute)."
}
