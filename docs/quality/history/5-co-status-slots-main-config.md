# 5-co — ステータス固定枠のmain構成への分離（Issue #295）

出典: docs/quality/gate-proofs.md の節 5-co（2026-10-16 に D50 で移動・本文は移動時のまま）

幅索引のみの`36a1c9c`へ、#291の`0dfad49`からステータス保持だけを分離した`1883e27`。変更は`Direct2DRenderer.cpp/.hpp`・`StatusTextLayout.hpp`・ADR 0070。本文はmainの既存経路で、本文cache・重複無効化削減・字体cache・字形保持・splitは含まない。

**判定: HOLD。main未統合。** #294に依存するDraft候補。固定比較のA/Sは各18回すべて有効だが、空文書単打の前後半差はA 0.276 > 0.11305ms、S 0.112 > 0.1ms。A burstも0.897 > 0.5ms。中央値改善だけでは安定条件を代替しない。#294自体にも基準側欠測がある。原記録と予定は5-cnの同一archiveへ保存し、再試行・基準更新なし。

| 対象 | 幅索引 A → ステータス追加 S・中央値 ms |
| --- | --- |
| 空文書 単打 / 200文字 | 1.1305 / 4.414 → 0.622 / 4.045 |
| 16MiB短行 単打 / 200文字 | 1.853 / 7.6915 → 1.5185 / 7.932 |
| 日本語長行 単打 / 200文字 | 314.946 / 399.7125 → 314.8175 / 396.613 |

範囲と退行の根拠: 7枠の上限・文字列/書式/寸法照合、表示の更新、main本文経路との描画費用。無関係なcore/IME/split全件は実行していない。

- `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`成功、143.992秒、1372160 byte、SHA-256 `B4B8FBFFAB0ABE41E39508E2EE9E37EBAFD9BFF37B2BF37135C8438080C7779D`。`out/release/1883e27.json`。
- `python -B D:/NeNeNib/scripts/295-status-isolated-visual-20261005.py --exe <A/Sの実測exe> --name <A/B>`を各1回、`--compare`成功。通常/Vim・桁上がり・7枠録画・通知・Ex入力・幅・テーマ・フォント・palette切替など22場面/44撮影で、本文とステータスは全場面0画素差。刺激で変化する場面が変わることも確認。原点/色/文字更新の直接境界を確認した。
- `294-295-isolated-static-20261005.py`成功、変更3ソース・所有/依存・文書/waiverで0 findings。LLVM 19.1.5の`git-clang-format --diff 36a1c9c 1883e27 -- src/ui/win32/Direct2DRenderer.cpp src/ui/win32/Direct2DRenderer.hpp src/ui/win32/StatusTextLayout.hpp`差分なし。`git diff --check`成功。
- `python -B eng/protected-diff.py --base 36a1c9c --head 1883e27`成功。fixture 1853→1853、metadata/deleted/changed/added各0、protected files none。scope33は未測33。`out/protected/1883e27.json`。
- 自己レビュー: 通常6枠、録画時7枠、通知4枠、Ex入力3枠で固定上限内。UI書式再生成前に全枠破棄。借用書式は同一性比較専用。作成失敗で古い枠を出さない。実際のDPI遷移・device lost・割当失敗は未再現。

規則: ARC-001 / ARC-011 / CPP-012 / CPP-016 / QLT-001 / QLT-004 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。保存schema・基準値・ゲートは変更なし。Waivers: none。main統合前の文書追記やcommit SHA変更だけでは成功済み検証を再実行しない。
