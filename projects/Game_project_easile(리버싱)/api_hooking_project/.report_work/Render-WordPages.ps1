$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$word=[Runtime.InteropServices.Marshal]::GetActiveObject('Word.Application')
$word.DisplayAlerts=0
$taskRoot=Split-Path $PSScriptRoot -Parent
$entries = ,@('EASILE_HK01_Bot_Aim_Hooking_완성_보고서_간결본','compact_pages')
foreach($entry in $entries) {
    $source=Join-Path $taskRoot ($entry[0]+'.docx')
    $out=Join-Path $PSScriptRoot $entry[1]
    $null=New-Item -ItemType Directory -Path $out -Force
    $doc=$word.Documents.Open($source,$false,$true)
    $doc.Repaginate()
    $null=$doc.Sections.Item(1).Footers.Item(1).Range.Fields.Update()
    $doc.ActiveWindow.View.Type=3
    $pages=$doc.ActiveWindow.Panes.Item(1).Pages
    $count=$pages.Count
    Write-Output ($entry[1]+': '+$count+' pages')
    for($pageIndex=1;$pageIndex -le $count;$pageIndex++) {
        $emf=Join-Path $out ('page-'+$pageIndex+'.emf')
        [byte[]]$bits=$pages.Item($pageIndex).EnhMetaFileBits
        [IO.File]::WriteAllBytes($emf,$bits)
        $picture=[Drawing.Image]::FromFile($emf)
        try {
            $bitmap=New-Object Drawing.Bitmap(1240,1754)
            try {
                $graphics=[Drawing.Graphics]::FromImage($bitmap)
                try { $graphics.Clear([Drawing.Color]::White); $graphics.DrawImage($picture,0,0,1240,1754) } finally { $graphics.Dispose() }
                $bitmap.Save((Join-Path $out ('page-'+$pageIndex+'.png')),[Drawing.Imaging.ImageFormat]::Png)
            } finally { $bitmap.Dispose() }
        } finally { $picture.Dispose() }
        Write-Output ('Rendered page '+$pageIndex)
    }
    $doc.Close(0)
}






