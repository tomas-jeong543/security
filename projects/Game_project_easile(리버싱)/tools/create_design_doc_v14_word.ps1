param(
    [string]$SourcePath='docs/EASILE_2D게임_및_보안반복실험_설계서_v0.13.docx',
    [string]$OutputPath='docs/EASILE_2D게임_및_보안반복실험_설계서_v0.14.docx'
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath $SourcePath).Path
$output=Join-Path (Get-Location) $OutputPath
Copy-Item -LiteralPath $source -Destination $output -Force
$diagram=(Resolve-Path -LiteralPath 'docs/easile_v014_map.png').Path
$markdown=Get-Content -LiteralPath 'docs/EASILE_design_v0.14.md' -Encoding UTF8
$lines=@('15. v0.14 확장 맵·정글·2:2 Hero 현재 구현 (이 장이 이전 게임 규칙보다 우선)')
foreach($line in $markdown) {
    if([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('# ')){continue}
    $lines+=($line -replace '^## ','')
}
$word=New-Object -ComObject Word.Application
try {
    $word.Visible=$false;$word.DisplayAlerts=0
    $doc=$word.Documents.Open($output,$false,$false)
    Write-Output 'Opened source'
    try {
        $chapter=$doc.Content.Duplicate
        if(!$chapter.Find.Execute('15. v0.13')) {throw 'v0.13 implementation chapter not found'}
        $chapterStart=$chapter.Start
        $replacement=$doc.Range($chapterStart,$doc.Content.End-1)
        $replacement.Text=($lines -join "`r")+"`r"
        Write-Output 'Replaced implementation chapter'
        $latest=$doc.Range($chapterStart,$doc.Content.End-1)
        $latest.Style=-1
        $latest.Font.Name='맑은 고딕';$latest.Font.Size=11;$latest.Font.Bold=0
        $latest.ParagraphFormat.SpaceBefore=0;$latest.ParagraphFormat.SpaceAfter=6
        $latest.ParagraphFormat.LineSpacingRule=5;$latest.ParagraphFormat.LineSpacing=13.8
        $latest.ParagraphFormat.KeepTogether=$true;$latest.ParagraphFormat.KeepWithNext=$false
        $headings=@($markdown|Where-Object{$_.StartsWith('## ')}|ForEach-Object{$_ -replace '^## ',''})
        foreach($paragraph in $latest.Paragraphs) {
            $text=$paragraph.Range.Text.Trim()
            if($text.StartsWith('15. v0.14')) {
                $paragraph.Range.Style=-2;$paragraph.Range.Font.Size=14;$paragraph.Range.Font.Bold=1
                $paragraph.Format.PageBreakBefore=$true;$paragraph.Format.KeepWithNext=$true
            } elseif($headings -contains $text) {
                $paragraph.Range.Style=-3;$paragraph.Range.Font.Size=13;$paragraph.Range.Font.Bold=1
                $paragraph.Format.SpaceBefore=10;$paragraph.Format.SpaceAfter=6
                $paragraph.Format.KeepWithNext=$true
            }
        }
        Write-Output 'Formatted chapter'
        $version=$doc.Content.Duplicate
        if($version.Find.Execute('v0.13')){$version.Text='v0.14'}
        $doc.Tables.Item(1).Cell(2,2).Range.Text='0.14'
        $doc.Tables.Item(1).Cell(3,2).Range.Text='2026년 9월 29일'
        $versionNote=$doc.Range($version.End,$version.End)
        $versionNote.Text="`r현재 게임 수치·맵·봇 구조의 기준은 15장과 EASILE_design_v0.14.md다. 이전 장의 상충하는 게임 규칙은 최신 장으로 대체한다. NEW BASELINE: NOT READY — 사람의 직접 플레이 검증이 남아 있다.`r"
        $versionNote.Font.Size=11;$versionNote.Font.Bold=0
        $map=$doc.Content.Duplicate
        if(!$map.Find.Execute('Map·Lane·Base·Tower·Nexus')){throw 'Map heading missing'}
        $mapParagraph=$map.Paragraphs.Item(1)
        $mapParagraph.Format.PageBreakBefore=$true
        $insertion=$doc.Range($mapParagraph.Range.End,$mapParagraph.Range.End)
        $caption='그림 5. v0.14: 3 Lane, 정글 통로·전투 공간, 벽 4개, Bush 6개, 회복 오브젝트, Hero 생성점.'
        $insertion.Text=$caption+"`r`r"
        $insertion.Font.Size=10;$insertion.Font.Bold=0
        $insertion.ParagraphFormat.KeepWithNext=$true
        $pictureRange=$doc.Range($insertion.End-1,$insertion.End-1)
        $picture=$doc.InlineShapes.AddPicture($diagram,$false,$true,$pictureRange)
        Write-Output 'Inserted figure'
        $picture.LockAspectRatio=$true;$picture.Width=430
        Write-Output 'Saving document'
        $doc.Save()
        Write-Output ('Native Word v0.14 saved; pages='+$doc.ComputeStatistics(2))
    }finally{$doc.Close($false)}
}finally{$word.Quit()}
