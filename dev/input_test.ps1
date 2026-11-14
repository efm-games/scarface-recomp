param(
    [string]$Tag = 'inputtest',
    [int]$MenuWait = 30,
    [int]$GameTimeout = 300
)

# Starts the game through the launcher, waits for the menu, pictures it (the fps counter is in the corner), sends a
# keyboard press while the controller is connected, then opens and closes the F1 overlay. Prints the log lines that
# show the intro skip, the merged keyboard input and the overlay.
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
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
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

function Key([IntPtr]$h, [int]$vk, [bool]$up) {
    [void][W]::PostMessage($h, $(if ($up) { 0x0101 } else { 0x0100 }), [IntPtr]$vk, [IntPtr]1)
}

Remove-Item (Join-Path $root "dev\$Tag`_*.bmp") -ErrorAction SilentlyContinue
$env:XBE_VERBOSE = '1'
$env:XBE_LAUNCHER_PLAY = '1'
Remove-Item Env:XBE_LAUNCHER_SHOT -ErrorAction SilentlyContinue
Remove-Item Env:XBE_CAPTURE_DIR -ErrorAction SilentlyContinue

$p = Start-Process -FilePath (Join-Path $rel 'Scarface.exe') -ArgumentList ('--game "' + $iso + '"') -WorkingDirectory $rel -PassThru

$game = [IntPtr]::Zero
for ($i = 0; $i -lt $GameTimeout * 10; $i++) {
    Start-Sleep -Milliseconds 100
    $p.Refresh()
    if ($p.HasExited) { throw "Scarface exited before the game window (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -eq [IntPtr]::Zero) { continue }
    if ((ClassOf $p.MainWindowHandle) -eq 'Scarface') { $game = $p.MainWindowHandle; break }
}
if ($game -eq [IntPtr]::Zero) { throw 'the game window never appeared' }
Write-Host "game window $game"

Start-Sleep -Seconds $MenuWait
Write-Host ('menu:   ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_menu.bmp")))

Key $game 0x57 $false  # W: the keyboard must merge while the controller is connected
Start-Sleep -Milliseconds 600
Key $game 0x57 $true
Start-Sleep -Seconds 1

Key $game 0x70 $false  # F1
Start-Sleep -Milliseconds 80
Key $game 0x70 $true
Start-Sleep -Seconds 2
Write-Host ('overlay: ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_overlay.bmp")))
Key $game 0x1B $false  # Esc
Start-Sleep -Milliseconds 80
Key $game 0x1B $true
Start-Sleep -Seconds 1
Write-Host ('closed: ' + [W]::Shot($game, (Join-Path $root "dev\$Tag`_closed.bmp")))

[void][W]::PostMessage($game, 0x0010, [IntPtr]0, [IntPtr]0)  # WM_CLOSE
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; throw 'Scarface did not exit' }
Write-Host "exit code $($p.ExitCode)"
Remove-Item Env:XBE_LAUNCHER_PLAY -ErrorAction SilentlyContinue
Remove-Item Env:XBE_VERBOSE -ErrorAction SilentlyContinue

$keep = Select-String -Path $settings -Pattern '^(show_fps|skip_intro|launcher|kbm)\s*=' | ForEach-Object { $_.Line.Trim() }
Write-Host ('settings.toml: ' + ($keep -join '; '))

$newest = Get-ChildItem $logs -File -Filter 'scarface*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Copy-Item $newest.FullName (Join-Path $root "dev\$Tag.log") -Force
Write-Host "log dev\$Tag.log (from $($newest.Name))"
Select-String -Path (Join-Path $root "dev\$Tag.log") -Pattern 'skip_intro|input:|Overlay:|Launcher:|Exit' |
    ForEach-Object { $_.Line } | Select-Object -First 30
