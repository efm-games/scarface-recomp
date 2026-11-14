param(
    [string]$Tag = 'saves',
    [int]$BootWait = 8,
    [int]$GameTimeout = 300,
    [switch]$Delete
)

# Starts the game through the launcher, opens the F1 overlay, clicks the Saves tab, pictures it, then closes. Used to
# check the save manager's layout and listing; the imported test save must exist before the run.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\elton\Downloads\Scarface'
$rel = Join-Path $root 'out\build\release'
$logs = Join-Path $rel 'logs'
$iso = Join-Path $root 'Scarface - The World Is Yours (USA, Europe).xiso.iso'
$testSave = Join-Path $rel 'hdd\E\UDATA\5655003f\ABCDEF123456'

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}
if (-not (Test-Path $iso)) { throw "missing $iso" }
if ($Delete -and -not (Test-Path $testSave)) { throw "missing the imported test save $testSave" }

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
    [void][W]::PostMessage($h, 0x0200, [IntPtr]0, $lp)
    Start-Sleep -Milliseconds 150
    [void][W]::PostMessage($h, 0x0201, [IntPtr]1, $lp)
    Start-Sleep -Milliseconds 150
    [void][W]::PostMessage($h, 0x0202, [IntPtr]0, $lp)
    Start-Sleep -Milliseconds 400
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

$rect = New-Object W+RECT
[void][W]::GetWindowRect($game, [ref]$rect)
$unit = [W]::GetDpiForWindow($game) / 96.0
$w = [int](720 * $unit)
$h = [int](540 * $unit)
$left = [int](($rect.Right - $rect.Left - $w) / 2)
$top = [int](($rect.Bottom - $rect.Top - $h) / 2)
Write-Host "game $($rect.Right - $rect.Left)x$($rect.Bottom - $rect.Top), panel ${w}x${h} at $left,$top"

Key $game 0x70  # F1
Start-Sleep -Seconds 1
Click $game ([int]($left + 0.26 * $w)) ([int]($top + 0.182 * $h))
Click $game ([int]($left + 0.26 * $w)) ([int]($top + 0.182 * $h))
Write-Host ('saves tab: ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_tab.bmp")))
if ($Delete) {
    Click $game ([int]($left + 0.308 * $w)) ([int]($top + 0.583 * $h))
    Write-Host ('delete dialog: ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_delete.bmp")))
    Click $game ([int]($left + 0.40 * $w)) ([int]($top + 0.563 * $h))
    Start-Sleep -Milliseconds 500
    Write-Host ('deleted: ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_deleted.bmp")))
    if (Test-Path $testSave) { throw "the delete button did not remove $testSave" }
    Write-Host 'the test save folder is gone'
}

Key $game 0x1B  # Esc
Start-Sleep -Milliseconds 700
[void][W]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }
Write-Host "exit code $($p.ExitCode)"
Remove-Item Env:XBE_LAUNCHER_PLAY -ErrorAction SilentlyContinue
Remove-Item Env:XBE_LAUNCHER_SHOT -ErrorAction SilentlyContinue

$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\$Tag.log") -Force
Write-Host "log dev\$Tag.log (from $($newest.Name))"
Select-String -Path (Join-Path $root "dev\$Tag.log") -Pattern 'Saves:|Overlay|Exit' | ForEach-Object { $_.Line } | Select-Object -First 20
