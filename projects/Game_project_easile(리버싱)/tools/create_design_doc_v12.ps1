param(
    [string]$SourcePath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.11.docx',
    [string]$OutputPath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.12.docx',
    [string]$DiagramPath = 'docs\easile_v012_road_plan.png'
)

$source = (Resolve-Path -LiteralPath $SourcePath).Path
$output = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
$diagram = [IO.Path]::GetFullPath((Join-Path (Get-Location) $DiagramPath))
Add-Type -AssemblyName System.Drawing
$bmp = [Drawing.Bitmap]::new(1280, 720)
$g = [Drawing.Graphics]::FromImage($bmp)
try {
    $g.Clear([Drawing.Color]::FromArgb(28, 34, 44))
    $roadEdge = [Drawing.Pen]::new([Drawing.Color]::FromArgb(125, 133, 138), 140)
    $roadInner = [Drawing.Pen]::new([Drawing.Color]::FromArgb(60, 70, 80), 132)
    foreach ($pen in @($roadEdge, $roadInner)) {
        $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
        $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
        $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
        foreach ($y in @(180, 360, 540)) {
            $pts = [Drawing.Point[]]@([Drawing.Point]::new(70,360), [Drawing.Point]::new(170,360), [Drawing.Point]::new(260,$y), [Drawing.Point]::new(1020,$y), [Drawing.Point]::new(1110,360), [Drawing.Point]::new(1210,360))
            $g.DrawLines($pen, $pts)
        }
    }
    $font = [Drawing.Font]::new('Arial', 18)
    $white = [Drawing.Brushes]::White
    foreach ($y in @(180, 360, 540)) {
        $g.FillEllipse([Drawing.Brushes]::DodgerBlue, 245, $y-15, 30, 30)
        $g.FillEllipse([Drawing.Brushes]::Tomato, 1005, $y-15, 30, 30)
    }
    $g.FillEllipse([Drawing.Brushes]::DodgerBlue, 47, 337, 46, 46)
    $g.FillEllipse([Drawing.Brushes]::Tomato, 1187, 337, 46, 46)
    $g.FillEllipse([Drawing.Brushes]::Gold, 630, 350, 20, 20)
    $g.FillRectangle([Drawing.Brushes]::ForestGreen, 410, 140, 180, 80)
    $g.FillRectangle([Drawing.Brushes]::ForestGreen, 690, 500, 180, 80)
    $g.FillEllipse([Drawing.Brushes]::Orange, 1078, 348, 24, 24)
    $g.DrawString('Blue Base (70,360)', $font, $white, 10, 20)
    $g.DrawString('Heal (640,360)', $font, $white, 550, 20)
    $g.DrawString('Red Base (1210,360)', $font, $white, 1000, 20)
    $g.DrawString('Fork (170,360)', $font, $white, 100, 625)
    $g.DrawString('Merge (1110,360)', $font, $white, 980, 625)
    $g.DrawString('Bot cover (1090,360)', $font, $white, 980, 660)
    $g.DrawString('road 140 px outer / 132 px inner; solid body inside road', $font, $white, 150, 680)
    $bmp.Save($diagram, [Drawing.Imaging.ImageFormat]::Png)
} finally { $g.Dispose(); $bmp.Dispose() }

Copy-Item -LiteralPath $source -Destination $output
$word = New-Object -ComObject Word.Application
$word.Visible = $false
try {
    $doc = $word.Documents.Open($output, $false, $false)
    try {
        $doc.Paragraphs.Item(2).Range.Text = "v0.12`r"
        foreach ($table in $doc.Tables) {
            foreach ($row in $table.Rows) {
                if ($row.Cells.Count -ge 2 -and $row.Cells.Item(1).Range.Text -like '*문서 버전*') {
                    $row.Cells.Item(2).Range.Text = '0.12'
                }
            }
        }
        $selection = $word.Selection
        $selection.EndKey(6) | Out-Null
        $selection.TypeParagraph()
        $selection.TypeText('15. v0.12 이동 구역·미니언 교전·Bot 방어 설계 (이 장이 이전 버전의 상충 규칙보다 우선)')
        $selection.TypeParagraph()
        $selection.TypeText('맵 1280×720. 도로는 Blue/Red Base (70,360)/(1210,360), 분기/합류 (170,360)/(1110,360), y=180/360/540의 세 중심선을 잇는 캡슐 합집합이다. 외곽 지름 140 px, 내부 표시 132 px. Player/Bot의 물리 반경 26 px, Minion 12 px를 빼고 중심을 제한한다. 이동은 축별 미끄러짐 및 가장 가까운 도로점으로 투영한다. 라인 변경은 분기/합류 연결부에서만 가능하며 일반 평행 라인 사이 40 px의 분리 지대를 넘을 수 없다. 투사체는 도로에 구속되지 않는다. Base 출생점과 중앙 회복 (640,360)은 도로 안이다. 수풀 중심 (500,180)/(780,540), 크기 180×80 px는 길 안에서 접근할 수 있다.')
        $selection.TypeParagraph()
        $selection.TypeText('그림 5. v0.12 계획 도로 경계와 Base, 분기·합류, 3개 라인 Tower, 수풀, 회복 지점, Bot 중단 라인 후방 방어점. 각 타워 좌표는 (260,y)/(1020,y). 다른 라인의 Bot 방어점도 해당 Red Tower에서 Red 합류점으로 70 px 이동한 중심선상이다.')
        $selection.TypeParagraph()
        $shape = $selection.InlineShapes.AddPicture($diagram, $false, $true)
        $shape.LockAspectRatio = -1
        $shape.Width = 450
        $selection.EndKey(6) | Out-Null
        $selection.TypeParagraph()
        $selection.TypeText('미니언: 양 진영 동일 규칙. 살아 있는 자기 라인 적 Tower가 70 px 안이면 무조건 최우선으로 정지·공격한다. Tower가 멀면 사거리 50 px 안의 적 Minion과 교전하며, 없으면 기존 웨이포인트로 110 px/s 전진한다. Tower 파괴 후에도 적 Minion이 50 px 안이면 교전한다. 그렇지 않고 적 Nexus가 60 px 안이면 공성한다. 8 피해/1초를 타깃 간 공유하며 매 갱신 표적의 생존·범위를 다시 검사한다. 동거리는 작은 고유 ID가 우선이다. Hero는 타깃이 아니다. Tower의 기존 15 피해/1초와 Base 동시 파괴 판정은 유지한다.')
        $selection.TypeParagraph()
        $selection.TypeText('Bot FSM: HP 30% 이하 Retreat → 자기 라인 생존 Red Tower 뒤쪽 중심선 70 px 방어점, 타워가 없으면 Red Base. 도착 또는 안전 구역에서 Recover 4 HP/s, 55% 이상 Push. Retreat·Recover 중에도 살아 있는 Player가 Basic 사거리 650 px이고 첫 충돌이 Player이면 이동 중 또는 방어점에서 1초마다 Basic 발사. 차단되는 탄은 발사하지 않는다. Ultimate 기존 조건과 투사체 피해·속도·사거리는 유지한다. 회복 지점 추격은 2.5초/생성주기 1회이며 후퇴·회복을 방해하지 않는다. 표적이 없으면 발사하지 않고 회복 또는 Push로 복귀한다.')
        $selection.TypeParagraph()
        $selection.TypeText('검증: 길 경계와 몸체 반경, 분기에서만 라인 변경, Base/Heal/수풀 접근, 양 진영 3라인 미니언 교전→Tower→Nexus, Tower 우선순위 및 동거리 결정, Bot 후퇴점 도달·후퇴 중 명중·차단 탄 미발사·55% 재진입을 자동 테스트한다. G01~G17 회귀와 Debug/Release x64 및 실제 GUI를 확인한다. 사람의 플레이 평가는 별도이다. 새 baseline 태그는 만들지 않는다.')
        $doc.Save()
        Write-Output "Saved v0.12 design before gameplay code; pages=$($doc.ComputeStatistics(2)); diagram=$diagram"
    } finally { $doc.Close($false) }
} finally { $word.Quit() }
