<#
.SYNOPSIS
  Build, run, test and package a Tessera game (Unreal Engine), configured by the game's tessera.json.

.DESCRIPTION
  Every command first does what's needed: finds the engine, syncs the game data if it changed, compiles the C++
  if any source (the game's or a plugin's) is newer than the build, and generates the game's assets if they're
  missing (prepare scripts).

    play      run the game (full screen; -Windowed for 1600x900); -GameArgs passes switches through
    build     compile only (-Rebuild: clean first)
    test      run self-test scenarios as separate game processes, several at once (sized to this PC, or
              -Parallel N), and print PASS / FAIL; -Scenario a,b runs only those
    package   a standalone build in <dist>\Windows (then runs it, unless -NoLaunch)
    shot      one screenshot: -Name, -At <sec>, optional -Cam "x,y,z,pitch,yaw" and -GameArgs
    shots     the documentation gallery (tessera.json shots), as JPEGs; -Only a,b
    prepare   run the asset-generation scripts (UnrealEditor-Cmd, Python)
    art       run the art generator (tessera.json art.build, Python 3 + numpy)
    sync      sync the game data now
    editor    open the project in the Unreal Editor

  tessera.json (next to the wrapper script that calls this):
    {
      "engine": "5.8",
      "project": "unreal/MyGame.uproject",
      "dist": "Dist",
      "data":    { "source": "data/game.json", "target": "unreal/Content/Data/game.json", "sync": "tools/sync.js" },
      "prepare": { "scripts": [ "unreal/Tools/make_assets.py" ], "needs": [ "unreal/Content/X/M_Sprite.uasset" ] },
      "art":     { "build": "tools/art/build.py" },
      "tests":   { "<scenario>": "<extra game switches>", ... },
      "shots":   { "out": "docs/screenshots", "list": { "<name>": { "at": 2.5, "args": "...", "png": "<file the scenario saves>" } } }
    }
  The command-line prefix and screenshot folder come from the game's Config/DefaultGame.ini [Tessera]
  (CommandPrefix, ShotFolder), the same settings the C++ reads. Self-test scenarios report through
  ATSTestRunner ("[TEST <scenario> ...] ... done", FAIL on failure).
#>
param(
    [Parameter(Mandatory = $true)] [string] $Config,
    [Parameter(Position = 0)] [ValidateSet("play", "build", "test", "package", "shot", "shots", "prepare", "art", "sync", "editor")] [string] $Command = "play",
    [switch] $Windowed,
    [switch] $Rebuild,
    [switch] $NoLaunch,
    [string[]] $GameArgs = @(),
    [string[]] $Scenario = @(),
    [string[]] $Only = @(),
    [int] $Parallel = 0,
    [string] $Name = "shot",
    [double] $At = 25,
    [string] $Cam = ""
)

$ErrorActionPreference = "Stop"
function Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Note($msg) { Write-Host "    $msg" -ForegroundColor DarkGray }
function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Split-List($list) { return @($list | ForEach-Object { $_ -split "," } | Where-Object { $_ }) }

# ------------------------------------------------------------------------------------------------
# The game
# ------------------------------------------------------------------------------------------------
if (-not (Test-Path $Config)) { Fail "No $Config" }
$Root = Split-Path -Parent (Resolve-Path $Config)
$Cfg = Get-Content $Config -Raw | ConvertFrom-Json
function In-Root($rel) { return Join-Path $Root $rel }
$Proj = In-Root $Cfg.project
$ProjDir = Split-Path -Parent $Proj
$ProjName = [System.IO.Path]::GetFileNameWithoutExtension($Proj)
$Dist = In-Root $(if ($Cfg.dist) { $Cfg.dist } else { "Dist" })

# [Tessera] settings shared with the C++.
$Prefix = "TS"; $ShotFolder = ""
$ini = Join-Path $ProjDir "Config\DefaultGame.ini"
if (Test-Path $ini) {
    $section = ""
    foreach ($line in Get-Content $ini) {
        if ($line -match '^\s*\[(.+)\]\s*$') { $section = $Matches[1]; continue }
        if ($section -ne "Tessera") { continue }
        if ($line -match '^\s*CommandPrefix\s*=\s*([^;\s]+)') { $Prefix = $Matches[1] }
        if ($line -match '^\s*ShotFolder\s*=\s*([^;]+?)\s*$') { $ShotFolder = $Matches[1] }
    }
}
if (-not $ShotFolder) { $ShotFolder = "Screenshots/$Prefix" }
$ShotDir = Join-Path $ProjDir ("Saved\" + $ShotFolder.Replace("/", "\"))

# ------------------------------------------------------------------------------------------------
# Unreal Engine
# ------------------------------------------------------------------------------------------------
$Version = $(if ($Cfg.engine) { $Cfg.engine } else { "5.8" })
$UE = $null
$launcherDat = "C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat"
if (Test-Path $launcherDat) {
    foreach ($i in (Get-Content $launcherDat -Raw | ConvertFrom-Json).InstallationList) { if ($i.AppName -eq "UE_$Version") { $UE = $i.InstallLocation } }
}
if (-not $UE) { $UE = "C:\Program Files\Epic Games\UE_$Version" }
$Editor = Join-Path $UE "Engine\Binaries\Win64\UnrealEditor.exe"
$EditorCmd = Join-Path $UE "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $Editor)) { Fail "Unreal Engine $Version not found (looked in $UE). Install it from the Epic Games Launcher." }

function Sync-Data {
    if (-not $Cfg.data) { return }
    $src = In-Root $Cfg.data.source
    $dst = In-Root $Cfg.data.target
    if ($Cfg.data.sync -and (Get-Command node -ErrorAction SilentlyContinue)) {
        node (In-Root $Cfg.data.sync)
        if ($LASTEXITCODE -ne 0) { Fail "$($Cfg.data.source) is not valid - fix it and try again." }
    } else {
        New-Item -ItemType Directory -Force (Split-Path -Parent $dst) | Out-Null
        Copy-Item $src $dst -Force
        if ($Cfg.data.sync) { Note "Node.js not found: copied the data into the game only." }
    }
}

function Build-Code([bool] $Force) {
    $dll = Join-Path $ProjDir "Binaries\Win64\UnrealEditor-$ProjName.dll"
    $need = $Force -or -not (Test-Path $dll)
    if (-not $need) {
        # Newest of the game's and the plugins' editor DLLs (a plugin-only change relinks only that plugin).
        $built = (@(Get-Item $dll) + @(Get-ChildItem (Join-Path $ProjDir "Plugins\*\Binaries\Win64\UnrealEditor-*.dll") -ErrorAction SilentlyContinue) | Measure-Object LastWriteTime -Maximum).Maximum
        $sources = @(Join-Path $ProjDir "Source") + @(Get-ChildItem (Join-Path $ProjDir "Plugins") -Directory -ErrorAction SilentlyContinue | ForEach-Object { Join-Path $_.FullName "Source" } | Where-Object { Test-Path $_ })
        $plugins = @(Get-ChildItem (Join-Path $ProjDir "Plugins\*\*.uplugin") -ErrorAction SilentlyContinue)
        $changed = @(Get-ChildItem $sources -Recurse -File) + $plugins + @(Get-Item $Proj) | Where-Object { $_.LastWriteTime -gt $built } | Select-Object -First 1
        if ($changed) { $need = $true }
    }
    if (-not $need) { Note "Code is up to date."; return }
    Step "Compiling the game code (first time: a few minutes; after that: seconds)"
    $buildArgs = @("${ProjName}Editor", "Win64", "Development", "-Project=`"$Proj`"", "-WaitMutex")
    if ($Force -and $Rebuild) { $buildArgs += "-Clean" }
    & (Join-Path $UE "Engine\Build\BatchFiles\Build.bat") @buildArgs | Out-Host
    if ($LASTEXITCODE -ne 0) { Fail "Compile failed. You need Visual Studio with the 'Game development with C++' workload. Errors are above." }
}

function Run-Prepare {
    foreach ($script in @($Cfg.prepare.scripts)) {
        if (-not $script) { continue }
        Note $script
        & $EditorCmd "$Proj" -run=pythonscript -script="$(In-Root $script)" -unattended -nosplash -nullrhi | Out-Null
    }
}

function Prepare-IfNeeded {
    if (-not $Cfg.prepare) { return }
    $missing = @($Cfg.prepare.needs) | Where-Object { $_ -and -not (Test-Path (In-Root $_)) }
    if (-not $missing) { return }
    Step "Generating the game's assets (first run only)"
    Run-Prepare
}

function Game-Args([string[]] $more) { return @("`"$Proj`"", "-game") + $more + $GameArgs }

# Several game processes at once: $jobs = ordered name -> argument list. Returns when all have exited (or timed out).
function Run-Batch($jobs, [int] $parallel, [double] $timeout) {
    $queue = [System.Collections.Generic.Queue[string]]::new([string[]]@($jobs.Keys))
    $running = @{}
    while ($queue.Count -gt 0 -or $running.Count -gt 0) {
        while ($queue.Count -gt 0 -and $running.Count -lt $parallel) {
            $n = $queue.Dequeue()
            $running[$n] = @{ Proc = (Start-Process -FilePath $Editor -ArgumentList $jobs[$n] -PassThru); Start = Get-Date }
        }
        foreach ($n in @($running.Keys)) {
            $r = $running[$n]
            if ($r.Proc.HasExited) { $running.Remove($n); Note ("{0,-10} finished in {1:n0}s" -f $n, ((Get-Date) - $r.Start).TotalSeconds) }
            elseif (((Get-Date) - $r.Start).TotalSeconds -gt $timeout) { Stop-Process -Id $r.Proc.Id -Force; $running.Remove($n); Note "$n timed out" }
        }
        Start-Sleep -Milliseconds 300
    }
}

# How many game instances this PC runs at once: ~3 CPU cores, 2.5 GB of RAM (keeping 4 GB for Windows) and
# 2 GB of video memory each; whichever runs out first sets the limit.
function Auto-Parallel([int] $count) {
    $cores = (Get-CimInstance Win32_Processor | Measure-Object NumberOfCores -Sum).Sum
    $freeGB = [math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB, 1)
    $vramGB = 0
    try {
        $vramGB = (Get-ItemProperty "HKLM:\SYSTEM\ControlSet001\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0*" -Name "HardwareInformation.qwMemorySize" -ErrorAction Stop |
            ForEach-Object { $_."HardwareInformation.qwMemorySize" } | Measure-Object -Maximum).Maximum / 1GB
    } catch {}
    if ($vramGB -le 0) { $vramGB = 4 }   # unknown: assume a modest card
    $byCpu = [math]::Floor($cores / 3)
    $byRam = [math]::Floor(($freeGB - 4) / 2.5)
    $byGpu = [math]::Floor($vramGB / 2)
    $n = [math]::Max(1, [math]::Min([math]::Min($byCpu, $byRam), [math]::Min($byGpu, $count)))
    Note ("This PC: {0} cores, {1} GB RAM free, {2:n0} GB video memory -> CPU allows {3}, RAM {4}, GPU {5}: running {6} at a time" -f $cores, $freeGB, $vramGB, $byCpu, $byRam, $byGpu, $n)
    return $n
}

# ------------------------------------------------------------------------------------------------
# Commands
# ------------------------------------------------------------------------------------------------
Step "Unreal Engine $Version"
Note $UE

if ($Command -eq "sync") { Sync-Data; exit 0 }
if ($Command -eq "editor") { Start-Process -FilePath $Editor -ArgumentList "`"$Proj`""; exit 0 }
if ($Command -eq "art") {
    if (-not $Cfg.art.build) { Fail "tessera.json has no art.build" }
    $py = $(if (Get-Command python3 -ErrorAction SilentlyContinue) { "python3" } else { "py" })
    & $py (In-Root $Cfg.art.build)
    exit $LASTEXITCODE
}

# Game data: sync when the source is newer than the game's copy.
if ($Cfg.data) {
    $src = In-Root $Cfg.data.source; $dst = In-Root $Cfg.data.target
    if (-not (Test-Path $dst) -or (Get-Item $src).LastWriteTime -gt (Get-Item $dst).LastWriteTime) { Step "Syncing game data"; Sync-Data }
}
Build-Code $Rebuild.IsPresent
if ($Command -eq "build") { exit 0 }
if ($Command -eq "prepare") { Step "Generating the game's assets"; Run-Prepare; exit 0 }
Prepare-IfNeeded

$display = @("-fullscreen")
if ($Windowed) { $display = @("-windowed", "-ResX=1600", "-ResY=900") }

switch ($Command) {
    "play" {
        Step "Starting the game (Alt+F4 to quit)"
        Start-Process -FilePath $Editor -ArgumentList (Game-Args $display)
    }
    "package" {
        $exe = Join-Path $Dist "Windows\$ProjName.exe"
        Step "Packaging a standalone build (first time: 10-20 minutes)"
        & (Join-Path $UE "Engine\Build\BatchFiles\RunUAT.bat") BuildCookRun -project="$Proj" -noP4 -platform=Win64 `
            -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="$Dist" -unattended -utf8output | Out-Host
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { Fail "Packaging failed - see the output above." }
        Note $exe
        if (-not $NoLaunch) { Step "Starting $exe"; Start-Process -FilePath $exe -ArgumentList ($display + $GameArgs) }
    }
    "shot" {
        $a = Game-Args @("-windowed", "-ResX=1600", "-ResY=900", "-log", "-${Prefix}Shot=$At", "-${Prefix}ShotName=$Name")
        if ($Cam) { $a += "-${Prefix}Cam=$Cam" }
        (Start-Process -FilePath $Editor -ArgumentList $a -PassThru).WaitForExit()
        Write-Host "Screenshot: $(Join-Path $ShotDir "$Name.png")"
    }
    "test" {
        $all = [ordered]@{}
        foreach ($p in $Cfg.tests.PSObject.Properties) { $all[$p.Name] = [string]$p.Value }
        $pick = Split-List $Scenario
        $names = @(foreach ($k in $all.Keys) { if ($pick.Count -eq 0 -or $pick -contains $k) { $k } })
        foreach ($s in $pick) { if (-not $all.Contains($s)) { $all[$s] = ""; $names += $s } }   # a scenario not in the list runs with no extra switches
        if ($names.Count -eq 0) { Fail "No test scenarios (tessera.json tests)." }
        $logDir = Join-Path $ProjDir "Saved\Logs\Tests"
        New-Item -ItemType Directory -Force $logDir | Out-Null
        if ($Parallel -le 0) { $Parallel = Auto-Parallel $names.Count }
        $jobs = [ordered]@{}
        foreach ($n in $names) {
            $log = Join-Path $logDir "$n.log"
            Remove-Item $log -ErrorAction SilentlyContinue
            $jobs[$n] = @("`"$Proj`"", "-game", "-windowed", "-ResX=960", "-ResY=540", "-log", "-abslog=`"$log`"", "-${Prefix}NoInput", "-${Prefix}Test=$n") + ($all[$n] -split " " | Where-Object { $_ }) + $GameArgs
        }
        $started = Get-Date
        Step "Running $($names.Count) scenario(s), $Parallel at a time"
        Run-Batch $jobs $Parallel 240
        $results = @()
        foreach ($n in $names) {
            $log = Join-Path $logDir "$n.log"
            $lines = @()
            if (Test-Path $log) { $lines = Select-String -Path $log -Pattern "\[TEST $n " | ForEach-Object { $_.Line -replace "^\[.*?\]\[.*?\]Log\w+: Display: ", "" } }
            $ok = @($lines | Where-Object { $_ -match "\] done" }).Count -gt 0 -and @($lines | Where-Object { $_ -match "FAIL" }).Count -eq 0
            if (-not $ok -or $names.Count -eq 1) { Write-Host "  --- $n ---" -ForegroundColor DarkGray; $lines | ForEach-Object { Note $_ } }
            $results += [pscustomobject]@{ Scenario = $n; Result = $(if ($ok) { "PASS" } else { "FAIL" }) }
        }
        Write-Host ""
        foreach ($r in $results) { Write-Host ("  {0,-10} {1}" -f $r.Scenario, $r.Result) -ForegroundColor $(if ($r.Result -eq "PASS") { "Green" } else { "Red" }) }
        Write-Host ("  all done in {0:n0}s (logs: {1})" -f ((Get-Date) - $started).TotalSeconds, $logDir)
        if ($results | Where-Object { $_.Result -ne "PASS" }) { exit 1 }
    }
    "shots" {
        # Each shot is its own game process (-<P>Shot = seconds in, then it quits), saved as a JPEG (a PNG would
        # go through Git LFS; a JPEG is ~200 KB in plain git).
        $out = In-Root $Cfg.shots.out
        $logDir = Join-Path $ProjDir "Saved\Logs\Shots"
        New-Item -ItemType Directory -Force $out, $logDir | Out-Null
        $pick = Split-List $Only
        $list = [ordered]@{}
        foreach ($p in $Cfg.shots.list.PSObject.Properties) {
            if ($pick.Count -eq 0 -or @($pick | Where-Object { $p.Name -like "*$_*" }).Count) { $list[$p.Name] = $p.Value }
        }
        $jobs = [ordered]@{}
        foreach ($n in $list.Keys) {
            $s = $list[$n]
            $png = $(if ($s.png) { $s.png } else { $n })
            Remove-Item (Join-Path $ShotDir "$png.png") -ErrorAction SilentlyContinue
            $jobs[$n] = @("`"$Proj`"", "-game", "-windowed", "-ResX=1600", "-ResY=900", "-log", "-abslog=`"$(Join-Path $logDir "$n.log")`"",
                          "-${Prefix}NoInput", "-${Prefix}Shot=$($s.at)", "-${Prefix}ShotName=$n") + ([string]$s.args -split " " | Where-Object { $_ })
        }
        Step "Capturing $($jobs.Count) screenshots"
        Run-Batch $jobs $(if ($Parallel -gt 0) { $Parallel } else { 6 }) 120
        Add-Type -AssemblyName System.Drawing
        $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
        $params = New-Object System.Drawing.Imaging.EncoderParameters 1
        $params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]88)
        foreach ($n in $list.Keys) {
            $png = Join-Path $ShotDir "$(if ($list[$n].png) { $list[$n].png } else { $n }).png"
            if (-not (Test-Path $png)) { Write-Host "  missing: $n" -ForegroundColor Red; continue }
            $img = [System.Drawing.Image]::FromFile($png)
            $jpg = Join-Path $out "$n.jpg"
            $img.Save($jpg, $codec, $params)
            $img.Dispose()
            Note ("{0,-18} {1:n0} KB" -f $n, ((Get-Item $jpg).Length / 1KB))
        }
    }
}
exit 0
