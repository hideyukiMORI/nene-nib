# ADR 0021 — 差分に必要な検証と成功結果の再利用

- 状態: 受理
- 日付: 2026-09-20
- Issue: #61
- 根拠: hide の明示指示。無関係な検証と工程ごとの再実行を減らす。
- 影響する規則: QLT-001 / QLT-002 / QLT-007 / QLT-009 / QLT-010 / QLT-012 / QLT-014 / GIT-004

## 決定

CI頻度と文書のみの免除は[ADR 0118](0118-ci-only-at-merge-except-documentation.md)が更新する。以下の差分検証・結果再利用の原則は維持する。

1. 完了条件は、差分の挙動と直接の依存・呼び出し元で起こり得る退行を検出する最小限の検証と、その記録・レビュー。各テストの必要性を説明できなければ実行しない。既存の CMake target、CTest `-R`、unittest の対象指定、個別の eng コマンドを使う。
2. 検証済みの実装・テスト・関連依存・必要な環境条件が不変なら、担当・工程・文書・コミット SHA が変わっても成功結果を再利用する。PR に対象、退行の根拠、コマンドと結果、記録の場所、再利用元と不変性の根拠を書く。関連する変更・失敗・具体的な未確認事項だけが再実行の理由になる。
3. `eng/check.ps1` の全件検証は `-Full -Reason <限定できない理由>` の明示指定でのみ実行する。指定なし・空の理由はツール初期化前に拒否する。実行前に対象と理由を短く知らせ、追加承認を定型化しない。既存の検査内容・閾値・警告・抑制・テストは変えない。
4. CI の必須 `check` は Git 規約、PR の検証記録の存在、差分の空白を確認する。製品テストは重ねて走らせない。記録の妥当性と入力の不変性はレビューで確認し、機械がテストの適切さまで証明したとは扱わない。PR 編集時は変わったメタデータの確認だけを行う。既存の pre-commit / commit-msg は軽い検査のまま維持し、push / merge フックにテストを足さない。
5. 差分が原因の失敗は修正する。無関係な既存失敗は根拠と別 Issue を記録して本件を継続する。関係のない全件再試行・別件修正や、成功するまでの反復で不安定さを隠すことを禁止する。
6. 文書・コメント・規約だけならアプリの動作検証は不要。フック・開発ツールは変更した道具の正例・反例を短く確認する。coverage・性能・negative proof も関連する変更時に選び、過去の成功を工程ごとに測り直さない。

## 既存決定との関係

ADR 0001 / 0003 / 0006 / 0011 / 0016 / 0019 と過去の日報・引き継ぎ・実測記録にある「常時全件」「Ready 直前・最終 HEAD ごと」「CI で毎回」の実行頻度は本 ADR が置き換える。検査の判定基準や過去の実測結果は変更しない。設定機能の Issue #60 は別の作業ツリーに保持する。

## 検証

対象は check.ps1 の明示指定、PR 記録の検査、CI 配線、文書整合。アプリのビルド・動作・性能・coverage は実行しない。実測結果は PR と quality/gate-proofs.md に記録する。

## 却下

- 全件テストをローカルと CI の双方で繰り返す: 不変の入力を工程だけで測り直す。
- 大規模な自動選定・キャッシュ基盤: この更新の範囲を超える。
- 全検査を無効化する: 必要な退行確認と Git・検証記録の確認を失う。

## 追補（2026-11-04・#1220・hide の判断 2026-11-03）: まとめ枝 — レビュー済みの PR を 1 本に積み、CI 1 回で rebase merge する

### 文脈

1 本ずつ「main の上に乗せ替え → CI（30〜45 分）→ squash」の形は、レビュー済みの PR が 17 本並ぶと CI だけで 10 時間を超え、1 日の実装より長い（2026-11-03 の実測・[日報](../reports/2026-11-03.md)）。hide の判断（2026-11-03 23:4x）: 「ブランチを新たに建ててまとめられるものをそこでマージして回す時間を少なくする。リポジトリの決まりをそろそろ緩めないと実装速度がどんどん遅くなる」。

### 決定

1. **まとめ枝は 1 本の PR で、1 PR = 1 commit。** 枝の名前は GIT-002 の形 `chore/<まとめ Issue>-integration-<yyyymmdd>`（CI の `validate-git.ps1` が head の枝の名前を見る）。レビュー済みの PR を列の順に `git merge --squash <その PR の head>` で積み、commit の件名は **元の PR の題**（GIT-003・Issue 番号つき）、本文は `Closes #N`（題の番号と同じ 1 つ）・`PR #K（head <sha>）を squash と同じ中身でまとめ枝 #<まとめ Issue> に積む。`・元の PR の commit の列（GitHub の squash の既定と同じ `* <message>`）。親の PR の上に積んだ子の PR は、親を積んだ後に子の head を積む（3-way merge が親の分を同じ変更として畳む）。衝突はまとめ枝で 1 回だけ解き、解き方は「両側の和」でなく**基の版に差し込む**（和型の末尾・操作の表の行・`NibTests.cpp` の件数・2026-11-03 の記録 §5.4）。
2. **検証はまとめ枝で 1 回。** 元の PR ごとの独立レビューと検証記録は済んでいることが前提で、まとめ枝では「積んだ結果の木」を確かめる: 手元の Debug の `nib_tests` ＋ `NeNeNib` ＋ `nib_perf_probes` のビルド（警告 0）・`nib_tests` の既定実行・`python eng/conformance.py --build-dir build`・`python eng/symbols.py --build-dir build --require core application`・`eng/format-check.ps1`・衝突を解いた PR の契約の selector。CI は `check.yml` の 1 回（定義は変えない・`display_title` はまとめ枝の PR の番号と head）。元の PR の検証記録は ADR 0021 の 2 のとおり再利用し、まとめ枝の PR の本文には積んだ PR の列と衝突を解いた所と上の検証の結果を書く。
3. **main へは rebase merge。** 正典の道具は `eng/merge-pr.py --integration`（別の道具を作らない・ARC-001）。確かめること: base が `main`・OPEN・ready・題が GIT-003 でまとめ Issue を指し本文の `Closes` はそれ 1 つ（1 本の PR と同じ検査）・**head の全 commit が GIT-003 の形で、本文に件名と同じ番号の `Closes #N` を持つ**・枝が main の先端を含む（`compare` の `behind_by == 0`。rebase merge が作り直す commit の木は CI が通した木と同じ）・ruleset の `allowed_merge_methods` に `rebase` がある・CI が緑（`run_ci` は 1 本の PR と同じ）。merge は `gh pr merge --rebase --match-head-commit <head>`。merge の後に `compare/<base>...<mergeCommit>` の commit の数が PR の commit の数と同じことを読み取り、積んだ元の PR（commit の本文の `PR #K`）を「main の commit <sha> として入った」のコメントで閉じる（Issue は commit の `Closes` で GitHub が閉じる・元の PR は head が main に無いので自動では閉じない）。
4. **ruleset は `allowed_merge_methods: ["squash", "rebase"]`。** リポジトリの `allow_rebase_merge` も on（2026-11-04 07:13 JST に設計席が API で変更・hide の判断の実行・[gate-proofs](../quality/gate-proofs.md) に記録）。1 本の PR の merge の形（squash・PR ごとの CI）は変えない。merge queue・auto merge・全 PR 一律の required status check は置かない（ADR 0118）。GitHub UI からの merge まで機械強制しない（GIT-004 の planned のまま）。
5. **緩めるのは「PR ごとの CI」だけ。** 保つのは「CI は merge される状態そのものを通した」（まとめ枝 = main の先端 ＋ 積んだ commit で、rebase merge は木を変えない）と「main の履歴は 1 commit = 1 Issue」の 2 つ。レビューの済んでいない PR・設計を変える PR（ADR の受理が要るもの）・速さの受理（D41）の要るものは、まとめ枝に積まない（1 本ずつ）。
6. **道具の実装で決めた 2 点（PR #1221・2026-11-04）。** merge の直前（CI の後）に `compare main...head` をもう一度読み、CI の間に main が進んでいれば merge しない（決定 3 の `behind_by == 0` を CI の後にも当てる）。merge 後の照合の起点は PR の `baseRefOid` でなく、その compare の `merge_base_commit`（behind 0 のとき main の先端そのもの）。GitHub の `pulls/<n>/commits` と `compare` は 250 commit で打ち切られるので、まとめ枝は 250 commit まで（超えれば道具が拒む・安全側）。
7. **道具の試験**は `tests/conformance/test_merge_pr.py`（`--integration` の受理と、rebase を許さない ruleset・GIT-003 でない commit・`Closes` の無い / 番号の違う commit・main より遅れた枝・commit の数の不一致・CI 失敗の拒否）。
8. **merge の呼び出しの待ち直しと閉じる側の詰め（#1223・2026-11-04 のまとめ枝 3 で 502 → 「Merge already in progress」→ 3 回目で入った）。** `gh pr merge` が非 0 で返り、その出力が GitHub の 5xx（502 Bad Gateway など）か `Merge already in progress` のときだけ、PR を読み直す: MERGED なら merge 後の照合へ進み、OPEN かつ番号と head が同じなら 30 秒待って**同じ merge の命令**を呼び直す（最大 10 回・新しい CI は起こさない・`--ci-run` も要らない）。head や番号が違う・OPEN でも MERGED でもない・10 回で尽きた、はどれも「成否不明・inspect the PR」で終了 2。ほかの失敗（mergeable でない・head が変わった・422 など）は従来どおり待たずに終了コードを返す。旗なしの道（1 本の PR の squash）も同じ関数 `merge_once` を通る。あわせて `--integration` の詰め: 閉じる語は `Closes` だけでなく GitHub の閉じる語の全部（close / closes / closed / fix / fixes / fixed / resolve / resolves / resolved・大小を問わず・`owner/repo#N` と Issue の URL も）を読み、件名の番号以外を指すものがあれば拒否する（rebase merge は commit の本文をそのまま main に置くので、GitHub が別の Issue を閉じる）。元の PR と main の commit は件名でなく**積んだ順**（PR の commit の順 = `compare` の commits の順・数は照合済み）で対応させ、同じ位置の件名が違えば閉じずに `notClosed` に出す。`PR #K（head <sha>）` の行が無い・形が違う commit も黙って飛ばさず `notClosed` に出す。元の PR を読む・閉じる呼び出しが `OSError` / timeout で落ちても、その PR を `notClosed` に積んで次へ進む（閉じた PR と閉じなかった PR の一覧を必ず出す）。`ahead_by` が 250 を超えて数が合わないときは、拒否の文に GitHub の上限 250 を書く。

### 置き換える記述

GIT-004 の「マージは squash のみ」→「1 本の PR は squash・まとめ枝は rebase」。ADR 0118 の「PR必須、squashのみ…は維持する」の squash は、この追補の rebase を含む。QUALITY_GATES の ruleset の記述も同じ。
ADR 0118 の「失敗・成否不明を自動再試行しない」には、決定 8 の待ち直し（merge の呼び出しが GitHub の 5xx / `Merge already in progress` で落ち、PR が OPEN かつ head 不変のときだけ・同じ命令・最大 10 回・新しい CI なし）を例外として足す（#1223）。DEVELOPMENT_WORKFLOW と COMMIT_CONVENTIONS の「失敗を再試行しない」も同じ。

### 残るリスク

最後の `compare` から merge までの数秒の間に main が進むと CI と違う木が入り得る（merge 後の commit の数の照合で必ず気づき終了 2・inspect main・道具では閉じられない・#1223 の R1-1）。決定 8 の待ち直しの間（最大約 5 分）も同じで、GitHub が最初の呼び出しを遅れて処理し終えていれば MERGED として照合へ進み、main が進んでいれば数の照合で気づく。rebase merge は GitHub が commit を作り直すので SHA が変わり committer は GitHub になる（squash でも同じ）。まとめ枝に積んだ後に元の PR へ push があっても道具は気づかない（積んだ head を commit の本文に書き、閉じるときのコメントで照合できる）。衝突の解き方の誤りは、まとめ枝の手元の検証と CI が受け止める。
