# 5-cn — 表示幅索引のmain構成への分離（Issue #294）

出典: docs/quality/gate-proofs.md の節 5-cn（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-05夜のhideの「可能なものはマージして」を受け、#291の`c637acc`から幅索引だけをmain `936b50b`上へ分離した。ソース`36a1c9c`、変更は`VirtualColumn.cpp`・`DisplayLineTests.cpp`・ADR 0074。本文cache・字体・字形・splitは含まない。

**判定: HOLD。main未統合。** 固定54回のうち52回が有効。`empty-1-O`は212入力、`16mib-2-O`は206入力で予定202入力に一致せず無効。最初の空文書の撮影にも予定外の本文がある。入力の発生元は特定していない。この2回を正常値へ補完せず、全原記録を保存した。

| 対象 | main O → 幅索引 A・中央値 ms | 判定 |
| --- | --- | --- |
| 空文書 単打 / 200文字 | 1.061 / 4.297 → 1.1305 / 4.414 | Oは5回の記述値。A単打の前後半差0.276 > 0.11305、burst差0.897 > 0.5。保留 |
| 16MiB短行 単打 / 200文字 | 2.244 / 11.057 → 1.853 / 7.6915 | Oは5回の記述値。比較不完備のため保留 |
| 日本語長行 単打 / 200文字 | 317.240 / 466.047 → 314.946 / 399.7125 | 両版6回有効、安定条件と中央値増分は事前条件内 |

範囲と退行の根拠: 幅の意味と直接の利用先（表示行・Vim仮想桁）、main描画と組み合わせた入力費用。全件テストは選んでいない。入力受領からPresent復帰までであり、光の遅延ではない。

- `git diff --exit-code c637acc 36a1c9c -- src/core src/application tests eng`成功。CMakeの差は未採用UI翻訳単位の登録だけで、対象のcompile設定/依存に変更なし。既存`build/debug-width/nib_tests.exe --display-line` 1114301 checks、`--vim-virtual-column` 424 checksを再利用（元の実行ログもarchiveに保存）。コマンドの元のbuild所在は#291のarchiveとRelease記録を参照。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`成功、162.307秒、1368064 byte、SHA-256 `18D1D1013A0E94060EF753F84B662536428D33D75BA2C88D30F2208095BE5190`。`out/release/36a1c9c.json`。
- main `936b50b`と既存Release `591b99f`の`src tests eng CMakeLists.txt`一致を`git diff --exit-code`で確認して基準exeを再利用。
- `python -B D:/NeNeNib/scripts/294-295-main-comparison-20261005.py --output D:/NeNeNib/outputs/294-295-main-comparison-20261005 --run`は54回完了後に無効2回のassertで終了1。予定・入力・版・依存hashを先に固定。再試行なし。`294-295-main-audit-20261005.py`成功、全54原記録を再集計しHOLDを確認。空文書O/Aは929画素差（予定外入力）、16MiB/長行のO/Aは本文・ステータス0画素差。
- `294-295-isolated-static-20261005.py`成功、変更2ソース・所有/依存・文書/waiverで0 findings。LLVM 19.1.5の`git-clang-format --diff 936b50b 36a1c9c -- src/core/VirtualColumn.cpp tests/unit/DisplayLineTests.cpp`は差分なし。`git diff --check`成功。
- `python -B eng/protected-diff.py --base 936b50b --head 36a1c9c`成功。fixture 1853→1853、metadata/deleted/changed/added各0、protected files none。scope33は未測33（全件checks同一とは主張しない）。`out/protected/36a1c9c.json`。

規則: ARC-001 / CPP-016 / QLT-001 / QLT-004 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。保存schema・基準値・ゲートは変更なし。Waivers: none。元の`fix/291-long-line-rendering`とPR #293は未統合のまま。

機械記録: [main-isolation-2026-10-05.json](../main-isolation-2026-10-05.json)。全証拠は`D:/NeNeNib/evidence/294-295-main-isolation-20261005/main-isolation-evidence.zip`（306 files + manifest、SHA-256 `e4afca114af9083a75a61ca744c72ac7829a30fa70c65c3be84b3c4125975717`）。全entry hash / ZIP CRC / bundle検証成功。候補ソース・3版の実測exe・全raw・22場面比較・失敗記録を保存。
