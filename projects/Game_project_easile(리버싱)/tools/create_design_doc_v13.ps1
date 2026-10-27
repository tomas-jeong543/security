param(
    [string]$SourcePath = 'docs/EASILE_2D게임_및_보안반복실험_설계서_v0.12.docx',
    [string]$OutputPath = 'docs/EASILE_2D게임_및_보안반복실험_설계서_v0.13.docx',
    [string]$DiagramPath = 'docs/easile_v013_wall_plan.png'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$source = (Resolve-Path -LiteralPath $SourcePath).Path
$output = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
$diagram = [IO.Path]::GetFullPath((Join-Path (Get-Location) $DiagramPath))
$bmp = [Drawing.Bitmap]::new(1280,720)
$g = [Drawing.Graphics]::FromImage($bmp)
try {
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([Drawing.Color]::FromArgb(28,34,44))
    foreach ($width in @(140,132)) {
        $color = if($width -eq 140){[Drawing.Color]::FromArgb(125,133,138)}else{[Drawing.Color]::FromArgb(60,70,80)}
        $pen = [Drawing.Pen]::new($color,$width)
        try {
            $pen.StartCap = $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
            $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
            foreach ($y in @(180,360,540)) {
                $points = [Drawing.Point[]]@([Drawing.Point]::new(70,360),[Drawing.Point]::new(170,360),[Drawing.Point]::new(260,$y),[Drawing.Point]::new(1020,$y),[Drawing.Point]::new(1110,360),[Drawing.Point]::new(1210,360))
                $g.DrawLines($pen,$points)
            }
        } finally {$pen.Dispose()}
    }
    $wallEdge = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(125,133,138))
    $wallFill = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(20,26,36))
    try {
        foreach($y in @(270,450)) {
            $g.FillRectangle($wallEdge,300,$y-20,680,40)
            $g.FillRectangle($wallFill,302,$y-18,676,36)
        }
    } finally {$wallEdge.Dispose();$wallFill.Dispose()}
    foreach($y in @(180,360,540)) {
        $g.FillEllipse([Drawing.Brushes]::DodgerBlue,245,$y-15,30,30)
        $g.FillEllipse([Drawing.Brushes]::Tomato,1005,$y-15,30,30)
    }
    $g.FillEllipse([Drawing.Brushes]::DodgerBlue,47,337,46,46)
    $g.FillEllipse([Drawing.Brushes]::Tomato,1187,337,46,46)
    $g.FillEllipse([Drawing.Brushes]::Gold,630,350,20,20)
    $g.FillRectangle([Drawing.Brushes]::ForestGreen,410,140,180,80)
    $g.FillRectangle([Drawing.Brushes]::ForestGreen,690,500,180,80)
    $g.FillEllipse([Drawing.Brushes]::Orange,1042,352,16,16)
    $patrol=[Drawing.Pen]::new([Drawing.Color]::Orange,4)
    try {$g.DrawLine($patrol,1032,384,1060,384)} finally {$patrol.Dispose()}
    $font=[Drawing.Font]::new('Arial',17)
    try {
        $g.DrawString('Blue Base (70,360)',$font,[Drawing.Brushes]::White,10,20)
        $g.DrawString('Heal (640,360)',$font,[Drawing.Brushes]::White,525,20)
        $g.DrawString('Red Base (1210,360)',$font,[Drawing.Brushes]::White,1000,20)
        $g.DrawString('Walls: x=300..980, y=250..290 / 430..470',$font,[Drawing.Brushes]::White,385,55)
        $g.DrawString('Fork (170,360)',$font,[Drawing.Brushes]::White,100,625)
        $g.DrawString('Merge (1110,360)',$font,[Drawing.Brushes]::White,990,625)
        $g.DrawString('Bot cover: Tower + (30,0); patrol +12..+40 px',$font,[Drawing.Brushes]::White,350,655)
        $g.DrawString('Road outer 140 / inner 132 px; Hero radius 26 / Minion radius 12 px',$font,[Drawing.Brushes]::White,245,685)
    } finally {$font.Dispose()}
    $bmp.Save($diagram,[Drawing.Imaging.ImageFormat]::Png)
} finally {$g.Dispose();$bmp.Dispose()}

Copy-Item -LiteralPath $source -Destination $output -Force
$zip=[IO.Compression.ZipFile]::Open($output,[IO.Compression.ZipArchiveMode]::Update)
try {
    function Read-Entry([string]$name) {
        $reader=[IO.StreamReader]::new($zip.GetEntry($name).Open())
        try {return $reader.ReadToEnd()} finally {$reader.Dispose()}
    }
    [xml]$xml=Read-Entry 'word/document.xml'
    $ns=[Xml.XmlNamespaceManager]::new($xml.NameTable)
    $ns.AddNamespace('w','http://schemas.openxmlformats.org/wordprocessingml/2006/main')
    $ns.AddNamespace('a','http://schemas.openxmlformats.org/drawingml/2006/main')
    $ns.AddNamespace('r','http://schemas.openxmlformats.org/officeDocument/2006/relationships')
    function Text-Of($p) {return (($p.SelectNodes('.//w:t',$ns) | ForEach-Object {$_.InnerText}) -join '')}
    function Replace-Text($p,[string]$text) {
        $run=$p.SelectSingleNode('w:r',$ns)
        $format=if($run){$run.SelectSingleNode('w:rPr',$ns)}else{$null}
        $copy=if($format){$format.CloneNode($true)}else{$null}
        foreach($child in @($p.ChildNodes)){if($child.LocalName -ne 'pPr'){$p.RemoveChild($child)|Out-Null}}
        $run=$xml.CreateElement('w','r',$ns.LookupNamespace('w'))
        if($copy){$run.AppendChild($copy)|Out-Null}
        $t=$xml.CreateElement('w','t',$ns.LookupNamespace('w'));$t.InnerText=$text
        $run.AppendChild($t)|Out-Null;$p.AppendChild($run)|Out-Null
    }
    $replacements=[ordered]@{
        'v0.12'='v0.13'
        '15. v0.12'='15. v0.13 연결 도로·라인 사이 벽·Bot 후방 교전 (이 장이 이전 버전의 상충 규칙보다 우선)'
        '맵 1280×720.'='맵 1280×720. Base (70,360)/(1210,360), 분기·합류 (170,360)/(1110,360), 라인 y=180/360/540, Tower (260,y)/(1020,y)를 유지한다. 도로는 MapPaths.h의 공통 경로 선분 주위 반경 70 px인 캡슐의 합집합이다. 렌더러는 외곽 140 px 전체를 그린 뒤 내부 132 px 전체를 그리며 둥근 끝단을 24면으로 근사한다. 별도 삼각형 덧칠은 사용하지 않는다. Player/Bot 반경 26 px, Minion 12 px를 고려해 이동을 도로 안으로 제한하며 최대 8 px 단계와 축별 미끄러짐을 사용한다. Base 생성점, 중앙 회복 (640,360), 수풀 (500,180)/(780,540), 크기 180×80 px는 기존 위치를 유지한다.'
        '그림 5. v0.12'='그림 5. v0.13 공통 맵 데이터: 둥근 도로 외곽, 상·중/중·하 사이 두 벽, 분기·합류, Tower·Base·수풀·회복 지점 및 Bot 후방 방어점. 벽은 x=300~980, y=250~290 및 y=430~470의 두 직사각형이다. Bot 방어점은 선택 Red Tower+(30,0), 왕복 범위는 Tower.x+12~+40이며 y는 해당 라인의 중심이다.'
        '미니언: 양 진영 동일 규칙.'='미니언·타워: 양 진영에 동일한 벽 선분 판정을 적용한다. Minion은 자기 라인의 살아 있는 적 Tower가 70 px 안이고 벽으로 차단되지 않으면 먼저 공격한다. 그 외에는 같은 라인의 적 Minion을 50 px 안에서 찾고, 벽으로 막힌 후보는 제외한다. 없으면 웨이포인트를 따라 110 px/s로 전진한다. Tower 파괴 후 적 Minion이 없고 Nexus가 60 px 안이며 벽에 막히지 않을 때 공성한다. 피해 8/1초는 표적 간 공유하고 동거리는 작은 ID가 우선이다. Hero는 공격하지 않는다. Tower는 260 px 안의 벽에 막히지 않은 같은 라인 적 Minion을 우선, 없으면 벽에 막히지 않은 적 Hero를 선택한다. 피해 15/1초와 즉시 피해·0.18초 시각 효과는 유지한다. 벽을 가로지르는 탄도선과 직접 피해는 발생하지 않는다.'
        'Bot FSM: HP 30% 이하 Retreat'='Bot FSM: HP 30% 이하에서 Retreat에 진입한다. 자기 라인 생존 Red Tower+(30,0)의 후방 방어점으로 이동하고, Tower가 없으면 Red Base로 fallback한다. 다른 라인에 있으면 선택 라인 Tower와 오른쪽 합류 구간을 거쳐 이동한다. 도착 허용 거리는 10 px다. Recover에서는 같은 라인 y를 유지하며 Tower.x+12~+40의 28 px 구간을 왕복한다. 끝점 3 px 이내에서 약 0.30초 쉬고 방향을 바꾸며 목표까지 남은 거리로 이동량을 제한해 떨림과 초과 이동을 막는다. Base fallback은 Base.x-30~+20 구간을 사용한다. Tower 반경 95 px 또는 Base 반경 90 px의 안전 구역에서 기존 4 HP/s로 회복한다.'
        '검증: 길 경계와 몸체 반경'='검증: 기존 G01~G17, 도로 왕복, 양 진영 3라인 공성, 시야·피해·회복 테스트를 유지한다. 추가 검사는 양 진영 Basic/Ultimate의 상↔중·중↔하 고속 벽 충돌, 벽 앞 첫 유닛 충돌, 동일 라인·벽 끝 공격, Auto/HUD/LMB/Mouse, Tower/Minion의 벽 차단, 세 라인 저체력 Bot의 왕복·적 Minion 반복 명중·55% Push·18초 시간 제한을 다룬다. 자동 GUI는 실제 Win32/DX11 시작·전투 및 저체력 시나리오를 렌더하고 같은 라인 Blue Minion HP 40→20, 다른 라인 HP 40 유지, Bot 회복 후 Push 복귀를 확인한다. Debug/Release x64와 실행 결과는 evidence/v013-validation.md에 기록한다. 사람의 장시간 조작감·밸런스 평가는 별도이며 새 baseline 태그는 만들지 않는다.'
    }
    $paragraphs=@($xml.SelectNodes('//w:body/w:p',$ns))
    $mapParagraph=$null;$botParagraph=$null;$imageParagraph=$null
    foreach($p in $paragraphs) {
        $text=Text-Of $p
        foreach($key in $replacements.Keys) {
            if(($key -eq 'v0.12' -and $text -eq $key) -or ($key -ne 'v0.12' -and $text.StartsWith($key))) {
                Replace-Text $p $replacements[$key]
                if($key -eq '맵 1280×720.'){$mapParagraph=$p}
                if($key -eq 'Bot FSM: HP 30% 이하 Retreat'){$botParagraph=$p}
                if($key -eq '그림 5. v0.12'){$imageParagraph=$p.NextSibling}
                break
            }
        }
    }
    if(!$mapParagraph -or !$botParagraph -or !$imageParagraph){throw 'v0.12 chapter 15 anchors were not found'}
    foreach($t in $xml.SelectNodes('//w:tbl//w:t',$ns)){if($t.InnerText -eq '0.12'){$t.InnerText='0.13'}}
    function Insert-After($anchor,[string]$text) {
        $p=$mapParagraph.CloneNode($true);Replace-Text $p $text
        $anchor.ParentNode.InsertAfter($p,$anchor)|Out-Null
        return $p
    }
    $wall=Insert-After $mapParagraph '내부 벽: MapPaths.h의 walls 배열을 렌더링·몸체 이동·공격 경로·Auto 조준에서 공유한다. 벽은 중심선 (300,270)→(980,270), (300,450)→(980,450), 두께 40 px다. 캐릭터 몸체가 벽을 침범할 수 없고 직선 구간에서 라인을 직접 건너갈 수 없다. x<300 또는 x>980의 분기·합류에서는 벽 끝을 돌아 라인 변경 및 전투가 가능하다. 벽 밖의 배경 역시 이동 가능한 도로가 아니다. Bot의 라인 변경·추격·회복 오브젝트 접근은 오른쪽 연결부를 경유하며 벽을 향해 계속 밀지 않는다.'
    $null=Insert-After $wall '조준·투사체: 수풀 은폐와 벽 차단은 별개다. 수풀에서 드러나거나 화면에 보이는 상대도 Hero→표적 선분이 벽과 교차하면 K/J Auto 후보에서 제외하며 HUD에도 표시하지 않는다. 사거리·최근 조준 방향 fallback은 유지한다. LMB와 Mouse K/J는 벽 방향으로 발사할 수 있다. Basic/Ultimate는 이전→새 위치 선분을 검사하여 벽과 유닛 중 첫 충돌에서 피해 또는 소멸을 처리한다. 한 틱에 벽을 넘는 고속 탄도 통과하지 못한다. 벽 앞 유닛은 정상적으로 피해를 받는다. 현재 두 공격은 단일 투사체이며 광역 피해나 벽 관통 피해가 없다. 투사체는 도로 자체에 구속되지 않지만 두 내부 벽에는 충돌한다.'
    $shots=Insert-After $botParagraph '후방 사격: Retreat/Recover에서는 살아 있고 보이는 같은 라인의 Blue Minion 중 650 px 안의 유효 대상을 우선 선택해 Basic을 기존 1초 간격으로 발사한다. Minion은 기존 규칙대로 수풀에 은폐되지 않는다. 벽 또는 먼저 충돌하는 적 Tower/Base/다른 Minion에 막힌 목표는 제외하며, 실제 충돌 기하로 발사 가능 여부를 검사한다. 왕복 중에도 발사하고 유효 Minion이 없으면 기존의 보이는 Player에 대한 사격만 검토한다. 둘 다 없으면 발사하지 않는다. 아군 Tower의 탄환 통과 규칙은 기존대로이며 적 구조물·벽을 관통하게 변경하지 않는다. Retreat/Recover에서는 Ultimate를 발사하지 않고 정상 상태의 기존 Ultimate 조건과 피해·쿨다운을 유지한다.'
    $null=Insert-After $shots '복귀·예외: HP 55% 이상이면 즉시 Push로 돌아간다. Recover가 18초를 넘으면 임시 Push로 전환하고 3초간 재후퇴를 제한해 영구 후방 왕복과 경계 진동을 방지한다. 안전 구역을 잃으면 새 Tower/Base 목표로 Retreat하며, 사망·부활·Restart 시 회복 누적·왕복·시간 제한 상태를 초기화한다. 회복 오브젝트 추격은 기존 HP 31~50%, 300 px, 생성 주기당 한 번·최대 2.5초 조건을 유지하며 Retreat/Recover를 방해하지 않는다. 기존 Hero/Minion/Tower 피해량, Basic 주기, Ultimate 5초, 승패·동시 파괴 판정은 바꾸지 않는다.'
    function Set-WordProperty($parent,[string]$name,[hashtable]$attributes) {
        $node=$parent.SelectSingleNode('w:'+$name,$ns)
        if(!$node){$node=$xml.CreateElement('w',$name,$ns.LookupNamespace('w'));$parent.AppendChild($node)|Out-Null}
        foreach($key in $attributes.Keys){$node.SetAttribute($key,$ns.LookupNamespace('w'),[string]$attributes[$key])|Out-Null}
    }
    $active=$false
    foreach($p in $xml.SelectNodes('//w:body/w:p',$ns)) {
        $text=Text-Of $p
        $heading=$text.StartsWith('15. v0.13')
        if($heading){$active=$true}
        if(!$active){continue}
        $pp=$p.SelectSingleNode('w:pPr',$ns)
        if(!$pp){$pp=$xml.CreateElement('w','pPr',$ns.LookupNamespace('w'));$p.PrependChild($pp)|Out-Null}
        Set-WordProperty $pp 'keepLines' @{val=1}
        Set-WordProperty $pp 'spacing' @{after=120;line=276;lineRule='auto'}
        if($heading){
            Set-WordProperty $pp 'pageBreakBefore' @{val=1}
            Set-WordProperty $pp 'keepNext' @{val=1}
        }
        if($text.StartsWith('그림 5.')){Set-WordProperty $pp 'keepNext' @{val=1}}
        foreach($run in $p.SelectNodes('w:r',$ns)) {
            if(!$run.SelectSingleNode('w:t',$ns)){continue}
            $rp=$run.SelectSingleNode('w:rPr',$ns)
            if(!$rp){$rp=$xml.CreateElement('w','rPr',$ns.LookupNamespace('w'));$run.PrependChild($rp)|Out-Null}
            Set-WordProperty $rp 'b' @{val=([int]$heading)}
            Set-WordProperty $rp 'sz' @{val=$(if($heading){28}else{22})}
        }
    }
    $blip=$imageParagraph.SelectSingleNode('.//a:blip',$ns)
    if(!$blip){throw 'Map figure relationship was not found'}
    $imageId=$blip.GetAttribute('embed',$ns.LookupNamespace('r'))
    [xml]$rels=Read-Entry 'word/_rels/document.xml.rels'
    $target=($rels.DocumentElement.ChildNodes | Where-Object {$_.Id -eq $imageId}).Target
    $entry=$zip.GetEntry('word/'+$target)
    if(!$entry){throw 'Map image ZIP entry was not found'}
    $entry.Delete();$entry=$zip.CreateEntry('word/'+$target)
    $stream=$entry.Open();try {$bytes=[IO.File]::ReadAllBytes($diagram);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    $zip.GetEntry('word/document.xml').Delete()
    $entry=$zip.CreateEntry('word/document.xml');$stream=$entry.Open()
    $writer=[Xml.XmlWriter]::Create($stream,[Xml.XmlWriterSettings]@{Encoding=[Text.UTF8Encoding]::new($false);Indent=$false})
    try {$xml.Save($writer)}finally{$writer.Dispose();$stream.Dispose()}
} finally {$zip.Dispose()}
Write-Output "Updated v0.13: $output"
