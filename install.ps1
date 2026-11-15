& {
$ErrorActionPreference = 'Stop'
$repo = 'sevvy-wevvy/sevs-mod-manager'
$base = if ($env:SMM_TAG) { "https://github.com/$repo/releases/download/$($env:SMM_TAG)" } else { "https://github.com/$repo/releases/latest/download" }
$dir = Join-Path $env:LOCALAPPDATA 'SevsModManager'
New-Item -ItemType Directory -Force $dir | Out-Null
Write-Host '> downloading smm-windows-x64.exe' -ForegroundColor Green
Invoke-WebRequest -UseBasicParsing "$base/smm-windows-x64.exe" -OutFile "$dir\smm.exe.part"
Move-Item -Force "$dir\smm.exe.part" "$dir\smm.exe"
& "$dir\smm.exe" --register
Write-Host "> installed to $dir, find it in the start menu as Sevs Mod Manager" -ForegroundColor Green
}
