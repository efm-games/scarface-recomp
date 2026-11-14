param(
    [int[]]$Times = @(3, 4, 5, 7, 9, 12, 14, 18, 22),
    [int]$SkipAt = 10,
    [int]$CloseAfter = 30,
    [string]$Tag = 'baseline',
    [switch]$FfTexture,
    [switch]$MenuInputs
)

$ErrorActionPreference = 'Stop'
$root = 'C:\Users\elton\Downloads\Scarface'
$rel = Join-Path $root 'out\build\release'
$logs = Join-Path $rel 'logs'

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}

$env:XBE_VERBOSE = '1'
$env:XBE_CAPTURE_DIR = 'cap'
if ($FfTexture) { $env:SCARFACE_FF_TEXTURE = '1' } else { Remove-Item Env:SCARFACE_FF_TEXTURE -ErrorAction SilentlyContinue }

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Win {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
}
'@

function Key([IntPtr]$h, [int]$vk, [int]$ms) {
    [Win]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds $ms
    [Win]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]0) | Out-Null
}

$p = Start-Process -FilePath (Join-Path $rel 'Scarface.exe') -ArgumentList '--no-launcher' -WorkingDirectory $rel -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 200; $i++) {
    Start-Sleep -Milliseconds 100
    $p.Refresh()
    if ($p.HasExited) { throw "Scarface exited during startup (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'no game window appeared' }

$sw = [Diagnostics.Stopwatch]::StartNew()
$stamp = Get-Date -Format 'HHmmss'
Write-Host "window $hwnd, pid $($p.Id), tag $Tag-$stamp"
# the keyboard pad fallback only feeds input while the window is active
[Win]::ShowWindow($hwnd, 5) | Out-Null
[Win]::SetForegroundWindow($hwnd) | Out-Null

# one event list, so captures, the movie skip and the menu keys happen in time order
$events = @()
foreach ($t in $Times) { $events += [pscustomobject]@{ When = $t; What = 'cap'; Vk = 0 } }
if ($SkipAt -gt 0) { $events += [pscustomobject]@{ When = $SkipAt; What = 'skip'; Vk = 0x0D } }
if ($MenuInputs) {
    $keys = @([int][char]'W', [int][char]'S', [int][char]'D', [int][char]'A')
    for ($i = 0; $i -lt $keys.Count; $i++) {
        $events += [pscustomobject]@{ When = $SkipAt + 4 + $i; What = 'key'; Vk = $keys[$i] }
    }
}

foreach ($e in ($events | Sort-Object When)) {
    $remain = $e.When * 1000 - $sw.ElapsedMilliseconds
    if ($remain -gt 0) { Start-Sleep -Milliseconds $remain }
    if ($e.What -eq 'cap') {
        Key $hwnd 0x72 80
        Write-Host ("F3 at {0:N1}s" -f $sw.Elapsed.TotalSeconds)
    } elseif ($e.What -eq 'skip') {
        Key $hwnd $e.Vk 200
        Write-Host ("skip (A) at {0:N1}s" -f $sw.Elapsed.TotalSeconds)
    } else {
        Key $hwnd $e.Vk 300
        Write-Host ("key {0} at {1:N1}s" -f [char]$e.Vk, $sw.Elapsed.TotalSeconds)
    }
}

$remain = $CloseAfter * 1000 - $sw.ElapsedMilliseconds
if ($remain -gt 0) { Start-Sleep -Milliseconds $remain }
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Start-Sleep -Milliseconds 800

$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\run_$Tag-$stamp.log") -Force
Write-Host "log dev\run_$Tag-$stamp.log (from $($newest.Name))"
