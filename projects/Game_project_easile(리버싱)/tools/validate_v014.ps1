param([switch]$Gui)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'evidence/v014'
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Push-Location $root
try {
    foreach ($configuration in @('Debug','Release')) {
        $buildLog = Join-Path $output "$configuration-build.txt"
        & $msbuild EASILE.sln "/p:Configuration=$configuration" /p:Platform=x64 /m /v:minimal 2>&1 |
            Tee-Object -FilePath $buildLog
        if ($LASTEXITCODE -ne 0) { throw "$configuration build failed" }
        $tests = Join-Path $root "build/$configuration/GameTests.exe"
        & $tests 2>&1 | Tee-Object -FilePath (Join-Path $output "$configuration-tests.txt")
        if ($LASTEXITCODE -ne 0) { throw "$configuration tests failed" }
        $client = Join-Path $root "build/$configuration/GameClient.exe"
        Get-FileHash -LiteralPath $client -Algorithm SHA256 |
            Format-List | Out-File (Join-Path $output "$configuration-sha256.txt")
        if ($Gui) {
            & $tests --visual-expanded 2>&1 |
                Tee-Object -FilePath (Join-Path $output "$configuration-visual.txt")
            if ($LASTEXITCODE -ne 0) { throw "$configuration expanded visuals failed" }
            $process = Start-Process -FilePath $client -ArgumentList '--smoke-gui' -PassThru -WindowStyle Hidden
            if (!$process.WaitForExit(30000)) { throw "$configuration GUI smoke timed out" }
            if ($process.ExitCode -ne 0) { throw "$configuration GUI smoke failed" }
            Copy-Item -LiteralPath (Join-Path $root 'GameClient.log') -Destination (Join-Path $output "$configuration-smoke.log")
            & (Join-Path $PSScriptRoot 'gui_input_check.ps1') -Executable $client -Expanded 2>&1 |
                Tee-Object -FilePath (Join-Path $output "$configuration-gui.txt")
        }
    }
} finally { Pop-Location }
