param(
    [string]$DocumentPath = 'docs\EASILE_2D게임_및_보안반복실험_설계서_v0.11.docx',
    [string]$ImagePath = 'build\Release\visual-smoke.bmp'
)

$document = (Resolve-Path -LiteralPath $DocumentPath).Path
$picture = (Resolve-Path -LiteralPath $ImagePath).Path
$word = New-Object -ComObject Word.Application
$word.Visible = $false
try {
    $file = $word.Documents.Open($document, $false, $false)
    try {
        $selection = $word.Selection
        $selection.EndKey(6) | Out-Null
        $selection.TypeParagraph()
        $selection.TypeText('그림 4. v0.11 실제 연결 맵: 양쪽 Base 분기·합류, 라인별 Tower와 중앙 지점을 표시한다.')
        $selection.TypeParagraph()
        $shape = $selection.InlineShapes.AddPicture($picture, $false, $true)
        $shape.LockAspectRatio = -1
        $shape.Width = 450
        $file.Save()
        Write-Output "Word opened and saved v0.11; pages=$($file.ComputeStatistics(2)); image_width_pt=$($shape.Width)"
    } finally { $file.Close($false) }
} finally { $word.Quit() }

