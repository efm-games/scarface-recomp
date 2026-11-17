param(
    [string]$Tag = 'pausecap',
    [int]$LoadWait = 55,
    [int]$PauseKey = 0x78
)

# Loads the existing save, starts the game, opens the pause menu and captures that frame with the renderer's own
# F3 capture (final.bmp is the real back buffer, unlike PrintWindow). Control captures at boot and in play prove
# that the key and the capture path work before the pause one.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$rel = Join-Path $root 'out\build\release'
$logs = Join-Path $rel 'logs'
$cap = Join-Path $rel $Tag

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}

Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class W3 {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
}
'@

function ClassOf([IntPtr]$h) {
    $text = New-Object System.Text.StringBuilder 256
    [void][W3]::GetClassName($h, $text, 256)
    $text.ToString()
}
function Tap([IntPtr]$h, [int]$vk, [int]$ms = 120) {
    [void][W3]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]1)
    Start-Sleep -Milliseconds $ms
    [void][W3]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]1)
    Start-Sleep -Milliseconds 400
}
$t0 = Get-Date
function Mark($text) { Write-Host ("[{0,6:N1}s] {1}" -f ((Get-Date) - $t0).TotalSeconds, $text) }
function Captures { @(Get-ChildItem $cap -Directory -ErrorAction SilentlyContinue).Count }

$env:XBE_VERBOSE = '1'
$env:XBE_CAPTURE_DIR = $Tag
Remove-Item $cap -Recurse -Force -ErrorAction SilentlyContinue

$p = Start-Process -FilePath (Join-Path $rel 'Scarface.exe') -ArgumentList '--no-launcher' -WorkingDirectory $rel -PassThru
$game = [IntPtr]::Zero
for ($i = 0; $i -lt 600; $i++) {
    Start-Sleep -Milliseconds 100
    $p.Refresh()
    if ($p.HasExited) { throw "Scarface exited before the game window (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -eq [IntPtr]::Zero) { continue }
    if ((ClassOf $p.MainWindowHandle) -eq 'Scarface') { $game = $p.MainWindowHandle; break }
}
if ($game -eq [IntPtr]::Zero) { throw 'the game window never appeared' }
[void][W3]::ShowWindow($game, 5)
[void][W3]::SetForegroundWindow($game)
try { [Microsoft.VisualBasic.Interaction]::AppActivate($p.Id) | Out-Null } catch { Mark "AppActivate failed: $_" }
Mark ("game window {0}, foreground {1}" -f $game, ([W3]::GetForegroundWindow() -eq $game))

Start-Sleep -Seconds 8
Mark 'capture 1: boot screen'
Tap $game 0x72
Start-Sleep -Seconds 5
Mark ("captures now: {0}" -f (Captures))

Tap $game 0x0D          # dismiss the profile load confirmation
Start-Sleep -Seconds 4
Tap $game 0x0D          # start / continue the saved game
Mark 'waiting for the saved game'
Start-Sleep -Seconds $LoadWait

Mark 'capture 2: gameplay'
Tap $game 0x72
Start-Sleep -Seconds 5
Mark ("captures now: {0}" -f (Captures))

Mark 'opening the pause menu'
Tap $game $PauseKey     # Start button only, no cancel
Start-Sleep -Seconds 3
Mark ("game alive after pause key: {0}" -f (-not $p.HasExited))
Mark 'capture 3: pause menu'
Tap $game 0x72
Start-Sleep -Seconds 6
Mark ("captures now: {0}" -f (Captures))
if (-not $p.HasExited) {
    Mark 'capture 4: pause menu, settled'
    Tap $game 0x72
    Start-Sleep -Seconds 6
    Mark ("captures now: {0}" -f (Captures))
}

[void][W3]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)
if (-not $p.WaitForExit(60000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }

Remove-Item Env:XBE_VERBOSE -ErrorAction SilentlyContinue
Remove-Item Env:XBE_CAPTURE_DIR -ErrorAction SilentlyContinue
$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\run_$Tag.log") -Force
Write-Host "log dev\run_$Tag.log"
Get-ChildItem $cap -Directory | ForEach-Object { "$($_.Name): $((Get-ChildItem $_.FullName -File).Count) files" }
