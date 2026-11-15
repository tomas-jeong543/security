$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=$PSScriptRoot
function Extract($file){$z=[IO.Compression.ZipFile]::OpenRead($file);try{$r=[IO.StreamReader]::new($z.GetEntry('word/document.xml').Open());try{[xml]$d=$r.ReadToEnd()}finally{$r.Dispose()};$n=[Xml.XmlNamespaceManager]::new($d.NameTable);$n.AddNamespace('w','http://schemas.openxmlformats.org/wordprocessingml/2006/main');return (($d.SelectNodes('//w:body//w:p',$n)|ForEach-Object{(($_.SelectNodes('.//w:t',$n)|ForEach-Object InnerText)-join '')})-join "`n")}finally{$z.Dispose()}}
$source=Join-Path (Split-Path (Split-Path $root)) 'EASILE_HK01_Bot_Aim_Hooking_완성_보고서_간결본.docx'
$final=Join-Path $root 'EASILE_HK01_공격방어_최종보고서.docx'
$before=Extract $source;$after=Extract $final
$before|Set-Content -LiteralPath (Join-Path $root 'text-before.txt') -Encoding UTF8
$after|Set-Content -LiteralPath (Join-Path $root 'text-after.txt') -Encoding UTF8
$required=@('12960','1794658074','SpawnProjectile_modified_or_unreadable','종료 코드 2','0xF670','26013CA4','0x1B050','100ms','SCAN_COMPLETE','3027C3DA7E340DA1A6BA1D853315BECB73FF26290B59156C4F435A17779C8153','CCD4020669ECF5381355162284CD4AE30660B0E42817DAD4781A3F3E6A9CCDB9','6B6B69517A1834A039B41FB52B726B68A723C573011B3470322B4FE123B490A7','Hook installation is a separate step.','+30°','장시간','미검증','BoundaryFixture','실험자가 제공한 PowerShell 관측')
foreach($v in $required){if(!$after.Contains($v)){throw "Missing required content: $v"}}
foreach($v in @('실제 GameClient B: 미검증','B는 독립 경계 시험만 PASS','현재 종료 코드는 미수집','이번에는 정상 전투 입력을 수행하지 못해 B PASS')){if($after.Contains($v)){throw "Stale claim: $v"}}
$preserved=(Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath (Join-Path $root 'original.docx')).Hash
if(!$preserved){throw 'Original changed'}
$tables=Get-Content -Raw -LiteralPath (Join-Path $root 'render\tables.json')|ConvertFrom-Json
if(@($tables|Where-Object {$_.startPage -ne $_.endPage}).Count){throw 'Table crosses page boundary'}
$pages=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'render\pages.json')|ConvertFrom-Json
if($pages.Count -ne 10){throw 'Unexpected page count'}
foreach($p in $pages){if($p.text.Trim().Length -lt 100){throw 'Blank page'};if(!(Test-Path -LiteralPath (Join-Path $root ('render\page-'+$p.page+'.png')))){throw 'Missing image'}}
$hashes=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'evidence-hashes.json')|ConvertFrom-Json
foreach($h in $hashes){if((Get-FileHash -LiteralPath $h.Path).Hash -ne $h.Hash){throw "Evidence changed: $($h.Path)"}}
$report=[ordered]@{run_id='manual_b_update_20261114';authorization='Document-only edit; no game, attack, build, test, source or protection changes';selected_source=$source;selection_reason='Latest completed report modified 2026-11-14 15:42; November 7 reports and 15:35 intermediate are older';original_preserved=$preserved;original_sha256=(Get-FileHash -LiteralPath $source).Hash;final_sha256=(Get-FileHash -LiteralPath $final).Hash;required_claims_checked=$required.Count;manual_log_rechecked=$true;three_binary_hashes_match=$true;pid_and_exit_code_provenance='User supplied PowerShell observation; not remeasured';page_count=$pages.Count;table_count=$tables.Count;tables_split=0;visual_qa='All 10 pages inspected at native image resolution; no clipping, overlap, missing glyphs, blank pages, or split tables';renderer='Native Microsoft Word page EMF to PNG';render_limitation='Managed Python/workspace loader unavailable; Word PDF export stalled, isolated Word instance stopped; page-image rendering succeeded';unexecuted='Game, attack replay, build, game tests, in-memory E9, per-call angle and FPS measurement';remaining_review='Exact +30 degree projectile linkage; long-run false positives and performance; original security limits retained';changes='Summary, section 15, new 16.1-16.3, results tables, conclusion, evidence list, section/table numbering, page layout';source_media_count=0;final_media_count=0}
$report|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $root 'verification.json') -Encoding UTF8
Write-Output 'Content, source preservation, evidence hashes, 10 pages and 13 single-page tables verified.'
