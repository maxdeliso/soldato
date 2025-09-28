Add-Type -AssemblyName System.Drawing

# Create a proper ICO file using Icon.Save method
function Create-ProperIcon {
    param([int]$size)

    $bitmap = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    # Set background to dark green
    $graphics.Clear([System.Drawing.Color]::FromArgb(0, 20, 0))

    # Create neon green pen
    $penWidth = [Math]::Max(1, $size / 16)
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0, 255, 0), $penWidth)

    # Draw cyberpunk terminal frame
    $margin = [Math]::Max(1, $size / 16)
    $graphics.DrawRectangle($pen, $margin, $margin, $size - 2*$margin, $size - 2*$margin)

    # Draw terminal lines (chat messages)
    $lineSpacing = $size / 8
    $startX = $size / 4
    $endX = 3 * $size / 4

    for ($i = 1; $i -le 5; $i++) {
        $y = $margin + $i * $lineSpacing
        $lineEndX = $endX - ($i * $size / 16)  # Varying line lengths
        $graphics.DrawLine($pen, $startX, $y, $lineEndX, $y)
    }

    # Add corner brackets
    $bracketSize = $size / 8
    # Top-left
    $graphics.DrawLine($pen, $margin, $margin, $margin + $bracketSize, $margin)
    $graphics.DrawLine($pen, $margin, $margin, $margin, $margin + $bracketSize)
    # Top-right
    $graphics.DrawLine($pen, $size - $margin - $bracketSize, $margin, $size - $margin, $margin)
    $graphics.DrawLine($pen, $size - $margin, $margin, $size - $margin, $margin + $bracketSize)

    # Add a small "C" for Chat
    $font = New-Object System.Drawing.Font("Consolas", $size/4, [System.Drawing.FontStyle]::Bold)
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 255, 0))
    $graphics.DrawString("C", $font, $brush, $size/2 - $size/8, $size/2 - $size/8)

    $font.Dispose()
    $brush.Dispose()
    $pen.Dispose()
    $graphics.Dispose()

    return $bitmap
}

# Create 32x32 icon
$icon32 = Create-ProperIcon -size 32
$icon32.Save("WindowsProject\cyberpunk.ico", [System.Drawing.Imaging.ImageFormat]::Icon)

# Create 16x16 icon
$icon16 = Create-ProperIcon -size 16
$icon16.Save("WindowsProject\cyberpunk_small.ico", [System.Drawing.Imaging.ImageFormat]::Icon)

# Cleanup
$icon32.Dispose()
$icon16.Dispose()

Write-Host "Proper ICO files created!"
