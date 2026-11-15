$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=$PSScriptRoot
$output=Join-Path $root 'EASILE_HK01_공격방어_최종보고서.docx'
Copy-Item -LiteralPath (Join-Path $root 'original.docx') -Destination $output
$zip=[IO.Compression.ZipFile]::Open($output,[IO.Compression.ZipArchiveMode]::Update)
$nsuri='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
function ReadXml($name){$r=[IO.StreamReader]::new($zip.GetEntry($name).Open()); try{ $d=[xml]::new(); $d.PreserveWhitespace=$true; $d.LoadXml($r.ReadToEnd()); return ,$d }finally{$r.Dispose()}}
function WriteXml($name,$d){$zip.GetEntry($name).Delete(); $e=$zip.CreateEntry($name); $s=$e.Open(); try{$d.Save($s)}finally{$s.Dispose()}}
$doc=ReadXml 'word/document.xml'
$ns=[Xml.XmlNamespaceManager]::new($doc.NameTable); $ns.AddNamespace('w',$nsuri)
function Txt($node){return (($node.SelectNodes('.//w:t',$ns)|ForEach-Object InnerText)-join '')}
function SetText($node,$text){$t=@($node.SelectNodes('.//w:t',$ns)); if(!$t.Count){throw 'Missing text'}; $t[0].InnerText=$text; for($j=1;$j -lt $t.Count;$j++){$t[$j].InnerText=''}}
function ReplacePrefix($prefix,$text){$found=@($doc.SelectNodes('//w:p',$ns)|Where-Object {(Txt $_).StartsWith($prefix)}); if($found.Count -ne 1){throw "Expected one paragraph: $prefix ($($found.Count))"}; SetText $found[0] $text}
ReplacePrefix 'HK-01의 +30°' 'HK-01의 Bot 조준 방향 +30° 변조 원리와 DLLGuard 우회, A/B/C 방어 결과를 정리한다. 2026-11-14 수동 시험에서 실제 GameClient의 HK01_B 탐지와 종료 코드 2를 확인했다. 기존 A/C 시험 및 독립 B 경계 시험은 조건을 구분해 유지한다.'
ReplacePrefix 'EASILE의 발사·스캐너' 'EASILE의 발사·스캐너 함수, 기존 EXE/DLL 해시와 실행 로그를 분석하고 수동 B 검증 결과를 반영했다. 이번 문서 수정에서는 게임·공격을 재실행하지 않았으며 소스·방어 설정·서버 코드를 변경하지 않았다.'
ReplacePrefix '기존 Debug/Release 49/49' '기존 Debug/Release 각 49/49 기록 유지. 후속 수동 실게임 B 탐지·종료 2 확인'
ReplacePrefix '기존 payload의 진입' '기존 payload의 진입 14바이트 검사는 과거 EXE용이다. 새 EXE에서는 원본 payload가 설치를 거부했다. 별도 호환 DLL의 E9 변조는 기존 자동화 시험에서 확인했으며 이번 수동 시험에서 새로 측정한 값은 아니다.'
ReplacePrefix '실제 GameClient B: 미검증.' '실제 GameClient B는 2026-11-14 수동 시험에서 탐지·종료를 확인했다. 바탕화면 GameClient.log의 HK01_B DETECTED와 실험자가 PID 12960에 대해 확인한 종료 코드 2가 근거다. 앞선 UI 초기화 실패는 과거 시도의 수집 실패로 남긴다.'
ReplacePrefix 'A/C의 우선 종료나' '기존 A/C 선행 탐지·종료와 외부 정리 77은 B 성공으로 계산하지 않는다. BoundaryFixture의 B 결과도 실제 게임 수동 결과와 별도로 유지한다. 이번 로그에서 확인된 탐지 항목은 B이며, A는 SCAN_COMPLETE를 반복 기록했다.'
ReplacePrefix 'A는 DLL 경로, B는' 'A는 DLL 경로, B는 발사 직전 코드, C는 스캐너 코드를 각각 검사한다. 수동 시험의 B 탐지 로그와 종료 코드 2는 실제 게임에서의 코드 무결성 탐지·종료를 뒷받침한다.'
ReplacePrefix 'UI 초기화 실패, 수집 없음' '11-14 수동 HK01_B 탐지·종료 2'
ReplacePrefix '미검증' '수동 확인'
ReplacePrefix '19  반복 측정 결과' '19  관측 결과와 미측정 항목'
ReplacePrefix '실게임 B 미검증' 'B 탐지·종료 2 / 각도 미측정'
ReplacePrefix 'fixture B 1회 확인' 'fixture B 1회 / 수동 B 1회'
ReplacePrefix 'fixture 차단 / 실게임 미검증' 'fixture 차단 / 실게임 효과 정량 없음'
ReplacePrefix 'C: 3 / fixture B: 2' 'C: 3 / fixture B: 2 / 수동 B: 2'
ReplacePrefix '기존 실행별 결과만 인용.' '기존 실행별 결과와 2026-11-14 수동 시험 1회를 구분해 인용했다. 이번 편집 중 게임·공격 재실행 및 반복 통계 측정은 하지 않았다.'
ReplacePrefix '기존: hk01_retest/followup' '기존: hk01_retest/followup_20261114/runs 및 verification. 수동: 바탕화면 api_hooking_v1114/GameClient.log와 실험자 PowerShell 출력. 문서 QA·원본 보존은 별도 작업 경로에 기록했다.'
ReplacePrefix '실게임 A 탐지·C 자체 종료' '기존 실게임 A 탐지·C 자체 종료 3 확인. 후속 수동 실게임 B 탐지·종료 2 확인. 독립 B 경계 시험은 별도 결과로 보존.'
ReplacePrefix '새 빌드 호환 MinHook 실제' '기존 자동화에서 호환 DLL의 E9 설치 확인. 수동 시험은 DLL 로드 출력·B 탐지·종료 2 확인. 수동 E9 바이트는 미측정.'
ReplacePrefix 'A의 경로 탐지와 C의' 'A의 경로 탐지와 C의 스캐너 코드 검사는 기존 실제 GameClient 시험에서 확인했다. B는 대상 함수 진입 전 검사와 본문 재검사를 사용한다. 수동 시험에서는 HK01_B 탐지와 종료 코드 2가 확인돼 실제 게임의 탐지·종료 근거가 추가됐다.'
ReplacePrefix 'B의 최종 판정은' '수동 시험의 로그는 파일에서 재확인했으며 PID 12960과 종료 코드 2는 실험자가 제공한 PowerShell 관측이다. 주입기의 DLL 로드 성공은 MinHook 설치 완료와 다르다. 이번 수동 결과만으로 진입 E9, detour 호출, +30° 발사 효과를 각각 확인했다고 결론 내리지 않는다.'
ReplacePrefix '최종 상태: A 실게임' '최종 상태: A 기존 실게임 탐지 확인 / B 실제 게임 수동 탐지·종료 2 확인 및 독립 경계 시험 PASS / C 기존 실게임 탐지·자체 종료 3 확인.'
ReplacePrefix '수동 후속: 별도 실행' '2026-11-14 수동 시험의 실제 로그와 세 바이너리의 SHA-256을 다시 확인했다. 종료 코드는 실험자의 관측값으로 반영했다. 이번 문서 편집에서는 게임·공격을 다시 실행하지 않았다.'
ReplacePrefix '승인된 기존 관측 수단으로' '기존 자동화의 후킹 설치·E9 관측, 과거 직접 플레이의 방향 변화 확인, 이번 수동 시험의 B 탐지·종료는 서로 다른 증거다. 호출별 각도와 최종 투사체의 연결은 아직 계측하지 않았다.'
ReplacePrefix '사람 검토: 실게임 B의' '남은 검토 항목은 호출별 각도 실측과 최종 투사체 연결, 장시간 오탐·성능, 검사 루틴·디스크 기준의 신뢰 한계다. B 탐지 성공을 이 항목들의 검증 성공으로 확대하지 않는다.'
ReplacePrefix '호환 MinHook·최종 미검증' '호환 MinHook·당시 미검증 기록'
ReplacePrefix '이번 기록·수동 절차·원본 백업' '이전 문서 완료 기록·원본 백업'

# Keep the existing history and append the new trial to its comparison table.
$body=$doc.SelectSingleNode('//w:body',$ns)
$history=@($body.SelectNodes('w:tbl',$ns)|Where-Object {(Txt $_).StartsWith('회차시험 조건')})[0]
Write-Output ('historyType='+$history.GetType().FullName); Write-Output $history.OuterXml; $row=$history.SelectNodes('w:tr',$ns)[-1].CloneNode($true)
$cells=$row.SelectNodes('w:tc',$ns)
SetText $cells[0] '05'; SetText $cells[1] '2026-11-14 실제 게임 수동 시험'; SetText $cells[2] 'HK01_B 탐지·종료 2. 수동 E9·+30° 실측 없음'
$null=$history.AppendChild($row)

# Correct the original missing section 12 without changing the content order.
foreach($p in @($body.SelectNodes('w:p',$ns))){$text=Txt $p; if($text -match '^(\d{1,2})  (.+)$'){$num=[int]$Matches[1]; $title=$Matches[2]; if($num -ge 13){$num--}; SetText $p ($num.ToString('00')+'  '+$title)}}

# Narrative material remains prose; comparison and record tables remain tables.
foreach($tbl in @($body.SelectNodes('w:tbl',$ns))){$cols=$tbl.SelectNodes('w:tblGrid/w:gridCol',$ns).Count; if($cols -eq 1){foreach($p in @($tbl.SelectNodes('.//w:tc/w:p',$ns))){$copy=$p.CloneNode($true); $null=$body.InsertBefore($copy,$tbl)}; $null=$body.RemoveChild($tbl)}}
function Paragraph($text,$style='Normal',$break=$false){$p=$doc.CreateElement('w','p',$nsuri); $pr=$doc.CreateElement('w','pPr',$nsuri); $s=$doc.CreateElement('w','pStyle',$nsuri); $s.SetAttribute('val',$nsuri,$style); $null=$pr.AppendChild($s); if($style -ne 'Normal'){$null=$pr.AppendChild($doc.CreateElement('w','keepNext',$nsuri))}; if($break){$null=$pr.AppendChild($doc.CreateElement('w','pageBreakBefore',$nsuri))}; $null=$p.AppendChild($pr); $r=$doc.CreateElement('w','r',$nsuri); $t=$doc.CreateElement('w','t',$nsuri); $t.InnerText=$text; $null=$r.AppendChild($t); $null=$p.AppendChild($r); return ,$p}
$anchor=@($body.SelectNodes('w:p',$ns)|Where-Object {(Txt $_) -eq 'VALIDATION  /  RESULTS'})[0]
$manual=@(
@('MANUAL VALIDATION  /  2026-11-14','Kicker',$true),
@('실제 GameClient 방어 B 수동 검증','PageTitle',$false),
@('16.1  실행 조건과 파일 식별','Subhead',$false),
@('실험자 정명훈은 2026-11-14 실제 A/B/C 방어 적용 GameClient.exe에 기존 injector.exe로 main_spawn.dll을 주입해 SpawnProjectile 후킹을 시도했다. 대상 PID는 12960이다. 실행 폴더는 아래와 같다.','Normal',$false),
@('C:\Users\정명훈\OneDrive\Desktop\api_hooking_v1114','CodeSmall',$false),
@('다음 SHA-256은 해당 폴더의 실제 파일에서 다시 계산했으며 제공된 값과 모두 일치했다. 동일 해시는 파일 식별 근거이며 클라이언트 신뢰 인증은 아니다.','Normal',$false),
@('GameClient.exe  SHA-256','Subhead',$false),
@('3027C3DA7E340DA1A6BA1D853315BECB73FF26290B59156C4F435A17779C8153','CodeSmall',$false),
@('main_spawn.dll  SHA-256','Subhead',$false),
@('CCD4020669ECF5381355162284CD4AE30660B0E42817DAD4781A3F3E6A9CCDB9','CodeSmall',$false),
@('injector.exe  SHA-256','Subhead',$false),
@('6B6B69517A1834A039B41FB52B726B68A723C573011B3470322B4FE123B490A7','CodeSmall',$false),
@('16.2  주입 출력과 핵심 탐지 증거','Subhead',$false),
@('& (Join-Path $testFolder ''injector.exe'') $game.Id (Join-Path $testFolder ''main_spawn.dll'') ''-''','CodeSmall',$false),
@('[*] Target PID: 12960','CodeSmall',$false),
@('[+] DLL loaded in target process. Hook installation is a separate step.','CodeSmall',$false),
@('[+] Single-DLL injection complete.','CodeSmall',$false),
@('위 명령과 출력은 실험자가 제공한 관측이다. DLL 로드 성공을 뜻하며 후킹 설치 완료나 detour 호출을 독립적으로 증명하지 않는다. 실제 GameClient.log에서는 다음 탐지 행을 재확인했다.','Normal',$false),
@('[wall=1794658074][simulation=0.000][SECURITY][DETECTED][HK01_B][SpawnProjectile_modified_or_unreadable]','CodeSmall',$false),
@('$game.Refresh(); $game.ExitCode  →  2','CodeSmall',$false),
@('종료 코드 2는 실험자가 같은 게임 프로세스에서 확인한 PowerShell 결과다. B 탐지 로그와 이 관측을 함께 보면 실제 게임에서 SpawnProjectile 코드 무결성 이상을 탐지하고 종료 코드 2로 종료한 것으로 판단할 수 있다. 이번 편집에서 종료 코드를 새로 측정한 것은 아니다.','Normal',$false),
@('16.3  A B C 상태와 해석 범위','Subhead',$false),
@('파일 로그의 B 기준은 RVA 0xF670, CRC32 26013CA4이며 C 기준 RVA는 0x1B050, watchdog 주기는 100ms다. A는 SCAN_COMPLETE를 반복 기록했다. 이번 로그의 실제 탐지 항목은 HK01_B이며 A/C 탐지 성공을 이번 실행의 결과로 추가하지 않는다.','Normal',$false),
@('이번 수동 시험에서는 MinHook 설치 후 진입부 E9 바이트와 +30° 발사 각도를 직접 계측하지 않았다. 기존 자동화의 E9 확인과 과거 직접 플레이의 방향 변화 확인은 별도 근거다. B의 탐지·종료 성공은 실제 게임에서 +30°로 발사된 최종 투사체를 실측했다는 뜻이 아니다.','Normal',$false)
)
foreach($item in $manual){$null=$body.InsertBefore((Paragraph $item[0] $item[1] $item[2]),$anchor)}

# Add the manual log as a distinct evidence record.
$evidence=@($body.SelectNodes('w:tbl',$ns)|Where-Object {(Txt $_).StartsWith('번호증거 / 산출물')})[0]
$row=$evidence.SelectNodes('w:tr',$ns)[-1].CloneNode($true); $cells=$row.SelectNodes('w:tc',$ns)
SetText $cells[0] 'E-07'; SetText $cells[1] '11-14 수동 B 로그·바이너리'; SetText $cells[2] 'C:\Users\정명훈\OneDrive\Desktop\api_hooking_v1114 (16.1~16.3절)'; $null=$evidence.AppendChild($row)

# Uniform table borders, no row splitting, repeated headers; keep automatic height.
foreach($tbl in @($body.SelectNodes('w:tbl',$ns))){$pr=$tbl.SelectSingleNode('w:tblPr',$ns); $b=$doc.CreateElement('w','tblBorders',$nsuri); foreach($side in 'top','left','bottom','right','insideH','insideV'){$e=$doc.CreateElement('w',$side,$nsuri); $e.SetAttribute('val',$nsuri,'single'); $e.SetAttribute('sz',$nsuri,'4'); $e.SetAttribute('color',$nsuri,'D9D9D9'); $null=$b.AppendChild($e)}; $null=$pr.AppendChild($b); foreach($row in $tbl.SelectNodes('w:tr',$ns)){$rp=$row.SelectSingleNode('w:trPr',$ns); if(!$rp){$rp=$doc.CreateElement('w','trPr',$nsuri); $null=$row.PrependChild($rp)}; $null=$rp.AppendChild($doc.CreateElement('w','cantSplit',$nsuri)); foreach($h in @($rp.SelectNodes('w:trHeight',$ns))){$null=$rp.RemoveChild($h)}}; foreach($border in $tbl.SelectNodes('.//w:tcBorders/*',$ns)){$border.SetAttribute('color',$nsuri,'D9D9D9')}}
# Remove empty spacer paragraphs which otherwise create unnecessary page fragments.
foreach($p in @($body.SelectNodes('w:p',$ns))){if(!(Txt $p) -and !$p.SelectSingleNode('.//w:drawing | .//w:fldChar | .//w:br',$ns)){$null=$body.RemoveChild($p)}}

$styles=ReadXml 'word/styles.xml'; $sn=[Xml.XmlNamespaceManager]::new($styles.NameTable); $sn.AddNamespace('w',$nsuri)
foreach($font in $styles.SelectNodes('//w:rFonts',$sn)){$font.SetAttribute('ascii',$nsuri,'맑은 고딕'); $font.SetAttribute('hAnsi',$nsuri,'맑은 고딕'); $font.SetAttribute('eastAsia',$nsuri,'맑은 고딕')}
foreach($font in $doc.SelectNodes('//w:rFonts',$ns)){$font.SetAttribute('ascii',$nsuri,'맑은 고딕'); $font.SetAttribute('hAnsi',$nsuri,'맑은 고딕'); $font.SetAttribute('eastAsia',$nsuri,'맑은 고딕')}
foreach($id in 'Normal','PageTitle','Kicker','Subhead','SubtitleSmall','Title'){ $s=$styles.SelectSingleNode("//w:style[@w:styleId='$id']",$sn); if($s){foreach($c in $s.SelectNodes('w:rPr/w:color',$sn)){$c.SetAttribute('val',$nsuri,'000000')}} }
# Use the Title style for the main report title; keep the reference's type scale.
$p=$body.SelectNodes('w:p',$ns)[1]; $p.SelectSingleNode('w:pPr/w:pStyle',$ns).SetAttribute('val',$nsuri,'Title')
$title=$styles.SelectSingleNode("//w:style[@w:styleId='Title']",$sn); $pt=$styles.SelectSingleNode("//w:style[@w:styleId='PageTitle']",$sn); if($title){foreach($name in 'pPr','rPr'){$old=$title.SelectSingleNode('w:'+$name,$sn); if($old){$null=$title.RemoveChild($old)}; $null=$title.AppendChild($pt.SelectSingleNode('w:'+$name,$sn).CloneNode($true))}}
$code=$styles.CreateElement('w','style',$nsuri); $code.SetAttribute('type',$nsuri,'paragraph'); $code.SetAttribute('styleId',$nsuri,'CodeSmall'); $code.InnerXml='<w:name xmlns:w="'+$nsuri+'" w:val="Code Small"/><w:pPr xmlns:w="'+$nsuri+'"><w:spacing w:before="20" w:after="50"/><w:keepLines/></w:pPr><w:rPr xmlns:w="'+$nsuri+'"><w:rFonts w:ascii="Consolas" w:hAnsi="Consolas" w:eastAsia="맑은 고딕"/><w:sz w:val="17"/><w:color w:val="000000"/></w:rPr>'; $null=$styles.DocumentElement.AppendChild($code)
$core=ReadXml 'docProps/core.xml'; $cn=[Xml.XmlNamespaceManager]::new($core.NameTable); $cn.AddNamespace('dc','http://purl.org/dc/elements/1.1/'); $cn.AddNamespace('dcterms','http://purl.org/dc/terms/'); $core.SelectSingleNode('//dc:title',$cn).InnerText='EASILE HK-01 공격 방어 최종보고서'; $core.SelectSingleNode('//dc:subject',$cn).InnerText='실제 GameClient 방어 B 수동 검증 반영'; $core.SelectSingleNode('//dc:description',$cn).InnerText='기존 공격·방어 보고서 개정본'; $core.SelectSingleNode('//dcterms:modified',$cn).InnerText=[DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
WriteXml 'word/document.xml' $doc; WriteXml 'word/styles.xml' $styles; WriteXml 'docProps/core.xml' $core
$zip.Dispose()
Write-Output $output
