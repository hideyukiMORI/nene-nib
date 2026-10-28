# 設定モーダルと「速さへの影響」の可視化の計画（案）

2026-10-28 / hide の指示（外から・「この辺りの設定は設定モーダルでオン、オフ状態一覧で、速度を犠牲にする度合いを可視化して初心者でも速度の状態管理が簡単に出来るようにしたい。どういう機能でどのくらい速度犠牲があるかを明示する」）/ 設計席（Nibリナ）。
前提: 設定モーダルは #438 / #454 で案（`docs/design/settings/settings-proposal.html`・ADR 0123 は表示言語の持ち方・未受理）。この計画は「機能のオン・オフの一覧」と「速さへの影響の明示」を足し、モーダル全体の採用（D12 の見た目・日英の ADR 0123）は別に判断する。

## 0. 要点

| 問い | 設計席の決め |
|---|---|
| 何を見せるか | 機能ごとに **オン・オフのスイッチ**と **「速さへの影響」の段（なし / 小 / 中 / 大）** と **1 行の説明**（何が速くなくなるか・どの操作のとき）。段の根拠は機械で測った数字（任意のベンチの差分）で、表に書いて PR で更新する。推測の段は出さない |
| 段の決め方 | 1 打鍵（空の文書）の増分: なし = +1% 未満（5 µs）・小 = +1〜5%・中 = +5〜20%・大 = +20% 超か「打鍵の道の外で裏の仕事が走る（CPU・メモリ・子プロセス）」。測り方は D67 の §0 の A/B |
| 要約 | 一覧の上に「いまの設定での 1 打鍵の見込み: 0.64 ms（基準）＋ 小 × 2 ＋ 中 × 1」と、オンの機能のうち段が中以上のものを名指しで 1 行。数字は基準値（`eng/perf-reference.json` の `key-to-frame-single`）と表の増分の和（実測ではなく見込み・そう書く） |
| 初心者向けの 3 つの組 | 「速さ優先」（段がなしの機能だけオン）・「おすすめ」（小までオン・既定）・「全部オン」。組を選ぶとスイッチが一括で変わり、個別に直せる |
| どこで切り替えるか | モーダルの一覧・`:set <名前>` / `:set no<名前>`・Ctrl+P の `:` の候補。全部同じ設定の欄（v4）に書く |
| 工程 | 設定 v4 の欄の定義（LSP / エクスプローラー / 機能のオン・オフ）→ 機能の表（core）→ モーダル（ui）→ 要約と組。3〜4 席日（モーダルの土台 #454 の採用を含めると +2〜3） |

## 1. 形

- **core の表 `feature_costs`（1 つ・`constexpr`）**: `FeatureCost{EditorFeature feature; CostTier tier; std::uint16_t single_key_delta_us; std::string_view bench; DisplayText description;}` の列。`EditorFeature` は閉じた enum（`matchparen` `commentary` `surround` `autopairs` `completion` `change_marks` `indent_guides` `yank_flash` `trailing_space` `todo_marks` `color_swatches` `explorer` `easymotion` `lsp` `syntax`（既存の `:syntax`）`markdown_preview`（既存）`wrap`（既存）…）。`tier` は測った数字から機械で決める（`tier_of(delta_us, base_us)` の純関数・境は §0）。表の行は機能の PR が足し、`bench` の名前が `eng/measure-speed.py` の `OPTIONAL_BENCHES` に在ることを conformance が照合する（CNF-015・planned）。
- **設定 v4**: `EditorSettings` に `features`（`EditorFeature` → on / off の閉じた表・既定は表の `default_on`）を足す。LSP（`lsp`・ADR 0160）・エクスプローラー（`explorer` `root`・ADR 0162）・`autopairs` `indentguides` `stripws` も同じ欄に畳む（名前の 1 本化）。`:set <名前>` / `:set no<名前>` / `:set <名前>?` は表から作る（1 本・名前の別表を書かない・ADR 0078 の形）。
- **application**: 要約の純関数 `speed_summary(settings, base_us) -> SpeedSummary{estimated_us, heavy_features}`（見込み・基準値の数字は合成ルートが `perf-reference.json` と同じ値を core の定数に写す・機械ごとの基準値は読まない（ARC-003・設定の表示は近似でよいと明示））。
- **ui（モーダル）**: #454 の案の「設定」の面に「機能」の節を足す。1 行 = スイッチ・名前・段の印（なし = 何も無し・小 = `muted` の ●・中 = `accent` の ●●・大 = `error` の ●●●）・説明。上に要約の 1 行と組の 3 つのボタン。Tab で移り Space で切り替え・Esc で閉じる（変更は閉じるときに保存・取消は Esc）。日英は ADR 0123 の判断に従う（初版は日本語）。
- **根拠の文書**: `docs/quality/feature-costs.md`（表の写し・測った日・機械・ベンチの名前・数字）。機能の PR が 1 行足す。

## 2. 施主に確かめる点

| # | 問い | 設計席の推し |
|---|---|---|
| G-1 | 段の境（§0）と要約の形 | この形で。数字は「見込み」と書く |
| G-2 | 3 つの組（速さ優先 / おすすめ / 全部オン） | 入れる。既定は「おすすめ」 |
| G-3 | モーダルの土台（#454 の案・D12 の見た目）を今の計画と一緒に採用するか | 一緒に。見た目は /design の案を 1 つ出して hide が選ぶ（既存の `settings-proposal.html` を基に「機能」の節を足した版） |
| G-4 | 表示言語（ADR 0123） | 初版は日本語だけ。英語は ADR 0123 の受理の後 |
| G-5 | 順 | 機能が 3〜4 本入った後（matchparen・commentary・surround・エクスプローラー）に設定の欄をまとめて v4 に。それより前に v4 が要る PR（LSP L0c・エクスプローラー E2）は、この計画の欄の名前で先に v4 を定義する |

## 3. 工程

| 段 | 中身 | 席日 | ADR | 前提 |
|---|---|---|---|---|
| S1 | ADR（この計画の §1）・/design の案 | 設計席 0.5 | **1** | G-1〜G-5 |
| S2 | core: `EditorFeature` `FeatureCost` の表・`tier_of`・`:set` の生成・設定 v4 の欄・CNF-015 | 1 | S1 | 分割（#992） |
| S3 | ui: モーダル（#454 の案の土台 ＋ 機能の節 ＋ 要約 ＋ 組）・鍵（Ctrl+,）・F1 の一覧 | 2〜3 | S1 | S2 |
| S4 | `docs/quality/feature-costs.md` と各機能の PR の表の行（機能ごとに測る） | 機能の PR の中 | — | 機能ごと |

## 4. 速さ

モーダルは開いたときだけ描く（本文の打鍵の道に分岐を足さない）。要約の計算は表の和で O(機能の数)。
