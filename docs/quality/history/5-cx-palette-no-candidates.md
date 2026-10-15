# 5-cx — Ctrl+P の面の「候補なし」を行の名前の欄に書く（Issue #309・ADR 0060 決定 9）

出典: docs/quality/gate-proofs.md の節 5-cx（2026-10-16 に D50 で移動・本文は移動時のまま）

[PR #317](https://github.com/hideyukiMORI/nene-nib/pull/317)。#312 と同じ分担（設計・受理・撮影は設計リナ（Fable）、実装と自動の検証は背景の実装リナ（Opus・1 工程 1 席））。次の既定にはしない。

**変更と確認する退行。** `draw_palette_choices` の候補 0 件の枝が「候補なし」を行の矩形そのもの（`palette_row`）に書いていたのを、候補のある行の名前が使う内側の欄 `palette_row_label(row, dpi, false)` に変えた 1 か所。書式（`mode_format_`）・色（`muted`）・文言・候補のある行の描画（`draw_palette_choice`）は不変。core は触らない（配置の関数はもうある）。退行として見るのは「候補のある面の画が 1 画素も変わらない」こと。

| 対象・退行 | 実行したコマンド | 結果 |
| --- | --- | --- |
| 翻訳単位・警告集合・clang-tidy | 固定 toolchain で Debug の `cmake --build build --target NeNeNib` | 成功・警告 0（`out/309-build.log`） |
| 層・実依存・正準整形 | `python eng/conformance.py`・`python eng/conformance.py --build-dir build`・変更 C++ の `clang-format --dry-run --Werror`・`git diff --check` | 0 violation・差分なし（`--build-dir` の最初の 1 回は File API の query 不在で ARC-002 に 1 件。query を置いて再 configure し 0。5-cw と同じ） |
| 単体テスト | 実行しない | core を触らず、ui/win32 の矩形の選び方に対象の試験は無い |

**Release と画。** `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `b9ef1b1`（rebase 前の commit。rebase 後の HEAD と製品ソース・試験・道具は `git diff --exit-code b9ef1b1 HEAD -- src tests eng CMakeLists.txt` で同一）、SHA-256 `5C62D74828D96F77706F6A384C0DDDE6E59D36C71D7221DDD6FA6D2F658112A0`、1430528 bytes。before は #304 の Release `c89cf00`（SHA-256 `94654A18…45A0BC`・製品ソースは main と同一）。専用 profile・実機 120 DPI・機械がほぼ空いた状態で、2026-10-09 深夜に設計席が実行した。

- `python -B D:/NeNeNib/scripts/309-empty-label-verify.py --mode capture --root <worktree> --name before --executable <c89cf00>` と `--name after --executable <b9ef1b1>` → どちらも終了 0・4 場面（`dark-empty`: F1 → `zzzz`・`light-empty`: `neutral-light` で同じ・`dark-one`: F1 → `ctrl+shift+s`・`dark-ctrlp-empty`: Ctrl+P → `zzzz`。本文 13.5 pt）。
- `--mode compare --reference .../before --candidate .../after --expect-same dark-one --expect-different dark-empty,light-empty,dark-ctrlp-empty` → 終了 0。

| 場面 | 面の領域の差分画素 | 判定 |
| --- | --- | --- |
| dark-one（候補 1 件） | 0 | 期待どおり同じ |
| dark-empty | 661 | 期待どおり違う。「候補なし」が面の左端から行の名前の左端（候補のある行の名前と同じ x）へ移った |
| light-empty | 661 | 同上（ライト） |
| dark-ctrlp-empty | 661 | 同上（Ctrl+P の面） |

面の外の差分は 4 場面とも 0。`zzzz` は F1 の操作の一覧にも Ctrl+P の候補にも当たらず 0 / 0 と出ることを after の画で確認した。画は `D:/NeNeNib/outputs/309-empty-label/{before,after}/`、判定は `compare/record.json`（`visualReview` は pending のままで、目視の合格はここに書く）。

**速さは測っていない。** 差分は 0 件の面を描くときの矩形の選び方だけで、QLT-014 の 8 本の経路（起動・打鍵・開く・候補 5000 件の面）に 0 件の面は無い。

**再利用。** Release の後は文書だけ（この節・current.md）で、`git diff --exit-code b9ef1b1 HEAD -- src tests eng CMakeLists.txt` の一致をもって Ready / merge で再利用する。全件検証は実行していない。恒久記録: 席の報告と log と Release は `D:/NeNeNib/evidence/309-empty-palette-label/`、依頼書は `D:/NeNeNib/briefs/impl-309-rina-20261008.md`。

適用: #309 / ADR 0060 決定 9 / ADR 0008 / ARC-001 / CPP-004 / CPP-017 / QLT-001 / QLT-012 / GIT-003 / GIT-004。Waivers: none。
