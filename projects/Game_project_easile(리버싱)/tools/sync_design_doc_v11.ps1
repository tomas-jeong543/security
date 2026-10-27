param(
    [string]$SourcePath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.10.docx',
    [string]$OutputPath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.11.docx'
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
    $uri = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
    $ns = [System.Xml.XmlNamespaceManager]::new($xml.NameTable)
    $ns.AddNamespace('w', $uri)
    function Get-Text($node) { return ($node.SelectNodes('.//w:t', $ns) | ForEach-Object InnerText) -join '' }
    function Set-Text($node, [string]$value) {
        $runs = @($node.SelectNodes('.//w:t', $ns))
        if ($runs.Count -eq 0) { return }
        $runs[0].InnerText = $value
        for ($i = 1; $i -lt $runs.Count; ++$i) { $runs[$i].InnerText = '' }
    }
    foreach ($paragraph in $xml.SelectNodes('//w:body//w:p', $ns)) {
        $before = Get-Text $paragraph
        $after = $before.Replace('v0.10 클래스 구조', 'v0.11 클래스 구조').Replace('v0.10 수정 사항을 반영한 기준선', 'v0.11 수정 사항을 반영한 기준선')
        if ($before -eq 'v0.10') { $after = 'v0.11' }
        $after = $after.Replace('Minion은 적 Tower만 공격한다.', 'Minion은 자기 경로의 살아 있는 적 Tower를 먼저 공격하고 파괴 후 적 Nexus를 공격한다.')
        $after = $after.Replace('Minion은 Tower만 공격', 'Minion은 Tower 우선, 이후 Nexus 공격')
        $after = $after.Replace('Tower만 공격, Tower 파괴 후 계속 Advance', 'Tower 앞 정지·반복 공격, 파괴 후 Nexus 앞 정지·반복 공격')
        $after = $after.Replace('적 Tower만 공격', '적 Tower 우선·이후 Nexus 공격')
        $after = $after.Replace('3개 단순 직선 라인', '양 Base에서 분기·합류하는 3개 연결 라인')
        $after = $after.Replace('MinionController', 'GameWorld::UpdateMinions')
        $after = $after.Replace('MinionState', 'waypoint/attackCd')
        $after = $after.Replace('전진과 적 Tower만 대상 선택', '웨이포인트 전진과 적 Tower 우선·이후 Nexus 대상 선택')
        $after = $after.Replace('Minion은 Enemy Hero/Enemy Minion/Base를 공격하지 않고 Enemy Tower만 공격한다. Tower가 파괴되면 계속 Advance한다.', 'Minion은 Enemy Hero/Enemy Minion을 공격하지 않는다. 자기 라인의 살아 있는 Enemy Tower를 먼저 공격하고, 파괴 후 Enemy Nexus를 공격한다. 타깃 앞 70/60 px에서 정지해 8 피해를 1초마다 준다.')
        $after = $after.Replace('Base는 공격 기능이 없고 PlayerHero/BotHero의 공격으로 피해를 받을 수 있다.', 'Base는 공격 기능이 없고 PlayerHero/BotHero 투사체와 적 Minion의 공격으로 피해를 받을 수 있다. Minion은 자기 라인의 적 Tower가 파괴된 뒤에만 Base를 공격한다.')
        if ($before.StartsWith('맵 상단, 중앙, 하단에 수평 이동 경로를 놓는다.')) {
            $after = '맵은 1280×720 한 화면이다. 양 Base(70,360)/(1210,360)에서 각각 (170,360)/(1110,360)으로 나와 세 경로로 분기한다. 각 경로는 y=180/360/540에서 분리되어 양 진영 Tower(260,y)/(1020,y)를 지나고 상대 Base 앞에서 합류한다. Minion은 자기 Base 밖 x=135 또는 1145, y=360+(lane-1)×40±10에서 출발해 공통 웨이포인트를 따라 110 px/s로 이동한다. 자기 경로의 살아 있는 적 Tower에 70 px 이내로 접근하면 정지해 8 피해를 1초마다 주며, Tower 파괴 후 같은 경로로 적 Nexus까지 가서 60 px 이내에서 8 피해를 1초마다 준다. Hero는 맵 전체를 자유롭게 이동하고 고급 길찾기/정글은 범위 밖이다.'
        }
        if ($after -ne $before) { Set-Text $paragraph $after }
    }
    foreach ($row in $xml.SelectNodes('//w:body//w:tr', $ns)) {
        $cells = @($row.SelectNodes('./w:tc', $ns))
        if ($cells.Count -lt 2) { continue }
        $category = (Get-Text $cells[0]).Trim()
        if ($category -eq '문서 버전') { Set-Text $cells[1] '0.11'; continue }
        if ($cells.Count -ge 3) {
            $parameter = (Get-Text $cells[1]).Trim()
            if ($category -eq 'Map' -and $parameter -eq 'Lanes') { Set-Text $cells[2] 'Base(70/1210,360) ↔ 분기(170/1110,360) ↔ y=180/360/540, 도로 폭 101 px' }
            if ($category -eq 'Minion' -and $parameter -eq 'Attack Range') { Set-Text $cells[2] 'Tower 70 px / Nexus 60 px' }
        }
    }
    $body = $xml.SelectSingleNode('//w:body', $ns)
    $section = $body.SelectSingleNode('./w:sectPr', $ns)
    function Append-Paragraph([string]$value, [string]$style = 'Normal') {
        $p = $xml.CreateElement('w', 'p', $uri)
        $pPr = $xml.CreateElement('w', 'pPr', $uri)
        $pStyle = $xml.CreateElement('w', 'pStyle', $uri)
        $pStyle.SetAttribute('val', $uri, $style)
        [void]$pPr.AppendChild($pStyle)
        [void]$p.AppendChild($pPr)
        $run = $xml.CreateElement('w', 'r', $uri)
        $text = $xml.CreateElement('w', 't', $uri)
        $text.InnerText = $value
        [void]$run.AppendChild($text)
        [void]$p.AppendChild($run)
        if ($section) { [void]$body.InsertBefore($p, $section) } else { [void]$body.AppendChild($p) }
    }
    Append-Paragraph '14. v0.11 연결 경로·공성·중앙 회복 오브젝트' 'Heading1'
    Append-Paragraph '14.1 맵 좌표 도식' 'Heading2'
    Append-Paragraph 'Blue Base (70,360) → Spawn (135,360) → Fork (170,360) → Blue Tower (260,y) → Red Tower (1020,y) → Merge (1110,360) → Red Base (1210,360). y는 상·중·하 순서로 180/360/540이며 Red 경로는 이를 역순으로 따른다. 도로 외곽 폭은 112 px, 안쪽 폭은 101 px이다.'
    Append-Paragraph '14.2 미니언과 기지 공성' 'Heading2'
    Append-Paragraph '12초마다 진영별·라인별 2기, 총 12기가 새로 생성된다. Minion의 경로 타깃은 자기 라인의 살아 있는 적 Tower → 적 Nexus이다. Tower 70 px, Nexus 60 px 안에서 멈춰 1초마다 8 피해를 주고, 죽은 타깃 이후에는 다음 웨이포인트로 진행한다. Enemy Hero·Enemy Minion·아군 구조물은 공격하지 않는다. Hero 투사체는 기존처럼 Tower 생존 여부와 무관하게 Base를 공격할 수 있다. Base HP 0은 기존 10ms 동시 파괴 Draw 판정을 따른다.'
    Append-Paragraph '14.3 타워 공격과 Bot 경로' 'Heading2'
    Append-Paragraph '양 팀 Tower는 260 px 이내 같은 라인 Enemy Minion을 우선 선택하고, 없으면 Enemy Hero를 선택한다. 1초마다 15 피해를 즉시 적용하며 같은 갱신의 Snapshot에 진영색 탄도선과 점 효과를 0.18초 표시한다. 표적이 없으면 공격하지 않는다. Bot Push는 Red Base 합류점에서 선택 라인 Tower를 거쳐 Blue Base 방향으로 이동한다. Retreat는 선택한 자기 Tower의 Base쪽 70 px 방어점으로 이동하고 Tower 파괴 시 자기 Base로 후퇴하며, 안전 구역 Recover 4 HP/s 및 55% 재진입을 유지한다.'
    Append-Paragraph '14.4 중앙 회복 오브젝트' 'Heading2'
    Append-Paragraph '센터 (640,360)에 한 개가 경기 시작 20초 후 생성된다. 획득 30초 후 재생성하며 양 팀에 항상 보인다. 살아 있는 HP 미만 Hero만 28 px 이내 접촉으로 HP 20을 최대치 이하로 회복한다. 같은 틱에 둘 다 닿으면 ID 0 Player가 ID 1 Bot보다 먼저 획득한다. Minion·Projectile·Tower와 풀 HP Hero는 소비하지 않는다. Bot은 HP 31~50%, 거리 300 px 이내, 오브젝트 활성 시 한 생성 주기에 한 번만 최대 2.5초 추격하며 Retreat/Recover를 방해하지 않는다.'
    Append-Paragraph '14.5 회귀 검증' 'Heading2'
    Append-Paragraph 'TC G01~G17과 Bot visibility/Basic damage를 유지한다. 추가 테스트는 양 진영×상·중·하 6경로의 Tower 정지·반복 피해·Nexus 승패, 양 진영 Tower의 15 피해와 Snapshot 효과/무표적, 중앙 회복 20/30초·동시 획득·풀 HP·Minion 비획득을 검사한다. Debug/Release x64 빌드와 실제 GUI에서 연결 도로·타워 발사·중앙 오브젝트를 확인한다. 최종 밸런스는 사람의 직접 플레이 후 판단한다.'
    $entry.Delete()
    $newEntry = $archive.CreateEntry('word/document.xml')
    $writer = [System.IO.StreamWriter]::new($newEntry.Open(), [System.Text.UTF8Encoding]::new($false))
    try { $xml.Save($writer) } finally { $writer.Dispose() }
} finally { $archive.Dispose() }

