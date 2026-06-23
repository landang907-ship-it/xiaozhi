# audit.ps1 - Mini-Xiaozhi Project Health Check
#
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File audit.ps1
#
# Checks:
#   1. STATE.md freshness (alert if >7 days old)
#   2. AUDIT_LOG.md freshness (alert if silent >2 hours during session)
#   3. Git status (uncommitted changes, branch, last commit)
#   4. Last 5 AUDIT entries
#   5. Phases table content (verify against actual files)
#   6. Critical files exist (STATE.md, AUDIT_LOG.md, main/CMakeLists.txt, etc.)

$ErrorActionPreference = 'Continue'

# Paths (relative to xiaozhi_like/)
$root = $PSScriptRoot
$state = Join-Path $root 'STATE.md'
$audit = Join-Path $root 'AUDIT_LOG.md'
$main = Join-Path $root 'main'
$boards = Join-Path $main 'boards\esp32s3-wroom-1'
$xiaozhi_src = Join-Path $root 'xiaozhi_src\main'
$tmp = Join-Path $root 'tmp'
$partitionCsv = Join-Path $root 'partitions\v2\4m.csv'

Write-Host ''
Write-Host '================================================================' -ForegroundColor Cyan
Write-Host '  Mini-Xiaozhi (xiaozhi_like) - Audit Report' -ForegroundColor Cyan
Write-Host ("  Generated: {0:yyyy-MM-dd HH:mm:ss}" -f (Get-Date)) -ForegroundColor Cyan
Write-Host '================================================================' -ForegroundColor Cyan
Write-Host ''

# === 1. STATE.md freshness ===
Write-Host '[1] STATE.md freshness' -ForegroundColor Yellow
if (Test-Path $state) {
    $stateInfo = Get-Item $state
    $age = (Get-Date) - $stateInfo.LastWriteTime
    if ($age.Days -gt 0) { $ageStr = "$($age.Days) ngay $($age.Hours) gio" }
    elseif ($age.Hours -gt 0) { $ageStr = "$($age.Hours) gio $($age.Minutes) phut" }
    else { $ageStr = "$($age.Minutes) phut" }
    Write-Host ("    Last modified: {0:yyyy-MM-dd HH:mm} ({1} truoc)" -f $stateInfo.LastWriteTime, $ageStr) -ForegroundColor Gray
    Write-Host ("    Size: {0:N0} bytes" -f $stateInfo.Length) -ForegroundColor Gray
    if ($age.Days -gt 7) {
        Write-Host '    [!] STATE.md stale (>7 days)' -ForegroundColor Red
    } elseif ($age.Days -gt 2) {
        Write-Host '    [!] STATE.md cu (>2 days)' -ForegroundColor Yellow
    } else {
        Write-Host '    [OK] STATE.md fresh' -ForegroundColor Green
    }
} else {
    Write-Host '    [NG] STATE.md MISSING!' -ForegroundColor Red
}
Write-Host ''

# === 2. AUDIT_LOG.md freshness ===
Write-Host '[2] AUDIT_LOG.md freshness' -ForegroundColor Yellow
if (Test-Path $audit) {
    $auditInfo = Get-Item $audit
    $age = (Get-Date) - $auditInfo.LastWriteTime
    $ageMin = [int]$age.TotalMinutes
    Write-Host ("    Last modified: {0:yyyy-MM-dd HH:mm} ({1} phut truoc)" -f $auditInfo.LastWriteTime, $ageMin) -ForegroundColor Gray
    Write-Host ("    Size: {0:N0} bytes" -f $auditInfo.Length) -ForegroundColor Gray
    if ($ageMin -gt 120) {
        Write-Host '    [!] AUDIT_LOG im >2 gio - co the session bi ngat' -ForegroundColor Yellow
    } else {
        Write-Host '    [OK] AUDIT_LOG active' -ForegroundColor Green
    }
} else {
    Write-Host '    [NG] AUDIT_LOG.md MISSING!' -ForegroundColor Red
}
Write-Host ''

# === 3. Git status ===
Write-Host '[3] Git status' -ForegroundColor Yellow
$gitDir = Join-Path $root '.git'
if (Test-Path $gitDir) {
    Push-Location $root
    $branch = git rev-parse --abbrev-ref HEAD 2>$null
    $lastCommit = git log -1 --pretty=format:'%h %s (%ai)' 2>$null
    Write-Host ("    Branch: {0}" -f $branch) -ForegroundColor Gray
    Write-Host ("    Last commit: {0}" -f $lastCommit) -ForegroundColor Gray
    $status = git status --short 2>$null
    if ($status) {
        Write-Host '    Uncommitted changes:' -ForegroundColor Yellow
        $status | ForEach-Object { Write-Host "      $_" -ForegroundColor Yellow }
    } else {
        Write-Host '    [OK] Working tree clean' -ForegroundColor Green
    }
    Pop-Location
} else {
    Write-Host '    [!] Chua co git init - chay: cd xiaozhi_like && git init' -ForegroundColor Yellow
}
Write-Host ''

# === 4. Last 5 AUDIT entries ===
Write-Host '[4] Last 5 AUDIT entries' -ForegroundColor Yellow
if (Test-Path $audit) {
    Get-Content $audit -Tail 5 | ForEach-Object { Write-Host "    $_" -ForegroundColor Gray }
} else {
    Write-Host '    (no AUDIT_LOG)' -ForegroundColor Gray
}
Write-Host ''

# === 5. Phases table ===
Write-Host '[5] Phases table (from STATE.md)' -ForegroundColor Yellow
if (Test-Path $state) {
    $phasesLines = Select-String -Path $state -Pattern 'P[1-9]' | Select-Object -ExpandProperty Line
    if ($phasesLines) {
        foreach ($line in $phasesLines) {
            if ($line -match 'Done') { $color = 'Green' }
            elseif ($line -match 'Next') { $color = 'Cyan' }
            else { $color = 'Gray' }
            Write-Host "    $line" -ForegroundColor $color
        }
    }
}
Write-Host ''

# === 6. Critical files exist ===
Write-Host '[6] Critical files check' -ForegroundColor Yellow
$critical = @{
    'main/CMakeLists.txt' = $main
    'boards/esp32s3-wroom-1/config.h' = $boards
    'boards/esp32s3-wroom-1/config.json' = $boards
    'xiaozhi_src/main/ (44 files copied)' = $xiaozhi_src
    'tmp/xiaozhi_src_repo/ (reference clone)' = $tmp
    'partitions/v2/4m.csv' = $partitionCsv
    'audit.ps1' = $root
    'AUDIT_LOG.md' = $root
}
foreach ($key in $critical.Keys) {
    $path = $critical[$key]
    if (Test-Path $path) {
        if ((Get-Item $path) -is [System.IO.DirectoryInfo]) {
            $count = (Get-ChildItem $path -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
            Write-Host ("    [OK] {0,-50} ({1} files)" -f $key, $count) -ForegroundColor Green
        } else {
            Write-Host ("    [OK] {0,-50}" -f $key) -ForegroundColor Green
        }
    } else {
        Write-Host ("    [NG] {0,-50} MISSING" -f $key) -ForegroundColor Red
    }
}
Write-Host ''

# === 7. PHASE_STATUS.json (auto-advance policy) ===
Write-Host '[7] PHASE_STATUS.json (auto-advance policy)' -ForegroundColor Yellow
$phaseStatus = Join-Path $root 'PHASE_STATUS.json'
if (Test-Path $phaseStatus) {
    try {
        $json = Get-Content $phaseStatus -Raw | ConvertFrom-Json
        Write-Host ("    policy: {0}" -f $json.policy) -ForegroundColor Gray
        Write-Host ("    current_phase: {0} (done={1})" -f $json.current_phase, $json.current_phase_done) -ForegroundColor Gray
        Write-Host ("    next_phase: {0}" -f $json.next_phase) -ForegroundColor Gray
        if ($json.current_phase_done -eq $true) {
            Write-Host '    [OK] PHASE_STATUS valid, auto-advance ready' -ForegroundColor Green
        } else {
            Write-Host '    [!] current_phase_done=false - phase chua hoan thanh' -ForegroundColor Yellow
        }
    } catch {
        Write-Host '    [NG] PHASE_STATUS.json invalid JSON' -ForegroundColor Red
    }
} else {
    Write-Host '    [NG] PHASE_STATUS.json MISSING' -ForegroundColor Red
}
Write-Host ''

# === Summary ===
Write-Host '================================================================' -ForegroundColor Cyan
Write-Host '  End of audit report' -ForegroundColor Cyan
Write-Host '================================================================' -ForegroundColor Cyan
Write-Host ''
