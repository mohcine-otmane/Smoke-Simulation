# Build and Installer Creation Script for Countdown Timer

# Configuration
$InnoSetupPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
$DistDir = "dist"
$InstallerDir = "installer"

Write-Host "Starting build process." -ForegroundColor Green

# Create directories if they don't exist
if (-not (Test-Path $DistDir)) {
    Write-Host "Creating distribution directory." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $DistDir | Out-Null
}

if (-not (Test-Path $InstallerDir)) {
    Write-Host "Creating installer directory." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $InstallerDir | Out-Null
}

# Compile the program using the existing batch file
Write-Host "Compiling program." -ForegroundColor Yellow
& .\compile_countdown.bat

if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed!" -ForegroundColor Red
    exit 1
}

# Copy files to distribution directory
Write-Host "Copying files to distribution directory." -ForegroundColor Yellow
$files = @(
    "countdown.exe",
    "SDL2.dll",
    "SDL2_ttf.dll",
    "SDL2_image.dll",
    "Technology.ttf",
    "config.txt",
    "quotes.txt",
    "icon.png"
)

foreach ($file in $files) {
    if (Test-Path $file) {
        Copy-Item $file -Destination $DistDir -Force
        Write-Host "Copied $file" -ForegroundColor Green
    } else {
        Write-Host "Warning: $file not found!" -ForegroundColor Yellow
    }
}

# Create installer
Write-Host "Creating installer." -ForegroundColor Yellow
if (Test-Path $InnoSetupPath) {
    & $InnoSetupPath setup.iss
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Installer created successfully!" -ForegroundColor Green
        Write-Host "Installer location: $((Get-Item $InstallerDir).FullName)\CountdownTimer-Setup.exe" -ForegroundColor Cyan
    } else {
        Write-Host "Installer creation failed!" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Error: Inno Setup not found at $InnoSetupPath" -ForegroundColor Red
    Write-Host "Please install Inno Setup from https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
    exit 1
}

Write-Host "\nBuild process completed!" -ForegroundColor Green
Write-Host "You can find the installer at: $((Get-Item $InstallerDir).FullName)\CountdownTimer-Setup.exe" -ForegroundColor Cyan 