[CmdletBinding()]
param(
    [ValidateRange(1, [int]::MaxValue)][int]$PullRequest,
    [ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedHead
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
# 違反の文言は日本語。CI の windows-2022 は stdout が cp1252 なので、印字で UnicodeEncodeError になり
# 理由が隠れていた（Issue #94）。python を呼ぶ前にここ 1 か所で UTF-8 にする。
# 入力側も同じ 1 か所で固定する（Issue #106）。commit の subject は日本語で、git は UTF-8 で出す。
# pwsh は native command の出力を [Console]::OutputEncoding で読むので、cp932 の端末だと
# 壊れた文字列を python に渡して GIT-003 になっていた。$OutputEncoding は逆向き（子へ渡す側）。
$env:PYTHONUTF8 = '1'
$utf8Encoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding
try {
    $branch = & git branch --show-current
    if ($LASTEXITCODE -ne 0) { throw 'GIT-002: cannot read branch.' }
    $base = 'origin/main'
    if ($PullRequest) {
        if (-not $ExpectedHead) { throw 'GIT-004: expected head is required for merge CI.' }
        $raw = & gh api --hostname github.com "repos/hideyukiMORI/nene-nib/pulls/$PullRequest"
        if ($LASTEXITCODE -ne 0) { throw 'GIT-004: cannot read the merge target.' }
        $pull = $raw | ConvertFrom-Json
        if ($pull.number -ne $PullRequest -or $pull.state -ne 'open' -or $pull.draft -or
            $pull.base.ref -ne 'main' -or $pull.head.sha -cne $ExpectedHead -or
            $pull.head.repo.full_name -cne 'hideyukiMORI/nene-nib') {
            throw 'GIT-004: merge target is not the expected open ready PR.'
        }
        $checkedHead = & git rev-parse HEAD
        if ($LASTEXITCODE -ne 0 -or $checkedHead -cne $ExpectedHead) { throw 'GIT-004: checkout head differs.' }
        $event = [pscustomobject]@{ pull_request = $pull }
    }
    elseif ($env:GITHUB_EVENT_PATH) {
        throw 'GIT-004: CI requires an explicit pull request and expected head.'
    }
    else { $event = $null }
    if ($event) {
        $branch = $event.pull_request.head.ref
        $base = $event.pull_request.base.sha
        if ($event.pull_request.draft) { throw 'QLT-012: merge checks run for ready pull requests.' }
        if ($event.pull_request.body -notmatch '(?m)\bCloses #[1-9][0-9]*\b') { throw 'GIT-001: PR body must close an Issue.' }
        foreach ($field in @('目的:', '使った正典経路:', '規則 ID:', '検証', 'Waivers:', '残るリスク:')) {
            if (-not $event.pull_request.body.Contains($field)) { throw "GIT-004: missing PR field $field" }
        }
        New-Item -ItemType Directory -Force -Path out/git | Out-Null
        Set-Content -LiteralPath out/git/pr-title.txt -Value $event.pull_request.title -Encoding utf8NoBOM
        & python eng/git-conventions.py out/git/pr-title.txt --title-only
        if ($LASTEXITCODE -ne 0) { throw 'GIT-003: invalid PR title.' }
        Set-Content -LiteralPath out/git/pr-body.txt -Value $event.pull_request.body -Encoding utf8NoBOM
        & python eng/git-conventions.py out/git/pr-body.txt --pr-body
        if ($LASTEXITCODE -ne 0) { throw 'GIT-004: incomplete verification record.' }
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
    if ($PullRequest) {
        & git diff --check "$base...HEAD"
        if ($LASTEXITCODE -ne 0) { throw 'GIT-004: whitespace errors in the merge diff.' }
    }
    Write-Host "Git conventions passed ($($commits.Count) new commit(s))."
}
finally { Pop-Location }
