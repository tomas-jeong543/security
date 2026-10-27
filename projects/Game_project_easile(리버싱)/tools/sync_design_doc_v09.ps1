param(
    [string]$SourcePath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.8.docx',
    [string]$OutputPath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.9.docx'
)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = (Resolve-Path -LiteralPath $SourcePath).Path
$output = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
Copy-Item -LiteralPath $source -Destination $output
$archive = [System.IO.Compression.ZipFile]::Open($output, [System.IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $archive.GetEntry('word/document.xml')
    $reader = New-Object System.IO.StreamReader($entry.Open())
    try { $xml = [xml]$reader.ReadToEnd() } finally { $reader.Dispose() }
    $manager = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $manager.AddNamespace('w', 'http://schemas.openxmlformats.org/wordprocessingml/2006/main')

    function Set-Text($parent, [string]$value) {
        $nodes = @($parent.SelectNodes('.//w:t', $manager))
        if ($nodes.Count -eq 0) { return }
        $nodes[0].InnerText = $value
        for ($index = 1; $index -lt $nodes.Count; ++$index) { $nodes[$index].InnerText = '' }
    }
    function Get-Text($parent) {
        return ($parent.SelectNodes('.//w:t', $manager) | ForEach-Object { $_.InnerText }) -join ''
    }

    $replacements = [ordered]@{
        'v0.8 클래스 구조' = 'v0.9 클래스 구조'
        'v0.8 요구사항을 반영한 기준선' = 'v0.9 요구사항을 반영한 기준선'
        'LMB와 K는 같은 Basic 공격과 10.0s cooldown을 공유한다.' = 'LMB와 K는 같은 Basic 공격이며 새 누름마다 즉시 한 발을 발사한다.'
        'J는 UltimateCommand이며 Basic과 별도의 10.0s cooldown을 사용한다.' = 'J는 UltimateCommand이며 5.0s cooldown을 사용한다.'
        'LMB/K 입력으로 동일 Basic projectile이 생성되고 10s cooldown이 시작된다.' = 'LMB/K 새 누름마다 동일 Basic projectile 한 발이 생성된다. 누른 상태에서는 반복 발사하지 않는다.'
        'J 입력으로 Ultimate projectile이 생성되고 독립 10s cooldown이 시작된다.' = 'J 입력으로 Ultimate projectile이 생성되고 5s cooldown이 시작된다.'
        'J 직선 Ultimate, Basic/Ultimate 각각 10.0s 독립 cooldown을 제공한다.' = 'J 직선 Ultimate와 5.0s cooldown을 제공한다. Player Basic은 새 누름마다 즉시 발사하고 Bot Basic은 1.0s AI 공격 주기를 사용한다.'
        'Basic +5 & Ultimate +8' = 'Basic +2 & Ultimate +8'
        'Basic/Ultimate cooldown 분기' = 'Ultimate cooldown 분기'
        '10s cooldown 무시' = 'Ultimate 5s cooldown 무시'
        'Basic attack cooldown' = 'Basic input edge and immediate repeat'
        'LMB/K 공유 10s cooldown, 10s 전 추가 projectile 없음' = '새 누름 2회는 즉시 2발, 길게 누름은 1발, 동시 LMB/K는 1발'
        'LMB/K cooldown 공유' = 'LMB/K 새 누름마다 1발'
        'Basic/Ultimate projectile, 각 10s cooldown' = 'Basic 새 누름마다 즉시 1발, Ultimate 5s cooldown'
        'Playing, 생존, Basic cooldown==0.' = 'Playing이고 생존 상태다.'
        '4) 즉시 10.0s cooldown을 시작한다. 5) 적 1개 충돌 시 damage 20' = '4) 새 누름당 한 발을 생성하며 누른 상태에서 반복하지 않는다. 5) 적 1개 충돌 시 damage 5'
        'cooldown>0 또는 Paused/Dead/Result면 요청을 무시한다. Brush에서 사용하면' = 'Paused/Dead/Result면 요청을 무시한다. Brush에서 사용하면'
        'LMB/K는 같은 Basic 공격과 cooldown을 공유하며' = 'LMB/K는 같은 Basic 공격을 새 누름당 한 번 생성하며'
        '3) 10.0s cooldown을 시작한다. 4) 충돌 시 damage 50' = '3) 5.0s cooldown을 시작한다. 4) 충돌 시 damage 30'
        'Basic cooldown과 독립이다.' = 'Player Basic의 새 누름 규칙과 독립이다.'
        'Attack은 Basic+5, Ultimate+8' = 'Attack은 Basic+2, Ultimate+8'
        'Basic +5, Ultimate +8' = 'Basic +2, Ultimate +8'
        'Basic의 최소 반격' = 'Basic의 1.0s AI 공격 주기에 따른 최소 반격'
        'LMB와 K는 동일한 Basic 공격 및 10초 쿨다운을 공유하고 J는 독립된 Ultimate 쿨다운을 유지한다.' = 'LMB와 K는 새 누름마다 동일한 Basic 공격 한 발을 즉시 발사하고 J는 독립된 5초 Ultimate 쿨다운을 유지한다.'
        'Hero ├─ HP / MaxHP / Attack / MoveSpeed ├─ Level / CurrentXP / HeroState └─ Basic/Ultimate cooldown' = 'Hero ├─ HP / MaxHP / Attack / MoveSpeed ├─ Level / CurrentXP / HeroState └─ Ultimate cooldown'
        'HP/XP/cooldown/result' = 'HP/XP/Ultimate cooldown/공격 피해량/조준 대상/result'
        'HP/Level/CurrentXP/RequiredXP/선호 강화/aim mode/cooldown/result' = 'HP/Level/CurrentXP/RequiredXP/선호 강화/aim mode/K·J 자동 표적/Ultimate cooldown/result'
        'target=BotHero amount=20' = 'target=BotHero amount=5'
    }
    foreach ($paragraph in $xml.SelectNodes('//w:body//w:p', $manager)) {
        $before = Get-Text $paragraph
        if (!$before) { continue }
        $after = $before
        foreach ($key in $replacements.Keys) { $after = $after.Replace($key, $replacements[$key]) }
        if ($before -eq 'v0.8') { $after = 'v0.9' }
        if ($after -ne $before) { Set-Text $paragraph $after }
    }

    foreach ($row in $xml.SelectNodes('//w:body//w:tr', $manager)) {
        $cells = @($row.SelectNodes('./w:tc', $manager))
        if ($cells.Count -lt 2) { continue }
        $category = (Get-Text $cells[0]).Trim()
        $parameter = (Get-Text $cells[1]).Trim()
        if ($category -eq '문서 버전') { Set-Text $cells[1] '0.9'; continue }
        if ($cells.Count -lt 3) { continue }
        if ($category -eq 'Basic' -and $parameter -eq 'Damage') { Set-Text $cells[2] '5' }
        if ($category -eq 'Basic' -and $parameter -eq 'Cooldown') { Set-Text $cells[2] 'Player: 새 누름마다 즉시 발사; Bot: AI 주기 1.0 s' }
        if ($category -eq 'Ultimate' -and $parameter -eq 'Damage') { Set-Text $cells[2] '30' }
        if ($category -eq 'Ultimate' -and $parameter -eq 'Cooldown') { Set-Text $cells[2] '5.0 s' }
        if ($category -eq 'Upgrade' -and $parameter -eq 'Attack') { Set-Text $cells[2] 'Basic +2, Ultimate +8' }
        if ($category -eq 'Bot' -and $parameter -eq 'Basic use') { Set-Text $cells[2] 'Enemy distance <=650 px; AI interval 1.0 s' }
        if ($category -eq 'Bot' -and $parameter -eq 'Ultimate use') { Set-Text $cells[2] 'Enemy distance <=850 px; cooldown 5.0 s' }
    }

    $entry.Delete()
    $newEntry = $archive.CreateEntry('word/document.xml')
    $writer = New-Object System.IO.StreamWriter($newEntry.Open(), (New-Object System.Text.UTF8Encoding($false)))
    try { $xml.Save($writer) } finally { $writer.Dispose() }
} finally { $archive.Dispose() }
