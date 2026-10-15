# 5-ds — 統合件名は検証したPRタイトルから取得する（Issue #384）

出典: docs/quality/gate-proofs.md の節 5-ds（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-10 23:20以降。#382はPR383/必須Check38059034643（head eb0ae7b）でmain38bc5b60fc686b98b8338d316793724717f8715bへ統合、snapshot-382の491records/112854990bytesをSHA照合収載した。manifest55725476bb9f0a593af78614c8591ed1118077d6f521db08eb07fa2ab1abbaad。698ignoredを分類し未保存/唯一成果/稼働/shortcut/linkなしを確認、追加WT/OUTを23:19:53に監査整理。branch/commit/通常Release/全原記録を保持。acceptance-382/closeout.mdが統合後の正本。

この統合時、親がgh --subjectを手入力し、正しいPRtitleのIssue番号382をPR番号383へ取り違えた。実main件名は`refactor(status): 固定ラベルの一時コピーを省く (#383)`。PRtitle/body/Closes #382/先行commitは正しく製品tree・CI結果に影響はないが、GIT-003/004の追跡誤りであり「全規約無違反」としない。main履歴は改変しない。

対象GIT-001/002/003/004、QLT-001/010/012、CNF-006。waiver none。先行計画8224dffの後、eng/merge-pr.pyを追加し既存git-conventions.pyのvalidate/validate_prを共用、validate_merge_titleだけ追加する。件名を別入力せずPRtitleから一字不変で渡す。Workflow/Commit規約の統合操作をこの入口へ接続。製品/API/schema/fixture/CI閾値/ruleset/権限/allowlistは不変、activeへの繰上げなし。大きな設計・ゲート変更ではなく既存Git規約の入口整備なので新ADRは追加しない。

WT=D:/NeNeNib/worktrees/384-merge-subject、OUT=D:/NeNeNib/outputs/20261010-merge-subject。恒久先はwave3/snapshot-384、統合/整理証拠acceptance-384。原pathは保持する。

### 実装と境界

- 正本repo `github.com/hideyukiMORI/nene-nib` は道具の定数一箇所。view/checks/merge/完了viewへ同じ--repo、branch rules APIも同じhost/repoを明示する。GH_REPO/default remoteへ委ねない。
- PR番号と40桁expected-headを必須にし、返却番号/head/OPEN/非draft/main、既存title形式、唯一のCloses Issueとの一致、4検証記録欄、required全SUCCESS、rules取得・非queueを要求する。
- 既定はplan出力だけ、明示--executeで一度だけ`gh pr merge --squash --match-head-commit`へ渡す。subjectはtitleそのまま、shell=Falseのargvなので日本語/引用/$()/backtickを展開しない。任意subject、admin/auto、branch削除を設けない。
- merge終了0だけで完了としない。同repoの番号/headがMERGEDで40桁mergeCommitが得られたことを再読する。非0/timeout/未完了を再試行せず出力を残し、成否不明なら実状態を確認する。

### 差分から選んだ検証とレビュー

| コマンド・範囲 | 回帰リスクと選定理由 | 実結果 |
| --- | --- | --- |
| `python -X utf8 -m unittest tests.conformance.test_merge_pr` | PR900/Issue42の別番号、件名argv、plan無mutation、実行一度、head/状態/基準/record/required/rules/queue/完了確認の停止境界 | 初版7tests0。独立P2修正後の新10tests初回0、0.026s。ネット/実merge/製品実行なしのfake runner試験 |
| `python -X utf8 -m unittest tests.conformance.test_conformance.GitChecks tests.conformance.test_verification_policy.VerificationPolicy.test_record_and_reuse_are_accepted tests.conformance.test_verification_policy.VerificationPolicy.test_missing_or_blank_fields_are_rejected` | 新入口が共用する既存title形式/4record欄を保つ | 直接6tests初回0、0.003s。既存2関数/入力不変で修正後も再利用 |
| `python -X utf8 eng/merge-pr.py --help` | CLI入口と任意subject引数を設けない契約 | 初回0。parse不変で修正後再実行なし。PR番号0/短いhead/任意subject拒否は上の限定testで確認 |
| `python -X utf8 OUT/source-proof.py` | 共通validatorや既存製品・gateを改変しない | ASTでvalidate/validate_pr不変、新helperだけ。src/既存tests/CI/hooks/CMake/規則/基準/依存許可の差分0、3code/test SHA保存 |
| `gh api --hostname github.com repos/hideyukiMORI/nene-nib/rules/branches/main` | ghのqueue暗黙挙動を当repoへ誤適用しない | 読取0、deletion/non_fast_forward/pull_request(squashのみ)/required_status_checks(check)の4rules、queueなし。道具も各実行でrulesを確認 |
| 文書の限定CNF-006/waiverと`git diff --check` | Git操作の手順・実施記録を整合させる | 違反0/waiver none。製品build/GUI/速度/full gateは未実行 |

初版の7tests成功後に必要な独立読取レビューを実施し、P2を2件受理した。暗黙repoにより同一番号/headを持つfork等を識別できない点と、validateのsplitlines先頭/CRLF限定単行判定/全string末尾Issue抽出がずれる点。後者は`fix(git): 確認 (#42)`+U+2028+`追加 (#99)`とCloses #99が誤受理されることを純粋関数で実再現した。GitHub mutationなし。初版3files/7成功/原反例JSONをOUT/initial-sourceとrawへ保持する。

修正はrepo明示固定、`title.splitlines()==[title]`で論理行の統一、rules/queueと実行後完了の境界。新10testsはUnicode改行8種と実反例、全PR callsのrepo、API host/path、rules failure/queue/終了0後未完了も確認する。改訂の独立読取レビューを経て技術受理。レビュー者はtests/gh/API/build/editを実施せず、保存結果と差分だけを確認した。新10＋既存6の直接16testsが今回の対象で、全件検証を意味しない。

### 通常統合・再利用・限界

当PRをdraft→ready→必須CIの通常経路へ出し、新道具のplanで対象/head/title/required/rulesを確認してからexecuteする。実plan/merge/完了read/CI/main同期はacceptance-384へ追記する。ここに未実施の実mergeを成功と先記しない。製品と既存CIは不変でアプリ試験を実行しない。新道具の成功結果もcode/tests/依存が不変なら工程・文書・SHAだけで繰り返さない。

PR metadata/rulesのreadとmergeは原子的ではなく、操作中に並行変更しない前提が残る。headだけはGitHubのmatchで競合を拒否する。queue未対応は失敗として止める。外部UI/直接CLIまで同じ入口を強制できず、GITのplanned/不能をactiveへ替えない。required成功と検証記録の真偽を同一視しない。履歴誤記は原記録として維持し、規約を緩めて正当化しない。通常統合/収載後にDの絶対path/取込/未保存・ignored/唯一成果/稼働/linkを監査整理、branch/commit保持。以後はhideの停止まで速度・安定性・品質の改善を続ける。
