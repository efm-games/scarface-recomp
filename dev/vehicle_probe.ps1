param(
    [string]$Tag = 'veh',
    [int]$LoadWait = 35
)

# Loads the existing save (it starts in the boat tutorial), then holds one key at a time and pictures the result.
# W / S / Enter / Space (pad B) are compared by frame difference and by OCR, so we learn which one really moves or
# stops the vehicle. Afterwards Esc opens the pause menu, where the game's own controller screen can be read.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$rel = Join-Path $root 'out\build\release'
$logs = Join-Path $rel 'logs'

if (Get-Process Scarface -ErrorAction SilentlyContinue) {
    throw 'Scarface already running; refusing to start a second instance'
}

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
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h, IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int w, int h);
    [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
    [DllImport("gdi32.dll")] public static extern int GetDIBits(IntPtr dc, IntPtr bmp, uint start, uint lines, byte[] bits,
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
function Tap([IntPtr]$h, [int]$vk, [int]$ms = 90) {
    [void][W]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]1)
    Start-Sleep -Milliseconds $ms
    [void][W]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]1)
    Start-Sleep -Milliseconds 350
}
function Down([IntPtr]$h, [int]$vk) { [void][W]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]1) }
function Up([IntPtr]$h, [int]$vk) { [void][W]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]1) }
function Shot([IntPtr]$h, [string]$name) {
    $path = Join-Path $root "dev\$Tag`_$name.bmp"
    Write-Host ("{0}: {1}" -f $name, [W]::Shot($h, $path))
}

$env:XBE_VERBOSE = '1'
Remove-Item Env:XBE_CAPTURE_DIR -ErrorAction SilentlyContinue

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
[void][W]::ShowWindow($game, 5)
[void][W]::SetForegroundWindow($game)
Write-Host "game window $game"

Start-Sleep -Seconds 6
Shot $game 'boot'
Tap $game 0x0D 120          # dismiss the profile load confirmation
Start-Sleep -Seconds 4
Shot $game 'menu'
Tap $game 0x0D 120          # start / continue the saved game
Start-Sleep -Seconds $LoadWait
Shot $game 'play0'

# one key at a time, each held for 2.5 s, with a picture before and after so the frame difference shows motion
$steps = @(@(0x57, 'w'), @(0x53, 's'), @(0x0D, 'enter'), @(0x54, 't'), @(0x52, 'r'), @(0x20, 'space'))
foreach ($step in $steps) {
    Shot $game ("{0}_a" -f $step[1])
    Down $game $step[0]
    Start-Sleep -Milliseconds 2500
    Shot $game ("{0}_b" -f $step[1])
    Up $game $step[0]
    Start-Sleep -Milliseconds 1500
    Shot $game ("{0}_c" -f $step[1])
}

Tap $game 0x1B 120          # Esc: pause menu
Start-Sleep -Seconds 2
Shot $game 'pause'

[void][W]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }

Remove-Item Env:XBE_VERBOSE -ErrorAction SilentlyContinue
$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\run_$Tag.log") -Force
Write-Host "log dev\run_$Tag.log"
