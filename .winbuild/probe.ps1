Write-Output ('ARCH=' + (Get-CimInstance Win32_Processor).Architecture)
Write-Output ('OS=' + (Get-CimInstance Win32_OperatingSystem).Caption)
foreach ($c in 'cmake','git','cl','ninja','winget','python') { $x = Get-Command $c -ErrorAction SilentlyContinue; if ($x) { Write-Output ('CMD ' + $c + '=' + $x.Source) } }
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ('VSROOT=' + $_.FullName) }
Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio' -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ('VSROOT86=' + $_.FullName) }
