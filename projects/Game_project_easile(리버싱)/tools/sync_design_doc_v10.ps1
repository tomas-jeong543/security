param(
    [string]$SourcePath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.9.docx',
    [string]$OutputPath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.10.docx'
)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = (Resolve-Path -LiteralPath $SourcePath).Path
$output = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
Copy-Item -LiteralPath $source -Destination $output
$archive = [System.IO.Compression.ZipFile]::Open($output, [System.IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $archive.GetEntry('word/document.xml')
    $reader = [System.IO.StreamReader]::new($entry.Open())
    try { $xml = [xml]$reader.ReadToEnd() } finally { $reader.Dispose() }
    $ns = [System.Xml.XmlNamespaceManager]::new($xml.NameTable)
    $ns.AddNamespace('w', 'http://schemas.openxmlformats.org/wordprocessingml/2006/main')
    function Get-Text($node) { return ($node.SelectNodes('.//w:t', $ns) | ForEach-Object InnerText) -join '' }
    function Set-Text($node, [string]$value) {
        $runs = @($node.SelectNodes('.//w:t', $ns))
        if ($runs.Count -eq 0) { return }
        $runs[0].InnerText = $value
        for ($i = 1; $i -lt $runs.Count; ++$i) { $runs[$i].InnerText = '' }
    }
    foreach ($paragraph in $xml.SelectNodes('//w:body//w:p', $ns)) {
        $before = Get-Text $paragraph
        $after = $before.Replace('v0.9 클래스 구조', 'v0.10 클래스 구조').Replace('v0.9 요구사항을 반영한 기준선', 'v0.10 수정 사항을 반영한 기준선')
        if ($before -eq 'v0.9') { $after = 'v0.10' }
        if ($before -eq 'Brush 안 Hero는 기본적으로 상대에게 invisible, 같은 Brush 상대 Hero가 있거나 공격 후 1.5s reveal 중이면 visible이다.') {
            $after = '한 화면 맵의 Brush 밖 생존 Hero는 거리와 관계없이 visible이다. Brush 안 Hero만 상대에게 invisible이며, 같은 Brush 상대 Hero가 있거나 공격 후 1.5s reveal 중이면 visible이다. 사망 Hero는 렌더링과 Auto 조준에서 제외하고 부활 시 시야를 다시 계산한다.'
        }
        if ($before.StartsWith('기본 조준 모드는 Auto이다. K/J를 누르면')) {
            $after = $before + ' 현재 커서 위치에는 작은 십자 조준점을 표시한다. 조준점은 클라이언트 좌표를 1280×720 월드 좌표로 변환한 위치에 그리며 창 크기와 DPI가 바뀌어도 LMB 및 Mouse 모드의 발사 방향과 일치한다.'
        }
        if ($before.StartsWith('TC G01~G13과 새 TC G14~G17을 실행하고')) {
            $after = $before.Replace('새 TC G14~G17', 'TC G14~G17, Bot visibility 및 Basic damage path 회귀 테스트')
        }
        if ($after -ne $before) { Set-Text $paragraph $after }
    }
    foreach ($row in $xml.SelectNodes('//w:body//w:tr', $ns)) {
        $cells = @($row.SelectNodes('./w:tc', $ns))
        if ($cells.Count -lt 2) { continue }
        $category = (Get-Text $cells[0]).Trim()
        if ($category -eq '문서 버전') { Set-Text $cells[1] '0.10'; continue }
        if ($cells.Count -ge 3 -and $category -eq 'Hero' -and (Get-Text $cells[1]).Trim() -eq 'Vision Radius') {
            Set-Text $cells[2] 'Brush 밖: 화면 전체 / Brush 안: 같은 Brush 또는 reveal 1.5 s'
        }
    }
    $entry.Delete()
    $newEntry = $archive.CreateEntry('word/document.xml')
    $writer = [System.IO.StreamWriter]::new($newEntry.Open(), [System.Text.UTF8Encoding]::new($false))
    try { $xml.Save($writer) } finally { $writer.Dispose() }
} finally { $archive.Dispose() }

