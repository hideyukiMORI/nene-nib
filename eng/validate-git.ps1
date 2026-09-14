[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    $branch = & git branch --show-current
    if ($LASTEXITCODE -ne 0) { throw 'GIT-002: cannot read branch.' }
    $base = 'origin/main'
    if ($env:GITHUB_EVENT_PATH) {
        $event = Get-Content -LiteralPath $env:GITHUB_EVENT_PATH -Raw | ConvertFrom-Json
        if (-not $event.PSObject.Properties['pull_request']) { throw 'GIT-004: CI requires a pull request event.' }
        $branch = $event.pull_request.head.ref
        $base = $event.pull_request.base.sha
        if ($event.pull_request.draft) { throw 'QLT-012: full CI gate must not run for draft pull requests.' }
        if ($event.pull_request.body -notmatch '(?m)\bCloses #[1-9][0-9]*\b') { throw 'GIT-001: PR body must close an Issue.' }
        foreach ($field in @('目的:', '使った正典経路:', '規則 ID:', '検証', 'Waivers:', '残るリスク:')) {
            if (-not $event.pull_request.body.Contains($field)) { throw "GIT-004: missing PR field $field" }
        }
        New-Item -ItemType Directory -Force -Path out/git | Out-Null
        Set-Content -LiteralPath out/git/pr-title.txt -Value $event.pull_request.title -Encoding utf8NoBOM
        & python eng/git-conventions.py out/git/pr-title.txt --title-only
        if ($LASTEXITCODE -ne 0) { throw 'GIT-003: invalid PR title.' }
    }
    if ($branch -ne 'main' -and $branch -notmatch '^(feat|fix|docs|refactor|test|build|ci|chore)/[1-9][0-9]*-[a-z0-9]+(?:-[a-z0-9]+)*$') {
        throw "GIT-002: invalid branch $branch"
    }
    $commits = @(& git rev-list "$base..HEAD")
    if ($LASTEXITCODE -ne 0) { throw 'GIT-003: base history is missing; fetch origin first.' }
    New-Item -ItemType Directory -Force -Path out/git | Out-Null
    foreach ($commit in $commits) {
        $message = & git show -s --format=%B $commit
        if ($LASTEXITCODE -ne 0) { throw 'GIT-003: cannot read commit.' }
        Set-Content -LiteralPath out/git/message.txt -Value $message -Encoding utf8NoBOM
        & python eng/git-conventions.py out/git/message.txt
        if ($LASTEXITCODE -ne 0) { throw "GIT-003: invalid commit $commit" }
    }
    Write-Host "Git conventions passed ($($commits.Count) new commit(s))."
}
finally { Pop-Location }
