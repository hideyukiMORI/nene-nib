# ADR 0143 — マージ直前の CI は main の workflow 定義で起動し、PR の head は入力で checkout する

- 状態: 受理（設計席 2026-10-17。#629 で道具を直し、新しい `merge-pr.py` が起動した最初の非文書の merge #636 の run [39028503021](https://github.com/hideyukiMORI/nene-nib/actions/runs/39028503021) が `workflow_dispatch` / `head_branch=main` で `check` 33 秒・`build-and-test` 11 分 38 秒の 2 job を通した・gate-proofs 1-f）
- 日付: 2026-10-17
- Issue: #599（ADR 0136 の穴・2026-10-16 夜の merge で発見）
- 規則: ARC-001 / QLT-012、ADR 0118 / 0136

## 文脈

`eng/merge-pr.py` の `run_ci` は `check.yml` を `workflow_dispatch` で起動するとき `ref=<PR の head branch>` を渡す。GitHub は workflow の**定義**をその ref の `check.yml` から読むので、#522（ADR 0136・`build-and-test` job）より前に切られた branch では `check` job だけが走り、製品の Release ビルドと単体テストが走らない。実例: run 38911344820（#477）と 38911353132（#505・merge 済み）は `check` だけ 30 秒で緑だった。#505 は設計席が main を手元で Release 全 target ビルド＋ctest 9/9 して補った。

今日の照合（ADR 0118）は run の `head_branch` と `head_sha` を PR の head と比べる。これは「起動した ref」が PR の head であることの確認で、「どの定義で走ったか」は見ていない。一方 workflow の step は `inputs.expected_head` を checkout し、`eng/validate-git.ps1` が PR の head と checkout の head が `expected_head` と一致することを確かめている。つまり検査の対象は既に入力で決まっていて、ref は定義を選ぶためだけに使われている。

## 決定

1. **dispatch の ref は `main` の 1 つ。** `run_ci` は `ref=main` で起動する。定義は main の `check.yml` だけで（ARC-001）、古い branch でも `build-and-test` が走る。検査の対象は今日どおり `inputs.expected_head`（PR の head）で、workflow の step（checkout の ref・`validate-git.ps1`）は変えない。
2. **run の照合は入力で行う。** 期待する値は `event = workflow_dispatch`・`path = .github/workflows/check.yml`・`head_branch = main`・`display_title = Merge PR #<番号> at <expected head>`・`repository = 正典のリポジトリ`・`conclusion = success`。`head_sha` は main の先端になるので PR の head と比べない（`display_title` は `run-name` が `inputs` から作る値で、入力の head を証明する）。それ以外の照合（実行前後の PR と rules の比較・再試行しない・`--ci-run` の引き継ぎ）は ADR 0118 のまま。
   - この ADR が置き換える記述: ADR 0118 の「fork の head を手動 workflow の ref にできないため同 repo の branch だけを受け付ける」（理由は変わるが、同 repo の branch だけという制限は `validate-git.ps1` の PR の照合のために残す）と、ADR 0136 の決定 4「`merge-pr.py` は変えない」。
3. **PR が `check.yml` 自身を変えるときは、その PR の定義では走らない。** 変更後の定義は merge の後の最初の run で確かめる（ADR 0136 の「初回の run が通った時点で受理」と同じ形）。別の道（そのときだけ head の ref で起動する）は作らない。
4. **道具の試験は同じ変更で直す。** `tests/conformance/test_merge_pr.py` は dispatch の `ref=main` と、`head_branch` が `main` でない run・`display_title` が違う run の拒否を確かめる。`head_sha` の不一致は拒否の理由から外す。

## 却下した選択肢

- B: dispatch の前に PR の head の `check.yml` が main と同じ内容か確かめ、違えば拒否して rebase を求める。定義の正本が「head と main の一致」という 2 か所の条件になり、rebase を強いるだけで定義の穴は塞がらない。
- `check.yml` を変える PR だけ head の ref で起動する。起動の道が 2 本になる（ARC-001）。
- required status check へ戻す。ADR 0118 が文書免除のために外した。

## 検証

- `python eng/test-conformance.py` の `test_merge_pr.py` が通ること。
- この PR 自身の merge で、`merge-pr.py --execute` が `ref=main` で起動した run に `check` と `build-and-test` の 2 job があること（run ID と所要時間を gate-proofs に 1 行）。ただし道具の変更だけなので文書扱いにはならない（非文書）。

## 限界

- CI の成功は製品の動作・検証記録の真実性を保証しない（ADR 0118 / 0136 と同じ）。
- `check.yml` の変更そのものは、その PR の merge 前には検査されない（決定 3）。
