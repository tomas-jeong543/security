param(
    [string]$SourcePath = 'tmp_docx_media/game_design_source.txt',
    [string]$ImagePath = 'tmp_docx_media/image5.png',
    [string]$OutputPath = 'tmp_docx_media/EASILE_2D_game_design_v0.14.draft.docx'
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourcePath).Path
$map = (Resolve-Path -LiteralPath $ImagePath).Path
$output = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
$lines = Get-Content -LiteralPath $source -Encoding UTF8

$word = New-Object -ComObject Word.Application
try {
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $doc = $word.Documents.Add()
    try {
        $section = $doc.Sections.Item(1)
        $section.PageSetup.PageWidth = 612
        $section.PageSetup.PageHeight = 792
        $section.PageSetup.TopMargin = 72
        $section.PageSetup.BottomMargin = 72
        $section.PageSetup.LeftMargin = 72
        $section.PageSetup.RightMargin = 72
        $section.PageSetup.HeaderDistance = 35.4
        $section.PageSetup.FooterDistance = 35.4

        $normal = $doc.Styles.Item(-1)
        $normal.Font.Name = 'Calibri'
        $normal.Font.NameFarEast = [char]0xB9D1 + [char]0xC740 + ' ' + [char]0xACE0 + [char]0xB515
        $normal.Font.Size = 11
        $normal.Font.Color = 0
        $normal.ParagraphFormat.SpaceBefore = 0
        $normal.ParagraphFormat.SpaceAfter = 6
        $normal.ParagraphFormat.LineSpacingRule = 5
        $normal.ParagraphFormat.LineSpacing = 13.75

        $title = $doc.Styles.Item(-63)
        $title.Font.Name = 'Calibri'
        $title.Font.NameFarEast = $normal.Font.NameFarEast
        $title.Font.Size = 26
        $title.Font.Bold = 1
        $title.Font.Color = 0x45250B
        $title.ParagraphFormat.SpaceBefore = 0
        $title.ParagraphFormat.SpaceAfter = 6

        $subtitle = $doc.Styles.Item(-75)
        $subtitle.Font.Name = 'Calibri'
        $subtitle.Font.NameFarEast = $normal.Font.NameFarEast
        $subtitle.Font.Size = 11
        $subtitle.Font.Color = 0x666666
        $subtitle.ParagraphFormat.SpaceBefore = 0
        $subtitle.ParagraphFormat.SpaceAfter = 14

        foreach ($spec in @(
            @(-2,16,0xB5742E,18,10),
            @(-3,13,0xB5742E,14,7),
            @(-4,12,0x784D1F,10,5)
        )) {
            $style = $doc.Styles.Item($spec[0])
            $style.Font.Name = 'Calibri'
            $style.Font.NameFarEast = $normal.Font.NameFarEast
            $style.Font.Size = $spec[1]
            $style.Font.Bold = 1
            $style.Font.Color = $spec[2]
            $style.ParagraphFormat.SpaceBefore = $spec[3]
            $style.ParagraphFormat.SpaceAfter = $spec[4]
            $style.ParagraphFormat.KeepWithNext = $true
        }

        $header = $section.Headers.Item(1).Range
        $header.Text = 'EASILE  |  GAME DESIGN  |  v0.14'
        $header.Font.Name = 'Calibri'
        $header.Font.Size = 8
        $header.Font.Color = 0x777777
        $footer = $section.Footers.Item(1).Range
        $footer.Text = 'EASILE Baseline 1.0  |  '
        $footer.Font.Name = 'Calibri'
        $footer.Font.Size = 8
        $footer.Font.Color = 0x777777
        $footer.ParagraphFormat.Alignment = 2
        $footer.Collapse(0)
        $null = $doc.Fields.Add($footer, -1, 'PAGE', $false)

        $selection = $word.Selection
        $selection.HomeKey(6) | Out-Null
        foreach ($line in $lines) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $cut = $line.IndexOf('|')
            if ($cut -lt 0) { throw "Invalid source line: $line" }
            $kind = $line.Substring(0,$cut)
            $value = $line.Substring($cut+1)
            switch ($kind) {
                'TITLE' { $selection.Style = $title }
                'SUBTITLE' { $selection.Style = $subtitle }
                'H1' { $selection.Style = $doc.Styles.Item(-2) }
                'H2' { $selection.Style = $doc.Styles.Item(-3) }
                'H3' { $selection.Style = $doc.Styles.Item(-4) }
                'P' { $selection.Style = $normal }
                'L' { $selection.Style = $normal }
                'IMAGE' { $selection.Style = $normal }
                default { throw "Unknown source kind: $kind" }
            }
            if ($kind -in @('TITLE','H1','H2','H3')) {
                $selection.Font.Bold = 1
            } else {
                $selection.Font.Bold = 0
            }
            if ($kind -eq 'IMAGE') {
                $selection.ParagraphFormat.KeepWithNext = $true
                $selection.ParagraphFormat.KeepTogether = $true
                $selection.TypeText($value)
                $selection.TypeParagraph()
                $selection.Style = $normal
                $shape = $doc.InlineShapes.AddPicture($map,$false,$true,$selection.Range)
                $shape.LockAspectRatio = $true
                $shape.Width = 390
                $selection.SetRange($shape.Range.End,$shape.Range.End)
                $selection.TypeParagraph()
            } else {
                if ($kind -eq 'L') {
                    $selection.Range.ListFormat.ApplyBulletDefault()
                    $selection.Font.Bold = 0
                    $selection.ParagraphFormat.LeftIndent = 27
                    $selection.ParagraphFormat.FirstLineIndent = -13.5
                    $selection.ParagraphFormat.SpaceAfter = 4
                    $selection.ParagraphFormat.LineSpacingRule = 5
                    $selection.ParagraphFormat.LineSpacing = 13.75
                }
                $selection.TypeText($value)
                $selection.TypeParagraph()
            }
        }
        $doc.Repaginate()
        $doc.SaveAs2($output,16)
        Write-Output ("Saved draft pages=" + $doc.ComputeStatistics(2) + " path=" + $output)
    } finally {
        $doc.Close($false)
    }
} finally {
    $word.Quit()
}
