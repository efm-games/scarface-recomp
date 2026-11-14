param([string]$Tag = 'diag')

$ErrorActionPreference = 'Stop'
$root = 'C:\Users\elton\Downloads\Scarface'
$rel = Join-Path $root 'out\build\release'
$iso = Join-Path $root 'Scarface - The World Is Yours (USA, Europe).xiso.iso'
if (Get-Process Scarface -ErrorAction SilentlyContinue) { throw 'Scarface already running' }

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class D {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    public struct RECT { public int left, top, right, bottom; }
    public struct POINT { public int x, y; }
}
'@

function Capture([int]$x, [int]$y, [int]$w, [int]$h, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $bmp.Dispose()
    Write-Host "captured $w x $h at $x,$y to $path"
}

$env:XBE_VERBOSE = '1'
$p = Start-Process -FilePath (Join-Path $rel 'Scarface.exe') -ArgumentList ('--game "' + $iso + '"') -WorkingDirectory $rel -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 200; $i++) {
    Start-Sleep -Milliseconds 100
    $p.Refresh()
    if ($p.HasExited) { throw "exited during startup ($($p.ExitCode))" }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
[void][D]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 2500

$wr = New-Object D+RECT
[void][D]::GetWindowRect($hwnd, [ref]$wr)
$cr = New-Object D+RECT
[void][D]::GetClientRect($hwnd, [ref]$cr)
$pt = New-Object D+POINT
[void][D]::ClientToScreen($hwnd, [ref]$pt)
Write-Host "window $($wr.left),$($wr.top) $($wr.right - $wr.left)x$($wr.bottom - $wr.top); client at $($pt.x),$($pt.y) $($cr.right)x$($cr.bottom)"

# whole desktop, then the client area, then the same after moving the window to 0,0
Capture 0 0 1600 900 (Join-Path $root "dev\${Tag}_desktop.bmp")
Capture $pt.x $pt.y $cr.right $cr.bottom (Join-Path $root "dev\${Tag}_client.bmp")

[void][D]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 0, 0, 0x0001 -bor 0x0004 -bor 0x0010)
Start-Sleep -Milliseconds 800
[void][D]::GetWindowRect($hwnd, [ref]$wr)
Write-Host "moved window to $($wr.left),$($wr.top)"
Capture 0 0 1600 900 (Join-Path $root "dev\${Tag}_moved_desktop.bmp")
Capture $pt.x $pt.y $cr.right $cr.bottom (Join-Path $root "dev\${Tag}_moved_client.bmp")

# the window's own copy, which does not depend on what is on screen
$bmp = New-Object System.Drawing.Bitmap($cr.right, $cr.bottom)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][D]::PrintWindow($hwnd, $hdc, 2)
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save((Join-Path $root "dev\${Tag}_printwindow.bmp"), [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()
Write-Host "printwindow saved"

[void][D]::PostMessage($hwnd, 0x0010, [IntPtr]0, [IntPtr]0)
if (-not $p.WaitForExit(8000)) { Stop-Process -Id $p.Id -Force }
