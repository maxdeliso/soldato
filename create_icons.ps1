Add-Type -AssemblyName System.Drawing

# Function to create cyberpunk icon
function Create-CyberpunkIcon {
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

    return $bitmap
}

# Create different sizes
$sizes = @(16, 32, 48, 64)
$icons = @()

foreach ($size in $sizes) {
    $icon = Create-CyberpunkIcon -size $size
    $icons += $icon
    $icon.Save("cyberpunk_${size}x${size}.png")
    Write-Host "Created ${size}x${size} icon"
}

# Create ICO file with multiple sizes
$iconStream = New-Object System.IO.MemoryStream
$iconStream.Write([byte[]]@(0, 0, 1, 0, $icons.Count, 0), 0, 6)  # ICO header

$offset = 6 + ($icons.Count * 16)  # Header + directory entries
$dataOffset = $offset

# Write directory entries
foreach ($i in 0..($icons.Count-1)) {
    $size = $sizes[$i]
    $icon = $icons[$i]

    # Convert to PNG bytes
    $pngStream = New-Object System.IO.MemoryStream
    $icon.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytes = $pngStream.ToArray()
    $pngStream.Dispose()

    # Directory entry
    $entry = @(
        [byte]$size, [byte]$size,  # Width, Height
        0, 0,                      # Color count, Reserved
        1, 0,                      # Planes, Bits per pixel
        [byte]($pngBytes.Length - $offset), [byte](($pngBytes.Length - $offset) -shr 8), [byte](($pngBytes.Length - $offset) -shr 16), [byte](($pngBytes.Length - $offset) -shr 24),  # Size
        [byte]$dataOffset, [byte]($dataOffset -shr 8), [byte]($dataOffset -shr 16), [byte]($dataOffset -shr 24)  # Offset
    )

    $iconStream.Write($entry, 0, 16)
    $dataOffset += $pngBytes.Length
}

# Write PNG data
foreach ($icon in $icons) {
    $pngStream = New-Object System.IO.MemoryStream
    $icon.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytes = $pngStream.ToArray()
    $iconStream.Write($pngBytes, 0, $pngBytes.Length)
    $pngStream.Dispose()
}

# Save ICO file
[System.IO.File]::WriteAllBytes("cyberpunk_chat.ico", $iconStream.ToArray())
$iconStream.Dispose()

# Cleanup
foreach ($icon in $icons) {
    $icon.Dispose()
}

Write-Host "Cyberpunk ICO file created: cyberpunk_chat.ico"
