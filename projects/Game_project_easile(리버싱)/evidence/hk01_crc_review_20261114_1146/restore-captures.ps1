$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$run = Join-Path $PSScriptRoot 'run_20261114_114849_944849'
$before = Get-Content -LiteralPath (Join-Path $run 'preserved-before.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$records = @()
foreach ($configuration in @('Debug','Release')) {
    $relative = "evidence\hk01_triple_20261114\final_20261114_112516\bin\$configuration\visual-smoke.bmp"
    $expected = $before.PSObject.Properties[$relative].Value
    if (!$expected) { throw 'Missing pre-run capture hash' }
    $target = (Resolve-Path -LiteralPath (Join-Path $projectRoot $relative)).Path
    $requiredDirectory = (Resolve-Path -LiteralPath (Join-Path $projectRoot "evidence/hk01_triple_20261114/final_20261114_112516/bin/$configuration")).Path
    if ((Split-Path $target -Parent) -ine $requiredDirectory) { throw 'Restore target mismatch' }
    $current = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    $source = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'evidence/hk01_triple_20261114') -Recurse -File -Filter 'visual-smoke.bmp' | Where-Object {
        $_.FullName -ine $target -and (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $expected
    } | Select-Object -First 1
    if (!$source) { throw "No hash-identical original found for $configuration; no write performed" }
    $saved = Join-Path $run "unexpected-render-$configuration.bmp"
    if (Test-Path -LiteralPath $saved) { throw 'Do not overwrite this attempt evidence' }
    Copy-Item -LiteralPath $target -Destination $saved
    Copy-Item -LiteralPath $source.FullName -Destination $target
    $restored = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($restored -ne $expected) { throw 'Restoration hash mismatch' }
    $records += [pscustomobject]@{target=$target;source=$source.FullName;preRunSha256=$expected;unexpectedSha256=$current;restoredSha256=$restored;unexpectedCopy=$saved}
    Write-Output "$configuration capture restored: $restored"
}
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'capture-restoration.json') -Encoding UTF8
