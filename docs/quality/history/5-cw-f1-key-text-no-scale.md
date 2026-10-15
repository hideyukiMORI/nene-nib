# 5-cw — F1 の鍵の文字を本文の倍率で拡大しない（Issue #312・ADR 0078 決定 12）

出典: docs/quality/gate-proofs.md の節 5-cw（2026-10-16 に D50 で移動・本文は移動時のまま）

[PR #316](https://github.com/hideyukiMORI/nene-nib/pull/316)。hide の指示で、設計・受理・撮影は設計リナ（Fable）、実装と自動の検証は背景の実装リナ（Opus・1 工程 1 席）が担当した。この分担を次の既定にはしない。

**変更と確認する退行。** `create_body_formats` の鍵の書式 `key_format_` が `ui_text_dips * ratio`（本文の倍率つき）だった 1 か所を `ui_text_dips`（12 DIP 固定）にした。面の行の名前・説明（`command_format_`）と右端の欄（`palette_row_note`・112 DIP）は本文の大きさに追従しないので、鍵だけが追従すると 24 pt で `Ctrl+Shift+S` が欄に入らず切れていた。既定の 13.5 pt は倍率 1.0 なので画は変わらない。gutter の書式は倍率を掛けたまま。ADR 0078 の決定 12 に 1 文を足した。core・application・操作表・案内の鍵（`guide_key_format_`）は触らない。

| 対象・退行 | 実行したコマンド | 結果 |
| --- | --- | --- |
| 書式の生成・警告集合・clang-tidy | 固定 toolchain で Debug の `cmake --build build --target NeNeNib` | 成功・警告 0（`out/312-build2.log`） |
| 層・実依存・正準整形 | `python eng/conformance.py`・`python eng/conformance.py --build-dir build`・変更 C++ の `clang-format --dry-run --Werror`・`git diff --check` | 0 violation・差分なし（最初の `--build-dir` は CMake File API の query を置かずに configure したため ARC-002 で 1 件落ち、query を置いて再 configure して 0。コード起因ではない） |
| 単体テスト | 実行しない | core と application を触らず、ui/win32 の書式に対象の試験は無い |

**Release と画。** `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `315b153`（rebase 前の commit。rebase 後の HEAD と製品ソース・試験・道具は `git diff --exit-code 315b153 HEAD -- src tests eng CMakeLists.txt` で同一）、SHA-256 `e2919e141466afdc606cc4c6bd2051d617c05bda6a14c8dccc55a75fb9441f32`、1430528 bytes。専用 profile・実機 120 DPI・機械がほぼ空いていること（CPU 1〜7%）を hide に確かめてから、2026-10-09 深夜に設計席が実行した。

- `python -B D:/NeNeNib/scripts/312-keycap-verify.py --mode capture --root <worktree> --name after-315b153 --executable <Release>` → 終了 0・5 場面（F1 → `ctrl+shift+s`。dark 13.5 / light 13.5 / dark 24 / dark 8 / dark 40）。
- 同じ道具の `--mode compare --reference D:/NeNeNib/outputs/304-guide/base-f1 --candidate D:/NeNeNib/outputs/312-keycap/after-315b153 --expect-same dark-13.5,light-13.5 --expect-different dark-24` → 終了 0。基準は #304 の `base-f1`（Release `4d84334`・5-cv で `c89cf00` と F1 の領域が 0 画素差）。

| 場面 | F1 の領域の差分画素 | 判定 |
| --- | --- | --- |
| dark-13.5 | 0 | 期待どおり同じ |
| light-13.5 | 0 | 期待どおり同じ |
| dark-24 | 1606（範囲 x 865・y 196・w 139・h 28 = 鍵の箱だけ） | 期待どおり違う。基準では `Ctrl+Shi` で切れ、候補では `Ctrl+Shift+S` が欄に収まる |
| dark-8 / dark-40 | 比較なし | 設計席が目視。鍵は 12 DIP のままで欄に収まり、行の名前・説明も不変 |

面の外（タブの帯・本文・ステータスバー）の差分は 5 場面とも 0。画は `D:/NeNeNib/outputs/312-keycap/after-315b153/`、判定は `compare-315b153/record.json`（`visualReview` は pending のままで、目視の合格はここに書く）。

**速さは測っていない。** 差分は鍵の書式の大きさだけで、起動・打鍵・開く道の呼び出しの回数と経路は不変（`make_format` の回数も同じ）。QLT-014 の 8 本に関わる差分ではない。

**再利用。** Release の後は文書だけ（ADR 0078 の 1 文・この節・current.md）で、`git diff --exit-code 315b153 HEAD -- src tests eng CMakeLists.txt` の一致をもって Ready / merge で再利用する。全件検証は実行していない。恒久記録: 席の報告と log は `D:/NeNeNib/evidence/312-palette-keycap-size/`、依頼書は `D:/NeNeNib/briefs/impl-312-rina-20261008.md`。

適用: #312 / ADR 0078 決定 12 / ARC-001 / ARC-012 / CPP-017 / QLT-001 / QLT-012 / GIT-003 / GIT-004。Waivers: none。
