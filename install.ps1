& {
$ErrorActionPreference = 'Stop'
$repo = 'sevvy-wevvy/sevs-mod-manager'
$tag = if ($env:SMM_TAG) { $env:SMM_TAG } else { 'dev-latest' }
$dir = Join-Path $env:LOCALAPPDATA 'SevsModManager'
New-Item -ItemType Directory -Force $dir | Out-Null
Write-Host '> downloading smm-windows-x64.exe' -ForegroundColor Green
Invoke-WebRequest -UseBasicParsing "https://github.com/$repo/releases/download/$tag/smm-windows-x64.exe" -OutFile "$dir\smm.exe.part"
Move-Item -Force "$dir\smm.exe.part" "$dir\smm.exe"
$lnk = Join-Path ([Environment]::GetFolderPath('Programs')) 'Sevs Mod Manager.lnk'
$sh = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$sh.TargetPath = "$dir\smm.exe"
$sh.Save()
Write-Host "> installed to $dir, find it in the start menu as Sevs Mod Manager" -ForegroundColor Green
}
