Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = 'GDI probe'
$form.ClientSize = New-Object System.Drawing.Size(760, 600)
$form.StartPosition = 'Manual'
$form.Location = New-Object System.Drawing.Point(0, 0)
$form.BackColor = [System.Drawing.Color]::FromArgb(11, 12, 11)
$form.Add_Paint({
    param($s, $e)
    $g = $e.Graphics
    $g.Clear([System.Drawing.Color]::FromArgb(11, 12, 11))
    # full width bars at exact rows
    for ($i = 0; $i -lt 10; $i++) {
        $g.FillRectangle([System.Drawing.Brushes]::Lime, 0, $i * 20, 760, 10)
    }
    # corner markers
    $g.FillRectangle([System.Drawing.Brushes]::Red, 0, 0, 40, 40)
    $g.FillRectangle([System.Drawing.Brushes]::Red, 720, 0, 40, 40)
    $g.FillRectangle([System.Drawing.Brushes]::Red, 0, 560, 40, 40)
    $g.FillRectangle([System.Drawing.Brushes]::Red, 720, 560, 40, 40)
    $g.DrawString('top left', (New-Object System.Drawing.Font('Consolas', 12)), [System.Drawing.Brushes]::White, 50, 50)
    $g.DrawString('top right', (New-Object System.Drawing.Font('Consolas', 12)), [System.Drawing.Brushes]::White, 620, 50)
})

$form.Show()
[void]$form.Focus()
$deadline = (Get-Date).AddSeconds(6)
while ((Get-Date) -lt $deadline) {
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 30
}

$bmp = New-Object System.Drawing.Bitmap(1600, 900)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen(0, 0, 0, 0, (New-Object System.Drawing.Size(1600, 900)))
$g.Dispose()
$bmp.Save('C:\Users\elton\Downloads\Scarface\dev\gdi_probe.bmp', [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()
$form.Close()
Write-Host 'saved dev\gdi_probe.bmp'
