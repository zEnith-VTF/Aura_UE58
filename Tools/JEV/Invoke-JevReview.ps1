param(
    [Parameter(Mandatory = $true)][string]$TaskId,
    [Parameter(Mandatory = $true)][string]$CheckpointId,
    [Parameter(Mandatory = $true)][ValidateRange(1, 1000000)][int]$ReportRevision,
    [Parameter(Mandatory = $true)][string]$ReportPath,
    [Parameter(Mandatory = $true)][string[]]$EvidencePaths,
    [Parameter(Mandatory = $true)][string]$ReviewerScriptPath,
    [string[]]$ReviewerArguments = @('{REPORT}', '{MANIFEST}'),
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\', '/')
$reviewStartUtc = [DateTime]::UtcNow
$reviewEndUtc = $null
$exitCode = $null
$stdout = ''
$stderr = ''
$manifestHash = $null
$outputDir = $null
$ownsOutputDir = $false
$result = $null

function Assert-ProjectPath([string]$Candidate, [bool]$MustExist = $true) {
    $full = if ([System.IO.Path]::IsPathRooted($Candidate)) {
        [System.IO.Path]::GetFullPath($Candidate)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Candidate))
    }
    $prefix = $projectRoot + [System.IO.Path]::DirectorySeparatorChar
    if (!$full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "PATH_OUTSIDE_PROJECT: $Candidate"
    }
    if ($MustExist -and !(Test-Path -LiteralPath $full -PathType Leaf)) {
        throw "MISSING_FILE: $Candidate"
    }
    # Reject project-internal junctions or symlinks that could resolve outside the root.
    $relative = $full.Substring($prefix.Length)
    $cursor = $projectRoot
    foreach ($part in ($relative -split '[\\/]')) {
        $cursor = Join-Path $cursor $part
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "REPARSE_POINT_NOT_ALLOWED: $Candidate"
            }
        }
    }
    return $full
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Test-NonEmptyStringField([psobject]$Object, [string]$Name) {
    $field = $Object.PSObject.Properties[$Name]
    return ($null -ne $field -and $field.Value -is [string] -and
        ![string]::IsNullOrWhiteSpace($field.Value))
}

function New-ErrorResult([string]$Code, [string]$Detail) {
    return [ordered]@{
        schemaVersion = 1
        taskId = $TaskId
        checkpointId = $CheckpointId
        reportRevision = $ReportRevision
        reviewStatus = 'ERROR'
        verdict = $null
        reviewedManifestSha256 = $manifestHash
        errorCode = $Code
        errorDetail = $Detail
        reviewer = [ordered]@{ tool = 'project-local-adapter'; version = 'UNKNOWN'; model = 'UNKNOWN' }
    }
}

try {
    if ($TaskId -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$' -or
        $CheckpointId -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$') {
        throw 'INVALID_ID: TaskId and CheckpointId must be simple path-safe identifiers'
    }
    $outputDir = Join-Path $projectRoot "Saved\Validation\JEV\$TaskId\$CheckpointId\r$ReportRevision"
    if (Test-Path -LiteralPath $outputDir) {
        throw 'REVISION_EXISTS: use a new report revision; existing results are immutable'
    }
    # Apply the same boundary and reparse-point checks to the output parents before creating anything.
    $null = Assert-ProjectPath $outputDir $false
    New-Item -ItemType Directory -Path $outputDir -ErrorAction Stop | Out-Null
    $null = Assert-ProjectPath $outputDir $false
    $ownsOutputDir = $true

    $sourceReport = Assert-ProjectPath $ReportPath
    $sourceReportHash = Get-Sha256 $sourceReport
    $reviewerScript = Assert-ProjectPath $ReviewerScriptPath
    if ([System.IO.Path]::GetExtension($reviewerScript) -ine '.ps1') {
        throw 'INVALID_REVIEWER: only an explicitly named project-local .ps1 adapter is allowed'
    }
    if ($EvidencePaths.Count -eq 0) { throw 'MISSING_EVIDENCE: at least one evidence file is required' }
    $evidence = @()
    foreach ($path in $EvidencePaths) {
        $evidencePath = Assert-ProjectPath $path
        $evidence += [ordered]@{
            path = $evidencePath.Substring($projectRoot.Length + 1).Replace('\', '/')
            sha256 = Get-Sha256 $evidencePath
        }
    }

    $reportCopy = Join-Path $outputDir 'checkpoint.md'
    Copy-Item -LiteralPath $sourceReport -Destination $reportCopy -ErrorAction Stop
    $manifestPath = Join-Path $outputDir 'manifest.json'
    $manifest = [ordered]@{
        schemaVersion = 1
        taskId = $TaskId
        checkpointId = $CheckpointId
        reportRevision = $ReportRevision
        projectRoot = $projectRoot
        report = [ordered]@{
            path = $reportCopy.Substring($projectRoot.Length + 1).Replace('\', '/')
            sha256 = Get-Sha256 $reportCopy
            sourcePath = $sourceReport.Substring($projectRoot.Length + 1).Replace('\', '/')
            sourceSha256 = $sourceReportHash
        }
        evidence = $evidence
        baseline = 'See checkpoint.md'
        scope = 'Exact report and listed evidence only'
    }
    [System.IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 12), [System.Text.UTF8Encoding]::new($false))
    $manifestHash = Get-Sha256 $manifestPath

    $joinedArgs = $ReviewerArguments -join ' '
    if (!$joinedArgs.Contains('{REPORT}') -or !$joinedArgs.Contains('{MANIFEST}')) {
        throw 'INVALID_ARGUMENTS: reviewer arguments must reference both {REPORT} and {MANIFEST}'
    }
    $hostExe = [System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $hostExe
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($arg in @('-NoProfile', '-NonInteractive', '-File', $reviewerScript)) {
        [void]$startInfo.ArgumentList.Add($arg)
    }
    foreach ($arg in $ReviewerArguments) {
        [void]$startInfo.ArgumentList.Add($arg.Replace('{REPORT}', $reportCopy).Replace('{MANIFEST}', $manifestPath))
    }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (!$process.Start()) { throw 'REVIEWER_START_FAILED: process did not start' }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill($true)
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        throw 'REVIEW_TIMEOUT: reviewer exceeded the configured timeout'
    }
    $exitCode = $process.ExitCode
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()

    if ((Get-Sha256 $sourceReport) -ne $manifest.report.sourceSha256 -or
        (Get-Sha256 $reportCopy) -ne $manifest.report.sha256 -or
        (Get-Sha256 $manifestPath) -ne $manifestHash) {
        throw 'STALE_EVIDENCE: checkpoint or manifest changed during review'
    }
    foreach ($entry in $evidence) {
        $evidenceFull = Assert-ProjectPath $entry.path
        if ((Get-Sha256 $evidenceFull) -ne $entry.sha256) {
            throw "STALE_EVIDENCE: $($entry.path) changed during review"
        }
    }
    if ($exitCode -ne 0) { throw "REVIEWER_EXIT_NONZERO: $exitCode" }
    if ([string]::IsNullOrWhiteSpace($stdout)) { throw 'REVIEWER_EMPTY_OUTPUT: no verdict was returned' }
    try { $parsed = $stdout | ConvertFrom-Json -ErrorAction Stop } catch { throw 'REVIEWER_INVALID_JSON: stdout is not one JSON object' }
    if ($parsed -isnot [pscustomobject] -or
        $parsed.schemaVersion -ne 1 -or $parsed.taskId -cne $TaskId -or
        $parsed.checkpointId -cne $CheckpointId -or $parsed.reportRevision -ne $ReportRevision -or
        $parsed.reviewStatus -cne 'COMPLETED' -or
        $parsed.verdict -cnotin @('PASS', 'REVISE', 'BLOCK') -or
        $parsed.reviewedManifestSha256 -cne $manifestHash -or
        $parsed.findings -isnot [array] -or $parsed.unverified -isnot [array] -or
        $parsed.reviewer -isnot [pscustomobject]) {
        throw 'REVIEWER_INVALID_VERDICT: missing, unsupported, or mismatched result fields'
    }
    if (!(Test-NonEmptyStringField $parsed.reviewer 'tool') -or
        !(Test-NonEmptyStringField $parsed.reviewer 'version') -or
        !(Test-NonEmptyStringField $parsed.reviewer 'model')) {
        throw 'REVIEWER_INVALID_VERDICT: reviewer tool, version, and model are required'
    }
    if ($parsed.verdict -in @('REVISE', 'BLOCK') -and $parsed.findings.Count -eq 0) {
        throw 'REVIEWER_INVALID_VERDICT: REVISE/BLOCK requires at least one finding'
    }
    foreach ($finding in $parsed.findings) {
        if ($finding -isnot [pscustomobject] -or
            !(Test-NonEmptyStringField $finding 'acId') -or
            !(Test-NonEmptyStringField $finding 'evidence') -or
            !(Test-NonEmptyStringField $finding 'issue') -or
            !(Test-NonEmptyStringField $finding 'clearance')) {
            throw 'REVIEWER_INVALID_VERDICT: finding lacks AC ID, evidence, issue, or clearance'
        }
    }
    # Unverified items state whether they leave a required AC unresolved. Optional items may remain on PASS.
    foreach ($item in $parsed.unverified) {
        if ($item -isnot [pscustomobject] -or
            $null -eq $item.PSObject.Properties['required'] -or
            $item.required -isnot [bool] -or
            !(Test-NonEmptyStringField $item 'issue') -or
            ($item.required -and !(Test-NonEmptyStringField $item 'acId'))) {
            throw 'REVIEWER_INVALID_VERDICT: unverified item requires required(bool), issue, and AC ID when required'
        }
    }
    if ($parsed.verdict -eq 'PASS' -and
        ($parsed.findings.Count -ne 0 -or @($parsed.unverified | Where-Object { $_.required }).Count -ne 0)) {
        throw 'REVIEWER_INVALID_VERDICT: PASS cannot include findings or unresolved required ACs'
    }
    $result = [ordered]@{
        schemaVersion = 1
        taskId = $TaskId
        checkpointId = $CheckpointId
        reportRevision = $ReportRevision
        reviewStatus = 'COMPLETED'
        verdict = $parsed.verdict
        reviewedManifestSha256 = $manifestHash
        findings = @($parsed.findings)
        unverified = @($parsed.unverified)
        reviewer = $parsed.reviewer
    }
} catch {
    $message = [string]$_.Exception.Message
    $code = ($message -split ':', 2)[0]
    $result = New-ErrorResult $code $message
} finally {
    $reviewEndUtc = [DateTime]::UtcNow
    if ($ownsOutputDir -and (Test-Path -LiteralPath $outputDir -PathType Container)) {
        $raw = "START_UTC=$($reviewStartUtc.ToString('o'))`nEND_UTC=$($reviewEndUtc.ToString('o'))`nEXIT_CODE=$exitCode`nSTDOUT:`n$stdout`nSTDERR:`n$stderr"
        [System.IO.File]::WriteAllText((Join-Path $outputDir 'review.raw.txt'), $raw, [System.Text.UTF8Encoding]::new($false))
        $result.startUtc = $reviewStartUtc.ToString('o')
        $result.endUtc = $reviewEndUtc.ToString('o')
        $result.exitCode = $exitCode
        [System.IO.File]::WriteAllText((Join-Path $outputDir 'review.json'), ($result | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))
    }
}

Write-Output ($result | ConvertTo-Json -Depth 16 -Compress)
if ($result.reviewStatus -eq 'ERROR') { exit 2 }
exit 0
