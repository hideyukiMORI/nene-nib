# ADR 0170 — 設定モーダルは機能のオン・オフを一覧にし、速さへの影響の段は測った数字から機械で決める

- 状態: 受理（施主決定 D68・FR-028・2026-10-28「ほぼ推し」。実装は S2〜S3 の席（言語の仕組み・ADR 0123 の改訂の後）・見た目は /design の案を hide が見る・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #1016・計画 `docs/plans/2026-10-28-settings-modal-speed-costs.md`・前段 #438 / #454（設定画面の案 `docs/design/settings/settings-proposal.html`）
- 規則: FR-028、ARC-001 / ARC-003 / ARC-004 / ARC-009 / ARC-011、CPP-002 / CPP-005 / CPP-011 / CPP-012、QLT-010 / QLT-014、ADR 0020 / 0079 / 0114（設定の版と移行）/ 0078（操作の表）/ 0123（表示言語・改訂）/ 0138（任意のベンチ）/ 0150（面の行の layout の保持）

## 文脈

D67 で機能が 20 本近く増える。hide の要望は「どの機能がどれだけ速さを犠牲にするかを一覧で見せ、初心者でも速さの状態を管理できる」こと。Nib には設定の版の移行（v3）・操作の表・任意のベンチの仕組みがあり、設定画面は案だけ（#454）。

## 決定

1. **機能の表は core の 1 つ。** `EditorFeature`（閉じた enum・`matchparen` `commentary` `surround` `autopairs` `completion` `change_marks` `indent_guides` `yank_flash` `trailing_space` `strip_whitespace` `todo_marks` `color_swatches` `explorer` `easymotion` `minimap` `start_screen` `lsp` `syntax` `markdown_preview` `wrap` `guide`・既存の `:set` の値もここに畳む）と `feature_costs`（`constexpr` の表・`EditorFeature` の宣言の順・行が欠けたらコンパイルが落ちる）: `FeatureCost{EditorFeature feature; bool default_on; std::uint16_t single_key_delta_us; std::string_view bench; UiMessage name; UiMessage description;}`。文は `ui_texts`（ADR 0123 の改訂）から引く。
2. **段は機械で決める。** `CostTier tier_of(std::uint16_t delta_us, std::uint16_t base_us) -> {none, low, medium, high}`（純関数・境は基準の 1 打鍵に対して +1% 未満 / 1〜5% / 5〜20% / 20% 超）。裏で CPU・メモリ・子プロセスを使う機能（`lsp` `minimap` の bitmap・全文検索）は表の `background = true` で段を `high` に上げる。`single_key_delta_us` は機能の PR が任意のベンチ（`bench` の名前・`eng/measure-speed.py` の `OPTIONAL_BENCHES` に在ること・CNF-015）で測った値で、`docs/quality/feature-costs.md` に測った日・機械・数字を書く。表に無い機能はスイッチに載らない（網羅性）。推測の段は出さない。
3. **設定 v4 の鍵の集合（1 回で決める・ADR 0123 の改訂の決定 7）。** `version=4`・既存 5 鍵（`colorscheme` `font_family` `font_size` `guide` `wrap`）・`ui_language`（ADR 0123）・`lsp`（ADR 0160）・`explorer` `root`（ADR 0162）・`features=<名前,名前,…>`（オンの機能の名前の列・ASCII・表の順・既定は表の `default_on`）・`lsp.<language>.command`（ADR 0160・0 行以上）。`guide` `wrap` は互換のため鍵のまま（`features` には入れない・読む側で同じ `EditorFeature` に写す）。v4 不在時は v3 を読み、既定を補う。最初に v4 が要る PR（LSP L0c か エクスプローラー E2 か 言語 T1 の早い方）がこの鍵の集合で v4 を定義し、後の PR は欄を使う。
4. **`:set` は表から作る。** `:set <名前>` / `:set no<名前>` / `:set <名前>?` を `EditorFeature` の名前（表の `name` の ASCII・`feature_name_of`）から 1 本で生成し、Ctrl+P の `:` の候補も同じ表から（名前の別表を書かない・ADR 0078 の形）。既存の `wrap` `guide` `syntax` `hlsearch` `incsearch` の `:set` はそのまま（`syntax` と `hlsearch` `incsearch` は文書 / Vim の値で設定ではないので `features` に入れない・表には `syntax` だけ「設定に入れない」の印で載せる）。
5. **モーダル。** 操作 `open_settings`（名前「設定」・鍵 Ctrl+,・F1 の一覧）で開く。#454 の案の土台（字体・大きさ・配色・案内・折り返し・言語）の下に「機能」の節: 上に要約の 1 行（「いまの設定での 1 打鍵の見込み: 0.64 ms ＋ 小 × 2 ＋ 中 × 1」・基準値は core の定数 `reference_single_key_us`（`eng/perf-reference.json` の `key-to-frame-single` の採用値を写す・機械ごとの実測は読まない・「見込み」と書く）と、中以上でオンの機能の名前）・3 つの組のボタン（速さ優先 = `none` だけオン / おすすめ = `low` までオン（既定）/ 全部オン）・機能の行（スイッチ・名前・段の印（none = 無し・low = `muted` ●・medium = `accent` ●●・high = `error` ●●●）・説明）。Tab / 矢印で移り Space で切り替え・Esc で閉じて保存（取消は `Ctrl+Z` 相当の「元に戻す」ボタン）。IME は閉じる。面の行の描画は `PaletteRowLayouts` の保持。色のトークンは足さない。
6. **保存。** 閉じるときに 1 回 `persist_settings`（v4）。失敗は 1 行の知らせ（ADR 0114 の規則）。切り替えは即時に反映（frame を作り直す・`lsp` はサーバーの起動 / 停止・`explorer` は面の出し入れ）。
7. **契約。** core: `--feature-costs`（表の網羅性・`tier_of` の境・`features` の読み書き・名前の往復・`:set` の生成・v4 の移行 v3 → v4 と既定）・CNF-015（`bench` の名前が `OPTIONAL_BENCHES` に在る・conformance）・`docs/quality/feature-costs.md` の行と表の一致（conformance・数字の照合）。application: `--settings-modal`（組の 3 つ・要約・切り替えの即時反映・保存 1 回・失敗）。ui: `nib_window_tests` の画素と `eng/verify-window.py --settings`。
8. **費用。** モーダルは開いているときだけ描く。要約は表の和 O(機能の数)。本文の打鍵の道に分岐を足さない。
9. **限界（後続）。** 実測の数字をその機械で測って見せる（今は採用値の見込み）・組の自作・機能ごとの詳細の設定（autopairs の対・minimap の幅）・英語以外の文（ADR 0123 の T3）。

## 強制

- 表の網羅性と `bench` の存在（CNF-015）: **planned**（S2 で active）。
- `docs/quality/feature-costs.md` と表の一致: **planned**（S2 で active）。
- 8 本の速さ: **active**（QLT-014）。
- 設定の版の移行の規則（ADR 0114）: **active**。

## 結果

得られるもの: 機能のオン・オフと速さの影響が一画面で分かり、組で一括に切り替えられる。`:set` と面が同じ表から出るので名前がずれない。
失うもの・残る穴: 段は「採用値に対する見込み」で、その機械の実測ではない（明示する）。段の境は固定（設定にしない）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 段を設計席が手で書く | 推測の段は hide の要望に反する。機械で測った数字から決める |
| 起動のたびにその機械で測る | 起動が遅れ、揺れで段が変わる。採用値の見込みで足りる |
| 機能ごとに別の設定ファイル | ADR 0114 の 1 ファイル 1 版の形を崩す |
