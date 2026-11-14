param(
    [string]$Tag = 'click',
    [int]$BootWait = 8,
    [int]$GameTimeout = 300
)

# Starts the game through the launcher, opens the F1 overlay, clicks the Controls tab, toggles the game's "Keyboard
# and mouse" option off, saves, reopens, toggles it back on, saves again, and checks settings.toml both times.
# This also tests that F1 still works while the game's keyboard layer is disabled.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\elton\Downloads\Scarface'
$rel = Join-Path $root 'out\build\release'
$logs = Join-Path $rel 'logs'
$iso = Join-Path $root 'Scarface - The World Is Yours (USA, Europe).xiso.iso'
$settings = Join-Path $rel 'settings.toml'

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}
if (-not (Test-Path $iso)) { throw "missing $iso" }

Add-Type @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
public static class W {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct BITMAPINFOHEADER {
        public uint biSize; public int biWidth; public int biHeight; public ushort biPlanes; public ushort biBitCount;
        public uint biCompression; public uint biSizeImage; public int biXPelsPerMeter; public int biYPelsPerMeter;
        public uint biClrUsed; public uint biClrImportant;
    }
    [StructLayout(LayoutKind.Sequential)] public struct BITMAPINFO { public BITMAPINFOHEADER h; public uint colors; }
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h, IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int w, int h);
    [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern int GetDIBits(IntPtr dc, IntPtr bmp, uint start, uint lines, byte[] bits,
                                                         ref BITMAPINFO bmi, uint usage);
    public static string Shot(IntPtr h, string path) {
        RECT r;
        if (!GetWindowRect(h, out r)) return "no rect";
        int w = r.Right - r.Left, hh = r.Bottom - r.Top;
        IntPtr screen = GetDC(IntPtr.Zero), mem = CreateCompatibleDC(screen);
        IntPtr bmp = CreateCompatibleBitmap(screen, w, hh), old = SelectObject(mem, bmp);
        bool ok = PrintWindow(h, mem, 2);
        var bi = new BITMAPINFO();
        bi.h.biSize = 40; bi.h.biWidth = w; bi.h.biHeight = -hh; bi.h.biPlanes = 1; bi.h.biBitCount = 32;
        bi.h.biCompression = 0; bi.h.biSizeImage = (uint)(w * hh * 4);
        byte[] pixels = new byte[w * hh * 4];
        int lines = GetDIBits(mem, bmp, 0, (uint)hh, pixels, ref bi, 0);
        using (var f = new BinaryWriter(File.Create(path))) {
            f.Write((byte)'B'); f.Write((byte)'M');
            f.Write(54 + pixels.Length); f.Write(0); f.Write(54);
            f.Write(40); f.Write(w); f.Write(-hh); f.Write((short)1); f.Write((short)32);
            f.Write(0); f.Write(pixels.Length); f.Write(0); f.Write(0); f.Write(0); f.Write(0);
            f.Write(pixels);
        }
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(IntPtr.Zero, screen);
        return (ok ? "ok " : "failed ") + w + "x" + hh + " lines " + lines;
    }
}
'@

function ClassOf([IntPtr]$h) {
    $text = New-Object System.Text.StringBuilder 256
    [void][W]::GetClassName($h, $text, 256)
    $text.ToString()
}
function Key([IntPtr]$h, [int]$vk) {
    [void][W]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]1)
    [void][W]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]0xC0000001)
}
function Click([IntPtr]$h, [int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [void][W]::PostMessage($h, 0x0200, [IntPtr]0, $lp)  # WM_MOUSEMOVE so ImGui hovers the item first
    Start-Sleep -Milliseconds 150
    [void][W]::PostMessage($h, 0x0201, [IntPtr]1, $lp)  # WM_LBUTTONDOWN
    Start-Sleep -Milliseconds 150
    [void][W]::PostMessage($h, 0x0202, [IntPtr]0, $lp)  # WM_LBUTTONUP
    Start-Sleep -Milliseconds 400
}
function Kbm() {
    $line = Select-String -Path $settings -Pattern '^kbm\s*=' | Select-Object -First 1
    if (-not $line) { return '?' }
    $line.Line.Trim()
}

Remove-Item (Join-Path $root "dev\$Tag`_*.bmp") -ErrorAction SilentlyContinue
$env:XBE_VERBOSE = '1'
$env:XBE_LAUNCHER_PLAY = '1'
$env:XBE_LAUNCHER_SHOT = Join-Path $root "dev\$Tag`_launcher.bmp"
Remove-Item Env:XBE_CAPTURE_DIR -ErrorAction SilentlyContinue

$p = Start-Process -FilePath (Join-Path $rel 'Scarface.exe') -ArgumentList ('--game "' + $iso + '"') -WorkingDirectory $rel -PassThru

$game = [IntPtr]::Zero
$seen = ''
for ($i = 0; $i -lt $GameTimeout * 10; $i++) {
    Start-Sleep -Milliseconds 100
    $p.Refresh()
    if ($p.HasExited) { throw "Scarface exited before the game window (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -eq [IntPtr]::Zero) { continue }
    $class = ClassOf $p.MainWindowHandle
    if ($class -ne $seen) { $seen = $class; Write-Host "window class $class" }
    if ($class -eq 'Scarface') { $game = $p.MainWindowHandle; break }
}
if ($game -eq [IntPtr]::Zero) { throw 'the game window never appeared' }
Start-Sleep -Seconds $BootWait

# the overlay panel is 720x540 logical pixels, centred
$rect = New-Object W+RECT
[void][W]::GetWindowRect($game, [ref]$rect)
$dpi = [W]::GetDpiForWindow($game)
$unit = $dpi / 96.0
$w = [int](720 * $unit)
$h = [int](540 * $unit)
$left = [int](($rect.Right - $rect.Left - $w) / 2)
$top = [int](($rect.Bottom - $rect.Top - $h) / 2)
Write-Host "game $($rect.Right - $rect.Left)x$($rect.Bottom - $rect.Top), dpi $dpi, panel ${w}x${h} at $left,$top"

$controlsTab = @([int]($left + 0.15 * $w), [int]($top + 0.204 * $h))
$firstCheck = @([int]($left + 0.05 * $w), [int]($top + 0.252 * $h))
$saveButton = @([int]($left + $w - 102 * $unit), [int]($top + $h - 28 * $unit))

Key $game 0x70  # F1
Start-Sleep -Seconds 1
Write-Host ('display:  ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_display.bmp")))
Click $game $controlsTab[0] $controlsTab[1]
Write-Host ('controls: ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_controls.bmp")))
Click $game $firstCheck[0] $firstCheck[1]
Write-Host ('toggled:  ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_toggled.bmp")))
Click $game $saveButton[0] $saveButton[1]
Start-Sleep -Seconds 1
Write-Host ('after first save: ' + (Kbm))
if ((Kbm) -ne 'kbm = false') { throw "the overlay did not save kbm = false (got '$((Kbm))')" }

Key $game 0x70  # F1 again, with the keyboard layer off
Start-Sleep -Seconds 1
Click $game $controlsTab[0] $controlsTab[1]
Click $game $firstCheck[0] $firstCheck[1]
Click $game $saveButton[0] $saveButton[1]
Start-Sleep -Seconds 1
Write-Host ('after second save: ' + (Kbm))
if ((Kbm) -ne 'kbm = true') { throw "the overlay did not save kbm = true (got '$((Kbm))')" }

[void][W]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }
Write-Host "exit code $($p.ExitCode)"
Remove-Item Env:XBE_LAUNCHER_PLAY -ErrorAction SilentlyContinue
Remove-Item Env:XBE_LAUNCHER_SHOT -ErrorAction SilentlyContinue

$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\$Tag.log") -Force
Write-Host "log dev\$Tag.log (from $($newest.Name))"
Select-String -Path (Join-Path $root "dev\$Tag.log") -Pattern 'Overlay|Game code|boot: title|Exit' |
    ForEach-Object { $_.Line } | Select-Object -First 25
