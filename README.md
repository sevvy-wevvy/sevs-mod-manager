<h1 align="center">Sevs Mod Manager</h1>

<p align="center">
<b>A mod manager that works for nearly all unity games.</b><br>
Rewritten in C on <a href="https://github.com/goome-ui/c-goome">goome</a>. Linux, Windows and macOS, one small file, nothing else to install.<br>
Add your own mods to the SB/SMM library at <a href=https://sevvy-wevvy.com/mods/sb/dashboard/>sevvy-wevvy.com/mods/sb/dashboard</a>!
</p>

<div align="center">
 <img src="https://github.com/sevvy-wevvy/sevs-mod-manager/blob/dev/prev.png?raw=true" width=70% height=auto</img>
</div><br>

<p align="center">
  <a href="https://github.com/sevvy-wevvy/sevs-mod-manager/releases">
    <img src="https://img.shields.io/github/downloads/sevvy-wevvy/sevs-mod-manager/total?label=Downloads&style=for-the-badge" alt="Downloads">
  </a>
  <a href="https://sevvy-wevvy.com/discord">
    <img src="https://img.shields.io/discord/1398944180079689728?label=Discord&style=for-the-badge" alt="Discord">
  </a>
  <a href="https://github.com/goome-ui"><img src="https://img.shields.io/badge/made%20with-goome-6fdc5f?style=for-the-badge" alt="made with goome"></a>
</p>

<details open>
<summary align="center"><b>⬇️ Install ⬇️</b></summary>

<h3 align="center">🐧 Linux &nbsp;·&nbsp; 🍎 Mac</h3>

```sh
curl -fsSL https://raw.githubusercontent.com/sevvy-wevvy/sevs-mod-manager/dev/install.sh | sh
```

<h3 align="center">🪟 Windows (PowerShell, not cmd)</h3>

```powershell
irm https://raw.githubusercontent.com/sevvy-wevvy/sevs-mod-manager/dev/install.ps1 | iex
```

<h3 align="center">🏹 Arch Linux</h3>

The line above works on Arch too. If you want pacman to own it, build the package from source:

```sh
mkdir smm && cd smm && curl -fsSLO https://raw.githubusercontent.com/sevvy-wevvy/sevs-mod-manager/dev/packaging/PKGBUILD && makepkg -si
```

<p align="center">
Run it again any time to update. Linux and Mac put it in <code>~/.local/bin/smm</code>, Windows in <code>%LOCALAPPDATA%\SevsModManager</code> with a start menu entry.<br>
It needs <code>curl</code> (already on Windows 10+, Mac and nearly every Linux). <code>zenity</code> or <code>kdialog</code> gives you file pickers on Linux.<br>
Windows may show a SmartScreen warning because the exe isn't signed, click More info then Run anyway.
</p>

</details>

<details>
<summary align="center"><b>🛠️ Build It Yourself 🛠️</b></summary>

<p align="center">You only need a C compiler.</p>

```sh
git clone -b dev https://github.com/sevvy-wevvy/sevs-mod-manager
cd sevs-mod-manager
./build.sh
./build/smm
```

<p align="center">
<code>./build.sh test</code> runs the tests, <code>./build.sh win</code> makes the Windows exe with mingw.<br>
<code>SMM_DEMO=1 ./build/smm</code> opens it with fake games and mods and no network.
</p>

</details>

<details>
<summary align="center"><b>📃 Features 📃</b></summary>

<b>Mods</b><br>
Browse and download mods from Thunderstore<br>
Browse, download, and upvote mods from the official SB API<br>
Search and filter mods (All / Featured / Verified / Unverified tabs)<br>
Gorilla Tag: browse and install from <a href="https://monkeforge.org">MonkeForge</a> too, every download is checked against its SHA-256<br>
Warns before installing unverified mods<br>
Auto-installs a mod's dependencies and the right mod loader (BepInEx or MelonLoader)<br>
Install your own DLLs, or drag and drop them onto the window<br><br>

<b>Installed</b><br>
Enable, disable, update and delete mods, update everything in one click<br>
Conflict detection when two mods ship the same file<br><br>

<b>Loadouts, Modpacks and Config</b><br>
Save and swap loadouts, share modpacks as <code>.smmpack</code> files<br>
Edit BepInEx and MelonLoader configs in the app<br><br>

<b>Logs</b><br>
Live game log, your last 10 sessions, search and filters<br>
"Possible culprit" banner when a mod crashes the game<br><br>

<b>Games</b><br>
Finds your Steam games on every OS, Proton included, and quick-switches between them<br>
Real game art for the games you manage<br>
Three layouts (Sev's Mod Manager, r2modman and MonkeModManager style) and five themes, all switch live without a restart<br>

</details>

<p align="center">
This is a full rewrite of the old C# app in C. Discord rich presence was dropped, and the old C# code is still in the git history before the rewrite.
</p>
