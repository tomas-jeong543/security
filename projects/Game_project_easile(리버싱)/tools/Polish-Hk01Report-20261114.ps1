$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$run=(Get-Content -Encoding UTF8 'evidence/hk01-report-latest.txt').Trim()
$report=(Get-ChildItem 'api_hooking_project' -Filter '*간결본.docx').FullName
if(!(Test-Path -LiteralPath "$run/report-before-layout.docx")){Copy-Item -LiteralPath $report -Destination "$run/report-before-layout.docx"}
$z=[IO.Compression.ZipFile]::OpenRead($report);$r=[IO.StreamReader]::new($z.GetEntry('word/document.xml').Open());[xml]$x=$r.ReadToEnd();$r.Close();$z.Dispose()
$ns=[Xml.XmlNamespaceManager]::new($x.NameTable);$ns.AddNamespace('w','http://schemas.openxmlformats.org/wordprocessingml/2006/main')
$tables=$x.SelectNodes('//w:body/w:tbl',$ns)
function Text-Cell($t,$row,$col,$text){
 $cell=$tables[$t].SelectNodes('w:tr',$ns)[$row].SelectNodes('w:tc',$ns)[$col]
 $ts=$cell.SelectNodes('.//w:t',$ns);$ts[0].InnerText=$text
 for($j=1;$j -lt $ts.Count;$j++){$ts[$j].InnerText=''}
}
Text-Cell 1 0 0 'HK-01의 +30° 조준 변조와 DLLGuard 우회, A/B/C 방어 결과를 정리한다. A/C는 실게임 증거가 있다. B는 독립 경계 시험만 PASS이며 실게임은 미검증이다.'
Text-Cell 2 0 0 'EASILE의 발사·스캐너 함수, 기존 EXE/DLL 해시와 실행 로그를 분석한다.'
Text-Cell 2 1 0 '이번에는 자료 조사·보고서만 갱신했다. 소스 변경, CRC32 추가 개선, 서버 확장은 없다.'
Text-Cell 18 1 2 '코드 +30° / 미실측'
Text-Cell 18 3 3 'fixture B 1회 확인'
Text-Cell 18 4 3 'fixture 차단 / 실게임 미검증'
Text-Cell 18 7 3 'C: 3 / fixture B: 2'
# Allow long comparison values to wrap inside their existing cells.
foreach($n in @($tables[18].SelectNodes('.//w:noWrap',$ns))){$null=$n.ParentNode.RemoveChild($n)}
$zip=[IO.Compression.ZipFile]::Open($report,[IO.Compression.ZipArchiveMode]::Update);$zip.GetEntry('word/document.xml').Delete();$entry=$zip.CreateEntry('word/document.xml');$s=$entry.Open();$settings=[Xml.XmlWriterSettings]::new();$settings.Encoding=[Text.UTF8Encoding]::new($false);$writer=[Xml.XmlWriter]::Create($s,$settings);$x.Save($writer);$writer.Close();$s.Close();$zip.Dispose()
Write-Output 'Layout content refined'
