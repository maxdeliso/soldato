Add-Type -AssemblyName System.Drawing

# Create a 32x32 bitmap
$bitmap = New-Object System.Drawing.Bitmap(32, 32)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)

# Set background to dark green
$graphics.Clear([System.Drawing.Color]::FromArgb(0, 20, 0))

# Create neon green pen
$pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0, 255, 0), 2)

# Draw cyberpunk terminal frame
$graphics.DrawRectangle($pen, 2, 2, 28, 28)

# Draw terminal lines (chat messages)
$graphics.DrawLine($pen, 8, 8, 24, 8)   # Line 1
$graphics.DrawLine($pen, 8, 12, 20, 12) # Line 2
$graphics.DrawLine($pen, 8, 16, 22, 16) # Line 3
$graphics.DrawLine($pen, 8, 20, 18, 20) # Line 4
$graphics.DrawLine($pen, 8, 24, 24, 24) # Line 5

# Add corner brackets
$graphics.DrawLine($pen, 4, 4, 8, 4)    # Top-left horizontal
$graphics.DrawLine($pen, 4, 4, 4, 8)    # Top-left vertical
$graphics.DrawLine($pen, 24, 4, 28, 4)  # Top-right horizontal
$graphics.DrawLine($pen, 28, 4, 28, 8)  # Top-right vertical

# Save as PNG
$bitmap.Save('cyberpunk_icon.png')

# Cleanup
$graphics.Dispose()
$bitmap.Dispose()

Write-Host "Cyberpunk icon created: cyberpunk_icon.png"
