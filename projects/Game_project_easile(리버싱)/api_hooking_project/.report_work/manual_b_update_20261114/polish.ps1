$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$path=Join-Path $PSScriptRoot 'EASILE_HK01_공격방어_최종보고서.docx'
$zip=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Update)
$r=[IO.StreamReader]::new($zip.GetEntry('word/document.xml').Open());[xml]$d=$r.ReadToEnd();$r.Dispose()
$u='http://schemas.openxmlformats.org/wordprocessingml/2006/main';$n=[Xml.XmlNamespaceManager]::new($d.NameTable);$n.AddNamespace('w',$u)
foreach($m in $d.SelectNodes('//w:tcMar/w:top | //w:tcMar/w:bottom',$n)){$null=$m.SetAttribute('w',$u,'60')}
foreach($t in $d.SelectNodes('//w:body/w:tbl',$n)){
 $p=@($t.SelectNodes('.//w:p',$n)); for($i=0;$i -lt ($p.Count-1);$i++){$pr=$p[$i].SelectSingleNode('w:pPr',$n);if(!$pr){$pr=$d.CreateElement('w','pPr',$u);$null=$p[$i].PrependChild($pr)};$k=$pr.SelectSingleNode('w:keepNext',$n);if(!$k){$k=$d.CreateElement('w','keepNext',$u);$null=$pr.AppendChild($k)};$null=$k.SetAttribute('val',$u,'1')}
}
foreach($p in $d.SelectNodes('//w:body/w:p',$n)){
 $heading=(($p.SelectNodes('.//w:t',$n)|ForEach-Object InnerText)-join ''); if($heading.StartsWith('10  ') -or $heading.StartsWith('19  ')){$pr=$p.SelectSingleNode('w:pPr',$n); if(!$pr){$pr=$d.CreateElement('w','pPr',$u);$null=$p.PrependChild($pr)};if(!$pr.SelectSingleNode('w:pageBreakBefore',$n)){$null=$pr.AppendChild($d.CreateElement('w','pageBreakBefore',$u))}}
 $style=$p.SelectSingleNode('w:pPr/w:pStyle',$n)
 $text=(($p.SelectNodes('.//w:t',$n)|ForEach-Object InnerText)-join '')
 if($text.StartsWith('기존 설치·탐지 기록') -or $text.StartsWith('직접 플레이 확인') -or $text.StartsWith('기존 실게임 A/C') -or $text.StartsWith('기존 독립 B 경계')){
  $pr=$p.SelectSingleNode('w:pPr',$n);if(!$pr){$pr=$d.CreateElement('w','pPr',$u);$null=$p.PrependChild($pr)};foreach($old in @($pr.SelectNodes('w:pStyle | w:jc',$n))){$null=$pr.RemoveChild($old)};$s=$d.CreateElement('w','pStyle',$u);$null=$s.SetAttribute('val',$u,'Normal');$null=$pr.PrependChild($s)
  foreach($r in $p.SelectNodes('w:r',$n)){$rp=$r.SelectSingleNode('w:rPr',$n);if($rp){$null=$r.RemoveChild($rp)}}
 }
}
foreach($t in $d.SelectNodes('//w:t',$n)){
 if($t.InnerText.StartsWith('followup_20261114/verification_')){$t.InnerText='hk01_retest/'+$t.InnerText}
 $t.InnerText=$t.InnerText.Replace('현재 C 시험에서는','기존 C 시험에서는')
}
$zip.GetEntry('word/document.xml').Delete();$s=$zip.CreateEntry('word/document.xml').Open();$d.Save($s);$s.Dispose();$zip.Dispose()
