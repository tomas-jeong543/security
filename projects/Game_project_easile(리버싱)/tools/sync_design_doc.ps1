param([Parameter(Mandatory=$true)][string]$DocumentPath)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$resolved = (Resolve-Path -LiteralPath $DocumentPath).Path
$archive = [System.IO.Compression.ZipFile]::Open($resolved, [System.IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $archive.GetEntry('word/document.xml')
    $reader = New-Object System.IO.StreamReader($entry.Open())
    try { $xml = [xml]$reader.ReadToEnd() } finally { $reader.Dispose() }
    $manager = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $manager.AddNamespace('w', 'http://schemas.openxmlformats.org/wordprocessingml/2006/main')
    $replacements = [ordered]@{
        '양 진영에는 Tower 1개와 Base/Nexus 1개가 있고' = '각 진영에는 라인별 Tower 3개와 Base/Nexus 1개가 있고'
        'LMB와 K는 완전히 동일한 BasicAttackCommand alias이며 같은 10.0s cooldown을 공유한다.' = 'LMB와 K는 같은 Basic 공격과 10.0s cooldown을 공유한다. LMB는 클릭 지점으로, K는 현재 Auto/Mouse 모드에 따라 조준한다.'
        'LMB/K는 완전히 동일한 입력 alias이며 cooldown을 공유한다.' = 'LMB/K는 같은 Basic 공격과 cooldown을 공유하며 조준 방향은 입력 방식에 따른다.'
        'v0.7 클래스 구조' = 'v0.8 클래스 구조'
        'v0.7 요구사항을 반영한 기준선' = 'v0.8 요구사항을 반영한 기준선'
        '13 플레이 경험 보완 규칙 v0.7' = '13 플레이 경험 보완 규칙 v0.8'
        '사거리 안 보이는 적을 우선 지정하고' = '사거리 안에서 보이는 적을 우선 지정하고'
        '약 240×160 px' = '약 240×90 px'
        '2) 현재 aimDirection을 읽는다.' = '2) LMB는 클릭 좌표 방향, K는 Auto/Mouse 모드의 aimDirection을 읽는다.'
        '2) 현재 aimDirection으로 Ultimate projectile을 생성 요청한다.' = '2) Auto/Mouse 모드의 aimDirection으로 Ultimate projectile을 생성 요청한다.'
    }
    foreach ($paragraph in $xml.SelectNodes('//w:body//w:p', $manager)) {
        $nodes = @($paragraph.SelectNodes('.//w:t', $manager))
        if ($nodes.Count -eq 0) { continue }
        $before = ($nodes | ForEach-Object { $_.InnerText }) -join ''
        $after = $before
        foreach ($key in $replacements.Keys) { $after = $after.Replace($key, $replacements[$key]) }
        if ($after -ne $before) {
            $nodes[0].InnerText = $after
            for ($index = 1; $index -lt $nodes.Count; ++$index) { $nodes[$index].InnerText = '' }
        }
    }
    $entry.Delete()
    $newEntry = $archive.CreateEntry('word/document.xml')
    $writer = New-Object System.IO.StreamWriter($newEntry.Open(), (New-Object System.Text.UTF8Encoding($false)))
    try { $xml.Save($writer) } finally { $writer.Dispose() }
} finally { $archive.Dispose() }
