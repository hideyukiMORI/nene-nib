# 機能の速さの費用 — 設定の面の段の根拠（ADR 0170）

設定の面（#1016）が機能の行に出す「速さへの影響」の段（なし / 小 / 中 / 大）は、この文書の表に行がある機能だけが持つ（[ADR 0170](../adr/0170-settings-modal-lists-features-with-measured-speed-cost-tiers.md) の決定 2 と 2026-11-18 の追補 3）。行の無い機能はスイッチだけで、段の印を出さない（推測の段を出さない）。

- 段は core の `tier_of(増分, 基準)` が機械で決める。基準は `reference_single_key_us = 641`（`eng/perf-reference.json` の `key-to-frame-single` の採用値 0.641 ms）。境は基準に対して +1% 未満 none・1〜5% low・5〜20% medium・20% 超 high。裏で CPU・メモリ・子プロセスを使う機能（表の `background`）は high。
- 下の表の「機能」「ベンチ」「1 打鍵の増分 µs」の 3 列は `src/core/FeatureCosts.hpp` の `feature_costs` の `name` `bench` `single_key_delta_us` と同じでなければならない。照合は `eng/conformance.py` の CNF-015（ベンチが `eng/measure-speed.py` の `OPTIONAL_BENCHES` に在ることも見る）。数字を変えるときは表と同じ PR で変える。
- 数字は既存の記録から写したもので、この文書のために測り直していない。どれも「採用値に対する見込み」で、使う人の機械の実測ではない。

| 機能 | ベンチ | 1 打鍵の増分 µs | 測った日 | 機械 | 元の数字 | 出典 |
|---|---|---|---|---|---|---|
| matchparen | matchparen-single-key | 17 | 2026-11-04 | 設計席の機械（CPU 20 論理コア・負荷 8%・Release） | 15 run × ABBA の本測定で `matchparen-single-key` の中央値が組 1 で 0.578 → 0.595 ms（+17 µs）、組 2 で 0.600 → 0.612 ms（+12 µs）。大きい方の +17 µs を写した（どちらでも段は low） | [quality-day-2026-11-04.md の §7.1](quality-day-2026-11-04.md) |
| lsp | lsp-single-key | 200 | 2026-11-03 | 設計席の機械（Release・替え玉 `nib_fake_language_server`・各 1 回 `--record`・基準値にしない初回の参考値） | `lsp-single-key` 1.307 ms と `key-to-frame-single-cpp` 1.110 ms の差 +0.20 ms（drawn の +0.18 は波線と右端の件数）。子プロセスを使うので段は high（background） | [quality-day-2026-11-04.md の §1（11-03 の夜のゲート）](quality-day-2026-11-04.md) |

## 行の無い機能（段の印を出さない）

| 機能 | 理由 |
|---|---|
| minimap | オフの A/B（2026-11-11・ADR 0171）は差が計測の分解能の外。オンの費用（`minimap-single-key` `minimap-burst-200`）は基準値が無く、試用の後に採る（ADR 0171 の「強制」） |
| wrap・guide・autoindent・explorer・easymotion・start_screen | 1 打鍵の増分を測った記録が無い |
| syntax | 文書ごとの値で設定の面に載せない（決定 4） |
| caretmotion | オフの道の増分は設定の欄 1 つを読むだけ。動いている間だけ frame を出すので、オンの費用は打鍵の基準に混ぜず別の基準で扱う（ADR 0192 の決定 7・D60 の考え方）。試用の後に採る |
| caretsparks | オフの道の増分は打鍵の道の真偽 1 つの読みと、描く側の設定の欄 1 つを読むだけ。粒が生きている間（1 打鍵から 420 ms）だけ frame を出すので、オンの費用は打鍵の基準に混ぜず別の基準で扱う（ADR 0192 の追補・#1979）。試用の後に採る |
| indentguides | オフの道は `features=` の真偽を 1 つ読むだけで欄を作らない（ADR 0193 の決定 6）。オンの費用は任意のベンチ `indentguides-single-key` で設計席が測ってから、この表に行を足す（測るまで段の印なし・ADR 0170 の追補 3） |
| list | オフの道は `features=` の真偽を 1 つ読むだけで欄を作らない（ADR 0193 の決定 6）。オンの費用は任意のベンチ `list-single-key` で設計席が測ってから、この表に行を足す（測るまで段の印なし・ADR 0170 の追補 3） |

行を足すときは、任意のベンチ（`OPTIONAL_BENCHES`）で測った A/B の記録（日・機械・数字）を品質の記録に残し、その行をここに写して `feature_costs` の同じ行の `single_key_delta_us` と `bench` を埋める。
