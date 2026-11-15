param([Parameter(Mandatory=$true)][string]$InputDocx,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$inputPath=(Resolve-Path -LiteralPath $InputDocx).Path
$outputPath=(Resolve-Path -LiteralPath $OutputDirectory).Path
$word=New-Object -ComObject Word.Application
$word.Visible=$false
$word.DisplayAlerts=0
try {
    Write-Output 'Opening read-only document'
    $doc=$word.Documents.Open($inputPath,$false,$true)
    Write-Output 'Document opened'
    $window=$doc.Windows.Item(1)
    $window.View.Type=3
    $count=$doc.ComputeStatistics(2)
    Write-Output "Computed pages=$count"
    # Native page rendering avoids the unavailable/stalled PDF export path.
    $pages=$window.Panes.Item(1).Pages
    for($i=1;$i -le $count;$i++) {
        $page=$pages.Item($i)
        Write-Output "Rendering page $i"
        [byte[]]$bits=$page.EnhMetaFileBits
        [IO.File]::WriteAllBytes((Join-Path $outputPath "page-$i.emf"),$bits)
        $stream=[IO.MemoryStream]::new($bits,$false)
        $meta=[Drawing.Imaging.Metafile]::new($stream)
        $width=1600
        $height=[int][Math]::Round($width*$page.Height/$page.Width)
        $bitmap=[Drawing.Bitmap]::new($width,$height)
        $g=[Drawing.Graphics]::FromImage($bitmap)
        $g.Clear([Drawing.Color]::White)
        $g.DrawImage($meta,[Drawing.Rectangle]::new(0,0,$width,$height))
        $bitmap.Save((Join-Path $outputPath "page-$i.png"),[Drawing.Imaging.ImageFormat]::Png)
        $g.Dispose(); $bitmap.Dispose(); $meta.Dispose(); $stream.Dispose()
    }
    [ordered]@{input=$inputPath;pages=$count;renderer='Native Word Page.EnhMetaFileBits';wordVersion=$word.Version} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputPath 'render.json') -Encoding UTF8
    $doc.Close(0)
    Write-Output "Rendered $count pages to $outputPath"
} finally {
    $saveChanges=[object]0
    $word.Quit([ref]$saveChanges)
    [Runtime.InteropServices.Marshal]::ReleaseComObject($word) | Out-Null
}
