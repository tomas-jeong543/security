$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Dll = Join-Path $Root 'SDL2.dll'
$Exe = Join-Path $Root 'PenguinRift.exe'

if (-not (Test-Path $Exe)) {
    Write-Host 'PenguinRift.exe is missing.' -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $Dll)) {
    $Version = '2.32.10'
    $Zip = Join-Path $env:TEMP "SDL2-$Version-win32-x64.zip"
    $Extract = Join-Path $env:TEMP "PenguinRift-SDL2-$Version"
    $Url = "https://www.libsdl.org/release/SDL2-$Version-win32-x64.zip"

    Write-Host "Downloading the official SDL2 $Version x64 runtime..."
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Zip
    if ((Get-Item $Zip).Length -lt 100000) { throw 'The SDL2 download is unexpectedly small.' }
    if (Test-Path $Extract) { Remove-Item $Extract -Recurse -Force }
    Expand-Archive -Path $Zip -DestinationPath $Extract -Force
    $Found = Get-ChildItem -Path $Extract -Filter 'SDL2.dll' -Recurse | Select-Object -First 1
    if (-not $Found) { throw 'SDL2.dll was not found in the downloaded archive.' }
    Copy-Item $Found.FullName $Dll -Force
    Write-Host 'SDL2.dll installed beside the executable.' -ForegroundColor Green
}

Start-Process -FilePath $Exe -WorkingDirectory $Root
