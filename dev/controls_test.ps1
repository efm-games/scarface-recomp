param(
    [string]$Tag = 'controls',
    [int]$MenuWait = 8,
    [switch]$Drive,
    [switch]$Observe,
    [switch]$Probe
)

# Boots straight to the menu (the development settings skip the logos), then sends keyboard input only: the arrow
# keys must move the Xbox menu selection and Enter must select, with a pad still connected. Screenshots are taken
# through PrintWindow so they can be inspected without reading the desktop.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\elton\Downloads\Scarface'
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
    [DllImport("gdi32.dll")] static extern int GetDIBits(IntPtr dc, IntPtr bmp, uint start, uint lines, byte[] bits,
                                                         ref BITMAPINFO bmi, uint usage);
    public static string Shot(IntPtr h, string path) {
        RECT r;
        if (!GetWindowRect(h, out r)) return "no rect";
        int w = r.Right - r.Left, hh = r.Bottom - r.Top;
        IntPtr screen = GetDC(IntPtr.Zero), mem = CreateCompatibleDC(screen);
        IntPtr bmp = CreateCompatibleBitmap(screen, w, hh), old = SelectObject(mem, bmp);
        bool ok = PrintWindow(h, mem, 2);  // PW_RENDERFULLCONTENT
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
Shot $game 'boot0'

# the profile auto-loads from the emulated hard disk and leaves a confirmation up; Enter closes it
Tap $game 0x0D 120
Start-Sleep -Seconds 4
Shot $game 'menu0'

# arrow keys stand in for the pad's d-pad: Down twice must move the selection
Tap $game 0x28
Shot $game 'menu1'
Tap $game 0x28
Shot $game 'menu2'

if ($Drive) {
    # W + Enter: steering plus gas, as the PC version binds them
    [void][W]::PostMessage($game, 0x0100, [IntPtr]0x57, [IntPtr]1)
    [void][W]::PostMessage($game, 0x0100, [IntPtr]0x0D, [IntPtr]1)
    Start-Sleep -Seconds 3
    Shot $game 'drive'
    [void][W]::PostMessage($game, 0x0101, [IntPtr]0x0D, [IntPtr]1)
    [void][W]::PostMessage($game, 0x0101, [IntPtr]0x57, [IntPtr]1)
}

# enter opens whatever the selection is on
Tap $game 0x0D 120
Start-Sleep -Seconds 2
Shot $game 'selected'
Start-Sleep -Seconds 8
Shot $game 'settled'

if ($Observe) {
    foreach ($step in @(@(3, 't3'), @(6, 't6'), @(10, 't10'))) {
        Start-Sleep -Seconds $step[0]
        Shot $game $step[1]
    }
}

if ($Probe) {
    # idle frames first, then W alone, then Enter alone: the frame differences show which key moves the scene
    Start-Sleep -Seconds 6
    for ($i = 0; $i -lt 3; $i++) {
        Shot $game ("idle{0}" -f $i)
        Start-Sleep -Seconds 1
    }
    [void][W]::PostMessage($game, 0x0100, [IntPtr]0x57, [IntPtr]1)
    for ($i = 0; $i -lt 3; $i++) {
        Start-Sleep -Seconds 1
        Shot $game ("w{0}" -f $i)
    }
    [void][W]::PostMessage($game, 0x0101, [IntPtr]0x57, [IntPtr]1)
    Start-Sleep -Seconds 2
    Shot $game 'wstop'
    [void][W]::PostMessage($game, 0x0100, [IntPtr]0x0D, [IntPtr]1)
    for ($i = 0; $i -lt 2; $i++) {
        Start-Sleep -Seconds 1
        Shot $game ("enter{0}" -f $i)
    }
    [void][W]::PostMessage($game, 0x0101, [IntPtr]0x0D, [IntPtr]1)
    Start-Sleep -Seconds 2
    Shot $game 'enterstop'
}

if ($Keys) {
    # hold each key in turn and picture the result: Enter gas, W steer, R brake/reverse, T handbrake
    foreach ($step in @(@(0x0D, 3, 'gas'), @(0x57, 2, 'steer'), @(0x52, 2, 'brake'), @(0x54, 2, 'handbrake'))) {
        [void][W]::PostMessage($game, 0x0100, [IntPtr]$step[0], [IntPtr]1)
        Start-Sleep -Seconds $step[1]
        Shot $game $step[2]
        [void][W]::PostMessage($game, 0x0101, [IntPtr]$step[0], [IntPtr]1)
        Start-Sleep -Milliseconds 700
    }
    Tap $game 0x1B 100
    Start-Sleep -Seconds 2
    Shot $game 'pause'
}

[void][W]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)  # WM_CLOSE
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }

Remove-Item Env:XBE_VERBOSE -ErrorAction SilentlyContinue
$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\run_$Tag.log") -Force
Write-Host "log dev\run_$Tag.log"
Select-String -Path (Join-Path $root "dev\run_$Tag.log") -Pattern 'skip_intro|input:' | ForEach-Object { $_.Line } |
    Select-Object -First 20
