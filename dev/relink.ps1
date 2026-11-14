# Rebuilds the generated code DLL after the executable changed, without a human at the keyboard.
# The game shows its progress window and waits for Enter on the ready page; this sends Enter until the
# DLL is loaded, then waits a moment and closes the game. Run it after changing src\game, before a capture run.
param(
    [int]$TimeoutSeconds = 180,
    [int]$AfterSeconds = 5
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$rel = Join-Path $root 'out\build\release'
$exe = Join-Path $rel 'Scarface.exe'
$dll = Join-Path $rel 'Scarface.dll'

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class RelinkWin {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
'@

$before = if (Test-Path $dll) { (Get-Item $dll).LastWriteTimeUtc } else { [datetime]::MinValue }
$p = Start-Process -FilePath $exe -ArgumentList '--no-launcher' -WorkingDirectory $rel -PassThru
$sw = [Diagnostics.Stopwatch]::StartNew()
$loaded = $false
try {
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Milliseconds 500
        $p.Refresh()
        if ($p.HasExited) { break }
        try { $null = $p.Modules | Where-Object { $_.ModuleName -eq 'Scarface.dll' } | Select-Object -First 1 -OutVariable dllModule } catch { }
        if ($dllModule) { $loaded = $true; break }
        if ($p.MainWindowHandle -ne [IntPtr]::Zero) {
            [RelinkWin]::PostMessage($p.MainWindowHandle, 0x0100, [IntPtr]0x0D, [IntPtr]0) | Out-Null
            [RelinkWin]::PostMessage($p.MainWindowHandle, 0x0101, [IntPtr]0x0D, [IntPtr]0) | Out-Null
        }
    }
    if ($loaded) { Start-Sleep -Seconds $AfterSeconds }
} finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    Start-Sleep -Milliseconds 500
}

$after = if (Test-Path $dll) { (Get-Item $dll).LastWriteTimeUtc } else { [datetime]::MinValue }
if ($after -gt $before) {
    Write-Host ("Scarface.dll relinked at {0:HH:mm:ss} after {1:N1}s" -f $after.ToLocalTime(), $sw.Elapsed.TotalSeconds)
} elseif ($loaded) {
    Write-Host 'Scarface.dll was already current'
} else {
    throw "the game did not load Scarface.dll within $TimeoutSeconds seconds"
}
