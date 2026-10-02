#requires -Version 7.0

param(
    [Parameter(Mandatory = $true)][int]$Changelist,
    [Parameter(Mandatory = $true)][string]$RecordRoot,
    [string]$EngineRoot = 'E:\UE5_Download\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$recordPath = [IO.Path]::GetFullPath($RecordRoot)
if (!$recordPath.StartsWith($projectRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Validation records must remain in this project.'
}
$releaseName = 'Aura-Demo-20261001-CL' + $Changelist + '-Win64'
$releaseRoot = Join-Path $projectRoot ('Releases\' + $releaseName)
$zipPath = $releaseRoot + '.zip'
if (Test-Path -LiteralPath $zipPath) { throw 'A final ZIP already exists for this changelist.' }
New-Item -ItemType Directory -Path $recordPath -Force | Out-Null

Push-Location -LiteralPath $projectRoot
try {
    $spec = @(p4 -c Aura_UE58_WS change -o $Changelist)
    if ($LASTEXITCODE -ne 0 -or !($spec -match '^Status:\s+submitted')) {
        throw 'Package only a successfully submitted changelist.'
    }
    $dirty = @(p4 -c Aura_UE58_WS opened Source/... Content/... Config/... Plugins/AsyncLoadingScreen/... Aura.uproject 2>$null)
    if ($dirty.Count -gt 0) { throw 'Gameplay/build inputs still have opened changes.' }
    $uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    $arguments = @(
        'BuildCookRun', ('-project=' + (Join-Path $projectRoot 'Aura.uproject')),
        '-noP4', '-platform=Win64', '-clientconfig=Shipping',
        '-nocompile', '-nocompileuat', '-skipbuildeditor',
        '-build', '-cook', '-stage', '-pak', '-iostore', '-archive',
        ('-archivedirectory=' + $releaseRoot),
        '-map=/Game/Maps/StartupMap+/Game/Maps/MainMenu+/Game/Maps/LoadMenu+/Game/Maps/Dungeon+/Game/Fantastic_Dungeon_Pack/maps/Dungeon_2',
        '-prereqs', '-nodebuginfo', '-utf8output', '-unattended'
    )
    $arguments | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $recordPath 'uat-arguments.json') -Encoding utf8
    & $uat @arguments 2>&1 | Tee-Object -FilePath (Join-Path $recordPath 'shipping-package.log') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw ('Shipping BuildCookRun failed with exit code ' + $LASTEXITCODE) }

    $windowsRoot = Join-Path $releaseRoot 'Windows'
    $required = @(
        'Aura.exe', 'Aura\Binaries\Win64\Aura-Win64-Shipping.exe',
        'Aura\Content\Paks\Aura-Windows.pak', 'Aura\Content\Paks\Aura-Windows.utoc',
        'Aura\Content\Paks\Aura-Windows.ucas',
        'Aura\Content\Movies\CG_Start.mp4', 'Aura\Content\Movies\CG_Load.mp4'
    )
    foreach ($relative in $required) {
        if (!(Test-Path -LiteralPath (Join-Path $windowsRoot $relative))) { throw ('Missing staged runtime file: ' + $relative) }
    }
    if (Test-Path -LiteralPath (Join-Path $windowsRoot 'Aura\Saved')) {
        throw 'Do not distribute developer save files, logs, or user settings.'
    }
    $installers = @(Get-ChildItem -LiteralPath $windowsRoot -Recurse -File | Where-Object { $_.Name -match '^(UEPrereqSetup_x64|vc_redist\.x64)\.exe$' })
    $crt = @(Get-ChildItem -LiteralPath $windowsRoot -Recurse -File | Where-Object { $_.Name -match '^(vcruntime140|msvcp140).*\.dll$' })
    if ($installers.Count -eq 0 -and $crt.Count -eq 0) {
        throw 'No prerequisite installer or app-local MSVC runtime was staged.'
    }

    $metadata = [ordered]@{
        Product = 'Aura Demo'; ProjectVersion = '0.1.0-demo.20261001';
        PerforceChangelist = $Changelist; Configuration = 'Shipping'; Platform = 'Win64';
        Engine = '5.8.3-58210709'; BuiltAt = (Get-Date -Format 'yyyy-MM-ddTHH:mm:sszzz');
        MultiplayerAcceptance = 'Deferred by user';
        RuntimePrerequisites = @($installers | ForEach-Object { $_.FullName.Substring($windowsRoot.Length + 1) });
        AppLocalRuntimeDLLs = @($crt | ForEach-Object { $_.FullName.Substring($windowsRoot.Length + 1) })
    }
    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $windowsRoot 'VERSION.json') -Encoding utf8
    $template = [IO.File]::ReadAllText((Join-Path $projectRoot 'Docs\Engineering\Templates\AuraDemo-Play.md'))
    $template = $template.Replace('{{CHANGE_LIST}}', [string]$Changelist).Replace('{{BUILD_TIME}}', $metadata.BuiltAt)
    [IO.File]::WriteAllText((Join-Path $windowsRoot '游玩说明.txt'), $template, [Text.UTF8Encoding]::new($false))
    $license = [IO.File]::ReadAllText((Join-Path $projectRoot 'Plugins\AsyncLoadingScreen\LICENSE'))
    [IO.File]::WriteAllText((Join-Path $windowsRoot 'THIRD-PARTY-NOTICES.txt'), "Async Loading Screen plugin`r`n`r`n" + $license, [Text.UTF8Encoding]::new($false))
    if ($installers.Count -gt 0) {
        $relativeInstaller = $installers[0].FullName.Substring($windowsRoot.Length + 1)
        $batch = "@echo off`r`nstart /wait `"`" `"%~dp0" + $relativeInstaller + "`"`r`n"
        [IO.File]::WriteAllText((Join-Path $windowsRoot 'Install-Prerequisites.bat'), $batch, [Text.UTF8Encoding]::new($false))
    }
    [IO.File]::WriteAllText((Join-Path $windowsRoot 'Start-DX11.bat'), "@echo off`r`nstart `"`" `"%~dp0Aura.exe`" -d3d11`r`n", [Text.UTF8Encoding]::new($false))

    $manifest = @(Get-ChildItem -LiteralPath $windowsRoot -Recurse -File | ForEach-Object {
        [ordered]@{ Path = $_.FullName.Substring($windowsRoot.Length + 1); Bytes = $_.Length; SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $recordPath 'distribution-file-manifest.json') -Encoding utf8
    [IO.Compression.ZipFile]::CreateFromDirectory($releaseRoot, $zipPath, [IO.Compression.CompressionLevel]::Fastest, $false)
    $zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
    [IO.File]::WriteAllText($zipPath + '.sha256', $zipHash + '  ' + [IO.Path]::GetFileName($zipPath) + "`r`n", [Text.UTF8Encoding]::new($false))
    [ordered]@{ Changelist = $Changelist; Package = $windowsRoot; Zip = $zipPath; SHA256 = $zipHash; Files = $manifest.Count; PrerequisiteInstallers = $installers.Count; AppLocalRuntimeDLLs = $crt.Count } |
        ConvertTo-Json | Tee-Object -FilePath (Join-Path $recordPath 'distribution-result.json')
}
finally { Pop-Location }
