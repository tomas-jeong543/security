param(
    [string]$DocumentPath='docs/EASILE_2D게임_및_보안반복실험_설계서_v0.13.docx',
    [string]$OutputDirectory='evidence/doc-v013-render'
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$path=(Resolve-Path -LiteralPath $DocumentPath).Path
$out=[IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
New-Item -ItemType Directory -Force -Path $out | Out-Null
$word=New-Object -ComObject Word.Application
try {
    $word.Visible=$false
    $word.DisplayAlerts=0
    $doc=$word.Documents.Open($path,$false,$true)
    try {
        $doc.ActiveWindow.View.Type=3
        $doc.Repaginate()
        $pages=$doc.ActiveWindow.Panes.Item(1).Pages
        $count=$pages.Count
        Write-Output "Word render pages=$count"
        for($i=1;$i -le $count;$i++) {
            [byte[]]$bytes=$pages.Item($i).EnhMetaFileBits
            $memory=[IO.MemoryStream]::new($bytes)
            $metafile=[Drawing.Imaging.Metafile]::new($memory)
            try {
                $width=1240
                $height=[int][Math]::Round($width*$metafile.Height/$metafile.Width)
                $image=[Drawing.Bitmap]::new($width,$height)
                $graphics=[Drawing.Graphics]::FromImage($image)
                try {
                    $graphics.Clear([Drawing.Color]::White)
                    $graphics.DrawImage($metafile,0,0,$width,$height)
                    $image.Save((Join-Path $out ('page-{0:D2}.png' -f $i)),[Drawing.Imaging.ImageFormat]::Png)
                } finally {$graphics.Dispose();$image.Dispose()}
            } finally {$metafile.Dispose();$memory.Dispose()}
        }
        Write-Output 'PNG rendering finished'
    } finally {$doc.Close($false)}
} finally {$word.Quit()}
