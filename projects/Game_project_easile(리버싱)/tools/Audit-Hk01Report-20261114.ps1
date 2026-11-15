$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.Encoding]::UTF8
$run=(Get-Content -Encoding UTF8 'evidence/hk01-report-latest.txt').Trim()
$audit=Join-Path $run ('audit_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))
$null=New-Item -ItemType Directory -Path $audit
$report=(Get-ChildItem 'api_hooking_project' -Filter '*간결본.docx').FullName
$before=Get-Content -Raw -Encoding UTF8 "$run/preserved-before.json" | ConvertFrom-Json
$mismatches=@(foreach($f in $before){if((Get-FileHash -LiteralPath $f.path).Hash -ne $f.sha256){$f.path}})
$ErrorActionPreference='Continue'
git status --short -- . | Set-Content "$audit/after-status.txt" -Encoding UTF8
$statusExit=$LASTEXITCODE
git diff -- . | Set-Content "$audit/after.diff" -Encoding UTF8
$diffExit=$LASTEXITCODE
git diff --check -- . 2>&1 | Set-Content "$audit/after-check.txt" -Encoding UTF8
$checkExit=$LASTEXITCODE
$new=@('docs/hk01-final-report-completion-20261114.md','tools/Complete-Hk01Report-20261114.ps1','tools/Render-Hk01Report-20261114.ps1','tools/Render-Hk01WordPages-20261114.ps1','tools/Polish-Hk01Report-20261114.ps1','tools/Audit-Hk01Report-20261114.ps1','evidence/hk01-report-latest.txt')
$review=@();$i=0
foreach($file in $new){
 git diff --no-index -- /dev/null $file 2>&1 | Set-Content "$audit/new-$i.diff" -Encoding UTF8
 $code=$LASTEXITCODE
 git diff --no-index --check -- /dev/null $file 2>&1 | Set-Content "$audit/new-$i-check.txt" -Encoding UTF8
 $checkCode=$LASTEXITCODE
 $issues=@(Get-Content "$audit/new-$i-check.txt" -ErrorAction SilentlyContinue | Where-Object {$_ -match ':\d+: (trailing whitespace|new blank line|space before tab)'})
 $review+= [pscustomobject]@{path=$file;diff_exit=$code;whitespace_exit=$checkCode;whitespace_issues=$issues};$i++
}
git diff --no-index -- "$run/original-report.docx" $report 2>&1 | Set-Content "$audit/report-binary.diff" -Encoding UTF8
$binaryExit=$LASTEXITCODE
git check-ignore -v -- $report 2>&1 | Set-Content "$audit/report-ignore.txt" -Encoding UTF8
$ignoreExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Zip-Hashes($p){
 $zip=[IO.Compression.ZipFile]::OpenRead($p);$result=@{}
 foreach($entry in $zip.Entries){$stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create();$result[$entry.FullName]=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','');$sha.Dispose();$stream.Close()};$zip.Dispose();return $result
}
$old=Zip-Hashes "$run/original-report.docx";$now=Zip-Hashes $report
$changed=@(foreach($key in $old.Keys){if($now[$key] -ne $old[$key]){$key}})
$extra=@(foreach($key in $now.Keys){if(!$old.ContainsKey($key)){$key}})
$zip=[IO.Compression.ZipFile]::OpenRead($report);$rd=[IO.StreamReader]::new($zip.GetEntry('word/document.xml').Open());[xml]$xml=$rd.ReadToEnd();$rd.Close();$zip.Dispose()
$ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('w','http://schemas.openxmlformats.org/wordprocessingml/2006/main')
$text=($xml.SelectNodes('//w:t',$ns)|ForEach-Object{$_.InnerText})-join "`n"
$text | Set-Content "$audit/final-report-text.txt" -Encoding UTF8
$beforeDiff=Get-Content -Raw -Encoding UTF8 "$run/before.diff"
$afterDiff=Get-Content -Raw -Encoding UTF8 "$audit/after.diff"
$beforeOther=([regex]::Split($beforeDiff,'(?m)(?=^diff --git )') | Where-Object {$_ -notmatch 'Bot_Aim_Hooking_'}) -join ''
$afterOther=([regex]::Split($afterDiff,'(?m)(?=^diff --git )') | Where-Object {$_ -notmatch 'Bot_Aim_Hooking_'}) -join ''
$md=Get-Content -Raw -Encoding UTF8 'docs/hk01-final-report-completion-20261114.md'
$links=@(foreach($m in [regex]::Matches($md,'\]\(([^)]+)\)')){$target=Join-Path 'docs' $m.Groups[1].Value;[pscustomobject]@{target=$target;exists=(Test-Path -LiteralPath $target)}})
$images=@(Get-ChildItem -LiteralPath "$run/qa-final" -Filter 'page-*.png' -File)
$result=[ordered]@{
 approval_scope='Read-only verification, report backup/edit, documentation QA; no game/attack/source change'
 report=$report;final_sha256=(Get-FileHash -LiteralPath $report).Hash
 original_sha256=(Get-FileHash -LiteralPath "$run/original-report.docx").Hash
 original_matches_record=((Get-FileHash -LiteralPath "$run/original-report.docx").Hash -eq '149B1923CCA8B4054034552C00A580462C7A9DA4AA1EBDEAF23572EB122E5778')
 preservation_count=$before.Count;preservation_mismatches=$mismatches
 git_status_exit=$statusExit;git_diff_exit=$diffExit;git_diff_check_exit=$checkExit
 tracked_diff_unchanged=($beforeDiff -ceq $afterDiff)
 other_tracked_diff_unchanged=($beforeOther -ceq $afterOther)
 authorized_tracked_difference='Only the requested existing Word report; full tracked diff is expected to change'
 new_file_review=$review;report_binary_diff_exit=$binaryExit;report_ignore_exit=$ignoreExit
 zip_members_before=$old.Count;zip_members_after=$now.Count;changed_members=$changed;extra_members=$extra
 tables=$xml.SelectNodes('//w:body/w:tbl',$ns).Count;markdown_links=$links
 native_word_render_exit=0;final_pages=$images.Count;all_pages_visually_inspected=$true
 png_sha256=@($images | ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 real_game_B='UNVERIFIED';game_exit_code=$null;game_execution='NOT RUN due to both UI initialization failures'
 remaining='Real-game B detection + self exit 2, projectile event linkage, angle/performance/false-positive measurements'
}
$result|ConvertTo-Json -Depth 8|Set-Content "$audit/final-qa.json" -Encoding UTF8
if($mismatches.Count -or !$result.other_tracked_diff_unchanged -or $checkExit -ne 0 -or $images.Count -ne 10 -or $changed.Count -ne 1 -or $changed[0] -ne 'word/document.xml' -or $extra.Count -ne 0){throw 'Final audit failed; inspect final-qa.json'}
Write-Output ('Preserved '+$before.Count+' existing files; other tracked diff unchanged; Word pages '+$images.Count)
Write-Output ('Audit path '+$audit)
Write-Output ('Final report SHA256 '+$result.final_sha256)
