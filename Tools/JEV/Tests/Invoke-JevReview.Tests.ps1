param(
    [switch]$MockReviewer,
    [string]$Report,
    [string]$Manifest,
    [string]$Mode = 'PASS'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($MockReviewer) {
    if ($Mode -eq 'TIMEOUT') { Start-Sleep -Seconds 3 }
    if ($Mode -eq 'EMPTY') { exit 0 }
    if ($Mode -eq 'TEXT_PASS') { Write-Output 'PASS appears in this sentence, but this is not a verdict.'; exit 0 }
    if ($Mode -eq 'MALFORMED') { Write-Output '{invalid-json'; exit 0 }

    $data = Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
    if ($Mode -eq 'MUTATE') {
        $evidenceFile = Join-Path $data.projectRoot $data.evidence[0].path
        Add-Content -LiteralPath $evidenceFile -Value 'changed during synthetic review'
    }
    if ($Mode -eq 'MUTATE_REPORT') {
        $sourceReport = Join-Path $data.projectRoot $data.report.sourcePath
        Add-Content -LiteralPath $sourceReport -Value 'changed during synthetic review'
    }
    $finding = [ordered]@{
        acId = 'AC-01'
        evidence = $data.evidence[0].path
        issue = 'Synthetic mismatch'
        clearance = 'Provide matching evidence'
    }
    $verdict = if ($Mode -in @('REVISE', 'BLOCK')) { $Mode } else { 'PASS' }
    $output = [ordered]@{
        schemaVersion = 1
        taskId = $data.taskId
        checkpointId = $data.checkpointId
        reportRevision = $data.reportRevision
        reviewStatus = 'COMPLETED'
        verdict = $verdict
        reviewedManifestSha256 = (Get-FileHash -LiteralPath $Manifest -Algorithm SHA256).Hash.ToLowerInvariant()
        findings = @()
        unverified = @()
        reviewer = [ordered]@{ tool = 'synthetic-mock'; version = '1'; model = 'none' }
    }
    if ($verdict -ne 'PASS') { $output.findings = @($finding) }
    if ($Mode -eq 'PASS_FINDING') { $output.findings = @($finding) }
    if ($Mode -eq 'BAD_FINDINGS_TYPE') { $output.findings = 'not an array' }
    if ($Mode -eq 'BAD_UNVERIFIED_TYPE') { $output.unverified = 'not an array' }
    if ($Mode -eq 'BAD_REVIEWER_TYPE') { $output.reviewer = 'not an object' }
    if ($Mode -eq 'BAD_REVIEWER_FIELD') { $output.reviewer.tool = 7 }
    if ($Mode -eq 'BAD_FINDING_FIELD') { $finding.issue = 42; $output.findings = @($finding); $output.verdict = 'REVISE' }
    if ($Mode -eq 'BAD_UNVERIFIED_FIELD') {
        $output.unverified = @([ordered]@{ acId = ''; required = $false; issue = 42 })
    }
    if ($Mode -eq 'PASS_REQUIRED_UNVERIFIED') {
        $output.unverified = @([ordered]@{ acId = 'AC-01'; required = $true; issue = 'Required proof is missing' })
    }
    if ($Mode -eq 'PASS_OPTIONAL_UNVERIFIED') {
        $output.unverified = @([ordered]@{ acId = ''; required = $false; issue = 'Optional work outside this checkpoint' })
    }
    Write-Output ($output | ConvertTo-Json -Depth 12 -Compress)
    if ($Mode -eq 'NONZERO') { exit 7 }
    exit 0
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..')).TrimEnd('\', '/')
$invoke = Join-Path $projectRoot 'Tools\JEV\Invoke-JevReview.ps1'
$runId = [Guid]::NewGuid().ToString('N').Substring(0, 10)
$fixtureDir = Join-Path $projectRoot "Saved\Validation\JEV\TestFixtures\$runId"
New-Item -ItemType Directory -Path $fixtureDir -ErrorAction Stop | Out-Null
$sourceHash = (Get-FileHash -LiteralPath (Join-Path $projectRoot 'AGENTS.md') -Algorithm SHA256).Hash
$checks = 0

function Assert([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw "TEST_FAILED: $Message" }
    $script:checks++
}

function Invoke-Case([string]$Name, [string]$ReviewerMode, [string]$ExpectedStatus, [object]$ExpectedVerdict, [int]$Timeout = 10) {
    $taskId = "SYNTH-$runId-$Name"
    $reportFile = Join-Path $fixtureDir "$Name-report.md"
    $evidenceFile = Join-Path $fixtureDir "$Name-evidence.txt"
    Set-Content -LiteralPath $reportFile -Value "SYNTHETIC report $Name; AC-01" -Encoding utf8
    Set-Content -LiteralPath $evidenceFile -Value "SYNTHETIC evidence $Name" -Encoding utf8
    $text = & $invoke -TaskId $taskId -CheckpointId POST -ReportRevision 1 -ReportPath $reportFile -EvidencePaths @($evidenceFile) -ReviewerScriptPath $PSCommandPath -ReviewerArguments @('-MockReviewer', '-Report', '{REPORT}', '-Manifest', '{MANIFEST}', '-Mode', $ReviewerMode) -TimeoutSeconds $Timeout
    $code = $LASTEXITCODE
    $data = $text | ConvertFrom-Json
    Assert ($data.reviewStatus -eq $ExpectedStatus) "$Name status"
    Assert ($data.verdict -eq $ExpectedVerdict) "$Name verdict"
    Assert (($ExpectedStatus -eq 'ERROR' -and $code -ne 0) -or ($ExpectedStatus -eq 'COMPLETED' -and $code -eq 0)) "$Name exit code"
    $resultDir = Join-Path $projectRoot "Saved\Validation\JEV\$taskId\POST\r1"
    Assert (Test-Path -LiteralPath (Join-Path $resultDir 'review.raw.txt')) "$Name raw output"
    Assert (Test-Path -LiteralPath (Join-Path $resultDir 'review.json')) "$Name structured output"
    return [pscustomobject]@{ TaskId = $taskId; ResultDir = $resultDir; Report = $reportFile; Evidence = $evidenceFile }
}

$null = Invoke-Case PASS PASS COMPLETED PASS
$null = Invoke-Case REVISE REVISE COMPLETED REVISE
$null = Invoke-Case BLOCK BLOCK COMPLETED BLOCK
$null = Invoke-Case TIMEOUT TIMEOUT ERROR $null 1
$null = Invoke-Case EMPTY EMPTY ERROR $null
$null = Invoke-Case TEXT-PASS TEXT_PASS ERROR $null
$null = Invoke-Case MALFORMED MALFORMED ERROR $null
$null = Invoke-Case NONZERO NONZERO ERROR $null
$null = Invoke-Case MUTATE MUTATE ERROR $null
$null = Invoke-Case MUTATE-REPORT MUTATE_REPORT ERROR $null
$null = Invoke-Case PASS-FINDING PASS_FINDING ERROR $null
$null = Invoke-Case BAD-FINDINGS-TYPE BAD_FINDINGS_TYPE ERROR $null
$null = Invoke-Case BAD-UNVERIFIED-TYPE BAD_UNVERIFIED_TYPE ERROR $null
$null = Invoke-Case BAD-REVIEWER-TYPE BAD_REVIEWER_TYPE ERROR $null
$null = Invoke-Case BAD-REVIEWER-FIELD BAD_REVIEWER_FIELD ERROR $null
$null = Invoke-Case BAD-FINDING-FIELD BAD_FINDING_FIELD ERROR $null
$null = Invoke-Case BAD-UNVERIFIED-FIELD BAD_UNVERIFIED_FIELD ERROR $null
$null = Invoke-Case PASS-REQUIRED-UNVERIFIED PASS_REQUIRED_UNVERIFIED ERROR $null
$null = Invoke-Case PASS-OPTIONAL-UNVERIFIED PASS_OPTIONAL_UNVERIFIED COMPLETED PASS

$missingReport = Join-Path $fixtureDir 'missing-report.md'
Set-Content -LiteralPath $missingReport -Value 'SYNTHETIC missing-evidence case' -Encoding utf8
$missingFile = Join-Path $fixtureDir 'missing-evidence.txt'
$missing = & $invoke -TaskId "SYNTH-$runId-MISSING" -CheckpointId POST -ReportRevision 1 -ReportPath $missingReport -EvidencePaths @($missingFile) -ReviewerScriptPath $PSCommandPath -ReviewerArguments @('-MockReviewer', '-Report', '{REPORT}', '-Manifest', '{MANIFEST}')
$missingResult = $missing | ConvertFrom-Json
Assert ($missingResult.reviewStatus -eq 'ERROR' -and $missingResult.verdict -eq $null) 'missing evidence fails closed'
Assert ($missingResult.errorCode -eq 'MISSING_FILE') 'missing evidence error code'

$outside = Join-Path (Split-Path $projectRoot -Parent) 'synthetic-outside-evidence.txt'
$outsideOutput = & $invoke -TaskId "SYNTH-$runId-OUTSIDE" -CheckpointId POST -ReportRevision 1 -ReportPath $missingReport -EvidencePaths @($outside) -ReviewerScriptPath $PSCommandPath -ReviewerArguments @('-MockReviewer', '-Report', '{REPORT}', '-Manifest', '{MANIFEST}')
$outsideResult = $outsideOutput | ConvertFrom-Json
Assert ($outsideResult.reviewStatus -eq 'ERROR' -and $outsideResult.errorCode -eq 'PATH_OUTSIDE_PROJECT') 'outside path rejected before read'

$repeat = Invoke-Case REPEAT PASS COMPLETED PASS
$resultFile = Join-Path $repeat.ResultDir 'review.json'
$before = (Get-FileHash -LiteralPath $resultFile -Algorithm SHA256).Hash
$again = & $invoke -TaskId $repeat.TaskId -CheckpointId POST -ReportRevision 1 -ReportPath $repeat.Report -EvidencePaths @($repeat.Evidence) -ReviewerScriptPath $PSCommandPath -ReviewerArguments @('-MockReviewer', '-Report', '{REPORT}', '-Manifest', '{MANIFEST}')
$againResult = $again | ConvertFrom-Json
$after = (Get-FileHash -LiteralPath $resultFile -Algorithm SHA256).Hash
Assert ($againResult.reviewStatus -eq 'ERROR' -and $againResult.errorCode -eq 'REVISION_EXISTS') 'duplicate revision rejected'
Assert ($before -eq $after) 'duplicate revision did not overwrite result'

# An in-project junction proves that output parent reparse points are rejected without touching outside paths.
$junctionTask = "SYNTH-$runId-OUTPUT-JUNCTION"
$junctionPath = Join-Path $projectRoot "Saved\Validation\JEV\$junctionTask"
New-Item -ItemType Junction -Path $junctionPath -Target $fixtureDir -ErrorAction Stop | Out-Null
$junctionOutput = & $invoke -TaskId $junctionTask -CheckpointId POST -ReportRevision 1 -ReportPath $repeat.Report -EvidencePaths @($repeat.Evidence) -ReviewerScriptPath $PSCommandPath -ReviewerArguments @('-MockReviewer', '-Report', '{REPORT}', '-Manifest', '{MANIFEST}')
$junctionResult = $junctionOutput | ConvertFrom-Json
Assert ($junctionResult.reviewStatus -eq 'ERROR' -and $junctionResult.errorCode -eq 'REPARSE_POINT_NOT_ALLOWED') 'output junction rejected before write'
Assert (!(Test-Path -LiteralPath (Join-Path $fixtureDir 'POST'))) 'output junction target unchanged'

Assert ($sourceHash -eq (Get-FileHash -LiteralPath (Join-Path $projectRoot 'AGENTS.md') -Algorithm SHA256).Hash) 'production document unchanged by test'
Write-Output "PASS: $checks deterministic assertions; synthetic artifacts: $fixtureDir"
