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
Write-Output ("Panel del Arduino en esta red:  http://192.168.137.80  (si no responde, probar la IP en la lista de clientes)")
