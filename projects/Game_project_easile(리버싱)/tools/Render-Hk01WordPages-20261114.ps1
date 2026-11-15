$ErrorActionPreference='Stop'
$run=(Get-Content -Encoding UTF8 'evidence/hk01-report-latest.txt').Trim()
$report=(Get-ChildItem 'api_hooking_project' -Filter '*간결본.docx').FullName
$output=Join-Path $run 'qa-final'
$null=New-Item -ItemType Directory -Path $output -Force
Add-Type -AssemblyName System.Drawing
$qaWord=New-Object -ComObject Word.Application
$qaWord.Visible=$false;$qaWord.DisplayAlerts=0
try {
 $qaDoc=$qaWord.Documents.Open($report,$false,$true)
 $qaDoc.Repaginate()
 $qaDoc.ActiveWindow.View.Type=3
 $pages=$qaDoc.ActiveWindow.Panes.Item(1).Pages
 Write-Output ('Page count: '+$pages.Count)
 for($i=1;$i -le $pages.Count;$i++){
  $bits=$pages.Item($i).EnhMetaFileBits
  $ms=[IO.MemoryStream]::new([byte[]]$bits)
  $meta=[Drawing.Imaging.Metafile]::new($ms)
  $bitmap=[Drawing.Bitmap]::new(1191,1684)
  $g=[Drawing.Graphics]::FromImage($bitmap);$g.Clear([Drawing.Color]::White)
  $g.DrawImage($meta,[Drawing.Rectangle]::new(0,0,1191,1684))
  $bitmap.Save((Join-Path (Get-Location) "$output/page-$i.png"),[Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose();$bitmap.Dispose();$meta.Dispose();$ms.Dispose()
  Write-Output ('Rendered page '+$i)
 }
 $qaDoc.Close([ref]0)
} finally {$qaWord.Quit([ref]0)}
