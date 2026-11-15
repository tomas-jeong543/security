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
    $doc=$word.Documents.Open($inputPath,$false,$true)
    $doc.ActiveWindow.View.Type=3
    $doc.Repaginate()
    $count=$doc.ComputeStatistics(2)
    $doc.ExportAsFixedFormat((Join-Path $outputPath 'preview.pdf'),17)
    $pages=$doc.ActiveWindow.Panes.Item(1).Pages
    for($i=1;$i -le $count;$i++) {
        $page=$pages.Item($i)
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
    $word.Quit()
    [Runtime.InteropServices.Marshal]::ReleaseComObject($word) | Out-Null
}
