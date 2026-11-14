param(
    [string]$Tag = 'launcher',
    [int]$Timeout = 25,
    [int]$GameTimeout = 300,
    [switch]$Play
)

# Runs the launcher alone and checks it: the window opens with the chosen image, the backbuffer is written to
# dev\<Tag>.bmp (screen grabs are unreliable on this machine), and the process exits cleanly when closed.
# -Play also presses Play from inside the launcher: the game window must appear and settings.toml keep the image.
# The first start after a runtime rebuild relinks Scarface.dll behind a ScarfaceGameCode window, so this waits for it.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\elton\Downloads\Scarface'
$rel = Join-Path $root 'out\build\release'
$logs = Join-Path $rel 'logs'
$iso = Join-Path $root 'Scarface - The World Is Yours (USA, Europe).xiso.iso'
$shot = Join-Path $root "dev\$Tag.bmp"
$settings = Join-Path $rel 'settings.toml'

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}
if (-not (Test-Path $iso)) { throw "missing $iso" }

Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class L {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
}
'@

function ClassOf([IntPtr]$h) {
    $text = New-Object System.Text.StringBuilder 256
    [void][L]::GetClassName($h, $text, 256)
    $text.ToString()
}

Remove-Item $shot -ErrorAction SilentlyContinue
$env:XBE_VERBOSE = '1'
$env:XBE_LAUNCHER_SHOT = $shot
Remove-Item Env:XBE_CAPTURE_DIR -ErrorAction SilentlyContinue
$env:XBE_LAUNCHER_PLAY = if ($Play) { '1' } else { $null }

$p = Start-Process -FilePath (Join-Path $rel 'Scarface.exe') -ArgumentList ('--game "' + $iso + '"') -WorkingDirectory $rel -PassThru

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt $Timeout * 10; $i++) {
    Start-Sleep -Milliseconds 100
    $p.Refresh()
    if ($p.HasExited) { throw "Scarface exited during startup (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'no launcher window appeared' }
Write-Host "launcher window $hwnd, class $(ClassOf $hwnd)"

for ($i = 0; $i -lt 100 -and -not (Test-Path $shot); $i++) { Start-Sleep -Milliseconds 100 }
if (-not (Test-Path $shot)) { throw 'the launcher did not write its picture' }
Write-Host "picture dev\$Tag.bmp"

if ($Play) {
    # Play was pressed from inside the launcher; the game's own window follows the rebuild window, if one shows
    $game = [IntPtr]::Zero
    $seen = ''
    for ($i = 0; $i -lt $GameTimeout * 10; $i++) {
        Start-Sleep -Milliseconds 100
        $p.Refresh()
        if ($p.HasExited) { throw "Scarface exited after Play (code $($p.ExitCode)); see the log" }
        if ($p.MainWindowHandle -eq [IntPtr]::Zero) { continue }
        $class = ClassOf $p.MainWindowHandle
        if ($class -ne $seen) { $seen = $class; Write-Host "window class $class" }
        if ($class -eq 'Scarface') { $game = $p.MainWindowHandle; break }
    }
    if ($game -eq [IntPtr]::Zero) { throw 'the game window never appeared' }
    Write-Host "game window $game"
    $after = Get-Content $settings -Raw
    if ($after -notmatch "(?m)^game\s*=") { throw 'settings.toml was not given the image path' }
    $keep = Select-String -Path $settings -Pattern '^(game|launcher)\s*=' | ForEach-Object { $_.Line.Trim() }
    Write-Host ('settings.toml: ' + ($keep -join '; '))
    Start-Sleep -Seconds 6
    [void][L]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)
} else {
    [void][L]::PostMessage($hwnd, 0x0010, [IntPtr]0, [IntPtr]0)
}

if (-not $p.WaitForExit(20000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }
Write-Host "exit code $($p.ExitCode)"
Remove-Item Env:XBE_LAUNCHER_SHOT -ErrorAction SilentlyContinue
Remove-Item Env:XBE_LAUNCHER_PLAY -ErrorAction SilentlyContinue

$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\launcher_$Tag.log") -Force
Write-Host "log dev\launcher_$Tag.log (from $($newest.Name))"
Select-String -Path (Join-Path $root "dev\launcher_$Tag.log") -Pattern 'Launcher:|Game code|input:|boot|Exit' |
    ForEach-Object { $_.Line } | Select-Object -First 20
