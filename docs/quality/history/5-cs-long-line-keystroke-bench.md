# 5-cs — 長い行の 1 打鍵のベンチと実機の基準値（Issue #298・ADR 0075）

出典: docs/quality/gate-proofs.md の節 5-cs（2026-10-16 に D50 で移動・本文は移動時のまま）

#291 で縮めた長い日本語行の 1 打鍵を、速さのゲート（QLT-014）の 8 本目 `key-to-frame-single-long-line` として足した。実装は実装席（Opus）、測定と採用は設計席。

- 確認する退行: 文書が #291 の fixture と 1 バイトでも違うこと・既存 7 本の選択と採用の振る舞い・既存の基準値と許容の書き換え。製品のコードは触っていない。
- 文書: 道具が `out/speed/long-japanese.txt` を作り、SHA-256 `ef2d1511a696984dcd4184498a852d944a8ca366bf847f440d1e46b73b32a6c5`（6,219,000 bytes）を照合する。実装席が文書を作る関数だけを呼び、`D:/NeNeNib/evidence/290-split/documents/long-japanese.txt` と `cmp` で同一を確かめた。
- `python -m unittest discover -s tests/conformance -p test_speed.py` → 74 tests 成功（64 → 74）。実装席が置いた「どの機械にも長行の基準値が無い」という試験は採用の前の状態を固定するものだったので、採用と同じ commit で「実機の基準値があり、許容は床が効く」に置き換えた（QUALITY_GATES の文と基準値がずれたら落ちる）。`python eng/conformance.py` → 0 violation(s)。
- 実機の測定（hide の了承の後・1 回だけ）: Release `f42a95d`（SHA-256 `44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900` を直前に照合。main `143284d` の `src tests CMakeLists.txt` は `f42a95d` と同じ・`git diff --exit-code` 終了 0）。実行の直前にビルドとテストのプロセス 0 件を確かめた。
  `python eng/measure-speed.py --check --bench key-to-frame-single-long-line --executable <exe>` → 5 試行すべて有効、中央値 **5.431 ms**（5.348〜6.658・samples 6.658 / 5.348 / 5.817 / 5.431 / 5.356）。基準値が無いので `recorded only` と 1 行言って終了 0。記録 `out/speed/2026-10-06T14-40-13Z.json`（worktree `D:/NeNeNib/worktrees/298-long-line-bench`）。
- 採用: `python eng/measure-speed.py --adopt --bench key-to-frame-single-long-line --values <記録>` で、実機 `bc8a356f37c68491` のこの 1 本だけを足した。`git diff eng/perf-reference.json` は足した 5 行だけで、既存 7 本の値と許容（25% / 床 2 ms）は不変。`--check --bench … --values <記録>` → 1 benches checked・0 regression・0 unmeasurable（採用した値と比較器の整合の確認で、新しい測定ではない）。
- 5-cm の固定実験の 7.27 ms とは窓の大きさと試行の形が違うので、値を比べない。この基準値は計測器 `eng/measure-speed.py` の条件での値。
- 許容は床 2 ms が効く（5.431 ms の 25% は 1.36 ms）。7.431 ms を越えると落ちる。#291 より前の約 326 ms への後戻りは確実に落とせるが、1 ms 台の悪化は見えない。
- 残り: CI の指紋 `e7a87d5b` は記録だけ。200 文字の長行版は足していない。

適用: #298 / ADR 0011 / ADR 0016 / ADR 0075 決定 7 / QLT-001 / QLT-012 / QLT-013 / QLT-014。Waivers: none。
