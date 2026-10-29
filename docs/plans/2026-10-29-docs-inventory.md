# 文書の棚卸と整理の計画（案）

2026-10-29 / hide「そのうちドキュメントも現状に合わせて棚卸して整理する必要あるかもね」/ 設計席（Nibリナ）。
きっかけ: 外部の講評（別のアカウントの Claude・2026-10-29）が「`gj` `gk`・`s S R`・大文字マーク・autoindent は該当 Issue なし」と読んだが、`gj` `gk` は main に入っていて（#596 #653）、`s S R` は #768 にあった。現状が 1 か所に無く、経緯と混ざっていて、人が書く一覧がずれている。

## 0. 要点

| 問い | 設計席の決め |
|---|---|
| 何を直すか | (1) 現状の一覧を**表から生成**して手で書かない (2) 経緯を現状から分ける (3) SPECIFICATION の未実装の表を 1 回だけ手で棚卸す |
| 守らせ方 | 生成物の一致は CNF-010 と同じ検査（`eng/conformance.py`）。手書きの一覧は残さない |
| 時期 | merge の列と席が落ち着いた日。(1) は実装席 0.5〜1 日・(2)(3) は設計席の半日。着手は hide の合図 |

## 1. 生成する一覧（`docs/reference/`・`python eng/inventory.py`）

| 文書 | 正本の表 | 中身 |
|---|---|---|
| `vim-keys.md` | `src/core/VimKeyTable.cpp`（NORMAL / VISUAL / OPERATOR-PENDING / `g` `z` `Ctrl-w` の後・INSERT の `insert_special`） | モード別の鍵 → 動作の表。fixture の件数（`tests/vim/fixtures.json` の接頭辞で数える）を添える |
| `operations.md` | `src/core/OperationTexts.cpp` `OperationBindings.hpp` | F1 の操作・鍵（通常 / Vim）・説明 |
| `ex-commands.md` | `src/core/ExResult.cpp`（コマンドの表・`set` の option の表・候補） | Ex のコマンドと `:set` の option |
| `settings.md` | `src/adapters/win32/SettingsCodec.cpp`（v4 の鍵の表・接頭辞の族） | 鍵・既定・必須か後から足した鍵か |
| `ports.md` | `src/application/EditorPorts.hpp` | port と実装（adapter / ui）と替え玉 |

生成の道具は `eng/vim-oracle.py --key-names` と同じ置き方（`eng/inventory.py --write` が書き、`--check` が一致を見る・新しい CNF の規則（番号は実装のときに採る） として `eng/conformance.py` に 1 行・`docs/QUALITY_GATES.md` に 1 行）。表の読み方は C++ の正規表現ではなく、表を `constexpr` の配列のまま小さな probe（`nib_inventory` の exe・`tests/` に置く開発用 target）で JSON に吐かせて Python が整形する（表の形が変わっても追従する）。

## 2. 経緯を現状から分ける

- CLAUDE.md §6「いまの状況」の日付ごとの段落（2026-09-15〜）は `docs/history/2026-10.md`（月ごと）へ移し、§6 は「最新の日報・引き継ぎ・`docs/reference/`・`docs/todo/current.md`」へのリンク 5 行だけにする。
- `docs/todo/current.md` は「いまの列（未実装・判断待ち・hide に決めてもらうこと）」だけにし、済みの行と日付の区切りの段落は消す（Issue と日報が正本）。
- `docs/handoffs/` と `docs/reports/` はそのまま（日付の記録）。

## 3. SPECIFICATION の棚卸（1 回だけ手で）

- §5 の T2（Vim の未実装の一覧）を `vim-keys.md` と突き合わせて書き直す。済みは消し、未実装は Issue 番号を添える（今日 #1085 #1086 を足し、#768 を `R` `s` `S` に狭めた）。
- FR の表の「状態」の列を (1) の生成物へのリンクにする。

## 4. 工程

| 段 | 中身 | 席 | 前提 |
|---|---|---|---|
| D1 | `eng/inventory.py` と `nib_inventory`・5 つの生成物・新しい CNF の規則（番号は実装のときに採る） | 実装席 1（0.5〜1 日） | merge の列が空 |
| D2 | CLAUDE.md §6 と `current.md` の分離・`docs/history/` | 設計席（半日） | D1 |
| D3 | SPECIFICATION §5 / FR の棚卸 | 設計席（半日） | D1 |

## 5. 施主に確かめる点

| # | 問い | 推し |
|---|---|---|
| I-1 | 生成する一覧を `docs/reference/` に置くこと（英語名のファイル・中身は日本語） | はい |
| I-2 | CLAUDE.md §6 の経緯を `docs/history/` へ移すこと（CLAUDE.md が短くなり、席が読む量が減る） | はい |
| I-3 | `current.md` の済みの行を消すこと（Issue が正本） | はい |
| I-4 | 時期 | merge の列が空の日の朝 |
