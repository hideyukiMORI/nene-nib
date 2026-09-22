# CLAUDE.md — NeNe Nib

Claude Code / AI エージェントがこのリポジトリで作業するための**中核ハンドブック**。
簡潔な英語版の入口は [AGENTS.md](AGENTS.md)。詳細の正本は `docs/` にあり、ここには複製しない。

---

## 0. まず読むもの（production コードに触れる前に必ず）

0. [SPECIFICATION.md](SPECIFICATION.md) — 何を作るか（FR-NNN・施主決定 D1〜D10）
1. [docs/ARCHITECTURE_CONSTITUTION.md](docs/ARCHITECTURE_CONSTITUTION.md) — 憲章（ARC-NNN）
2. [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md) — モジュールと依存方向
3. [docs/CODING_RULES.md](docs/CODING_RULES.md) — C++23 (clang-cl) 規約（CPP-NNN）
4. [docs/QUALITY_GATES.md](docs/QUALITY_GATES.md) — **いま何が機械で守られているか**（QLT-NNN / CNF-NNN）
5. [docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) — 手順
6. [docs/COMMIT_CONVENTIONS.md](docs/COMMIT_CONVENTIONS.md) — Issue・ブランチ・コミット・PR（GIT-NNN）
7. [docs/GLOSSARY.md](docs/GLOSSARY.md) — 用語
8. 該当する ADR（`docs/adr/`）と有効な waiver（`docs/waivers/`）

---

## 1. このリポジトリの統治原則

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

その帰結として、次の 3 つを常に守る。

1. **正典の経路を先に特定してから編集する。** 「ここで書いたほうが早いから」で第 2 の経路を作らない（ARC-001 / ARC-012）
2. **ゲートを弱めて通さない。** 検査が落ちたらコードを直す。閾値・除外・重大度を触るのは ADR 相当の判断（QLT-010）
3. **`planned` を `active` と書かない。** 未実装の強制を実装済みに見せるのは、この規約体系で唯一「壊す」行為（[ADR 0001](docs/adr/0001-strictness-is-mechanically-enforced.md)）

---

## 2. このプロジェクトで間違えやすい所

### コンパイラは clang-cl だけ

`cl` は Phase 0 の比較対象としてだけ残っている（[ADR 0003](docs/adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)）。`eng/toolchain.ps1` が `CXX=clang-cl` を固定する。
製品も測定ビルドも将来の md4c も同じ 1 本。警告集合は `eng/targets.cmake` の 1 か所で、`eng/probes/language.json` の `clangStrict` と同じ並び。

### 現在時刻・ファイル・スレッドを持てる場所は 1 つしかない

現在時刻・乱数・既定ロケール・環境変数・ファイル・**スレッド**を持ってよいのは **`src/adapters/win32`** だけである（ARC-007 / ARC-003 / CPP-013）。
中核で必要なら、型のあるポートから注入する。**テストが実時刻を読むことも決定性の破壊である。**
検査はソースの名前ではなく **リンカのシンボル**で行う（`eng/symbols.py`）。`system_clock::now()` は `_Xtime_get_ticks`、`std::thread` は `_beginthreadex` として現れる。
`std::atomic` はリンカに見えないので、`<atomic>` の include を字句検査（CNF-009）が拒否する。

### 網羅性検査を殺す分岐を書かない

閉じた選択肢の分岐に `default` / `else` / `_` を書かない。選択肢が増えたらコンパイルが落ちるのが正しい状態（CPP-002）。
`DefWindowProcW` へ渡す OS メッセージの既定分岐だけは例外（CPP-017）。

### 期待される失敗は例外にしない

検証エラー・見つからない・拒否・非互換・device lost は `std::expected` か閉じた `enum class` で返し、`[[nodiscard]]` を付ける（ARC-010 / CPP-005）。

### `std::optional` は `value()` で読む

MSVC STL の `optional` は `operator*` を clang-tidy が見ない（Phase 0 の T3-tidy-unchecked-optional-star-hole）。`value()` / `value_or()` で読み、`*` と `->` を書かない（CPP-004）。

### COM は `-Wno-language-extension-token` の上で書く

`__uuidof` / `IID_PPV_ARGS` は `-Wpedantic` で落ちる（W1）。名指しで外してあるので、警告集合を自分で触らない。COM の所有は `ComPtr`、device lost は結果型（CPP-017）。

### SIMD は `src/core/simd/` にだけ書く

関数ごとに `[[gnu::target("…")]]` を付け、fallback を必ず置く。target 単位の `/arch` は付けない（CPP-018・[ADR 0006](docs/adr/0006-speed-gate-simd-and-table-driven-dispatch.md)）。

### 大きな分岐は表で書く

60 分岐の `switch` は関数長 60 行で落ち、`constexpr` の表は通る（T8）。Vim のキー列 → 動作は表（CPP-012）。

### 公開 aggregate に `= default` の `operator==` を書かない

`RgbColor` / `Palette` のような公開 aggregate はメソッドを 1 つでも持つと lint が落ちる（Issue #3）。比較は非メンバーで書く（CPP-003）。

### `reinterpret_cast` は書けない。`std::bit_cast` と `ComPtr<IUnknown>` ＋ `As()`

clang-tidy が一律に拒否する。HWND ↔ `this` と `LPARAM` の読み替えは `std::bit_cast`、`IUnknown**` を要求する API は `ComPtr<IUnknown>` で受けて `As()`（CPP-009）。

### `.cpp` の無名名前空間の `struct` も 1 ファイル 1 型に数える

ローカルの補助は自由関数と `using` 別名で書く（CPP-011）。

### ui/win32 に色のリテラルを書かない

色は `core::Palette` のトークンだけ（テーマ拡張の前提・ADR 0008 決定 8）。足りない色はトークンを 1 つ足して採用案の表に書く。

### Mica には `WS_EX_NOREDIRECTIONBITMAP` と `DWMWA_USE_IMMERSIVE_DARK_MODE` が要る

無いとタイトルバーに透けない・ダークでも明るいままになる（Issue #5 で実測。ADR 0008 決定 9）。

### core で `std::lround` を呼ばない

libm のシンボルが core の外へ出て ARC-003 が落ちる。DIP → 物理画素は整数演算（ADR 0008 決定 5）。

### `std::string_view::find` などの STL の検索は許可シンボルに載っている

MSVC STL は `__std_find_trivial_*` 等の純関数を core の外へ出す（Issue #7 で実測）。`eng/symbol-allowlist.json` に足してある。他の `__std_*` が出たら「時刻・OS・スレッドに触れないか」を確かめてから足す。

### 意図は `std::variant` の閉じた和型

`EditorIntent` は `std::visit` で写し、選択肢が増えたらコンパイルが落ちる（ADR 0009）。方向や操作の種類は閉じた `enum` を持つ型に畳む。

### md4c はまだ入っていない

Markdown プレビュー（FR-007）の Issue で、別 target に `/W4 /WX` だけを当てて tag と SHA-256 で固定して入れる（ADR 0003）。厳格集合を当てると 20 件超で落ちる。

---

## 3. 検証コマンド

差分の挙動・直接の依存先と呼び出し元から、起こり得る退行を検出する最小限の検証を選ぶ。
何が壊れる可能性を確認するか説明できない検証は実行しない。既存の CMake target・CTest `-R`・unittest の対象指定を使う。
文書・コメント・規約変更にはアプリの動作テストは不要。フック・開発ツールは変更した道具だけを短く確認する。

実装・テスト・関連依存・必要な環境条件が不変なら成功結果を push / レビュー / merge で再利用する。
担当・工程・文書追記・SHA の変更だけでは再実行しない。関連する変更・失敗・具体的な未確認事項だけを再検証する。
全件は限定した検証では覆えない具体的な理由がある場合だけ、対象と理由を短く知らせて明示実行する。

```powershell
pwsh -NoProfile -File ./eng/check.ps1 -Full -Reason '限定した検証では影響を確認できない具体的な理由'
```

実行していない結果を書かない。本件が原因の失敗は直し、無関係な既存失敗は根拠とともに別 Issue へ記録して本件を続ける。
成功するまでの再試行や無関係な修正・全件再実行は禁止。PR には対象・退行の根拠・結果と所在・再利用の根拠を記録する。
正本は QLT-001 / QLT-012 と [ADR 0021](docs/adr/0021-diff-scoped-verification-and-result-reuse.md)。過去の全件・最終 HEAD ごとの実行指示より優先する。

---

## 4. 変更の進め方

[docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) が正本。要約すると:

Issue → 正典経路の特定 → ブランチ → （設計を変えるなら先に ADR）→ 最小の実装 →
必要な検証（成功結果を再利用）→ 規則 ID ごとの自己レビュー → PR（draft）→ 検証記録を整えて Ready → 必須 check → squash merge。

コミットは Conventional Commits（`type` と `scope` は英語、説明は日本語、末尾に `(#N)`）。形の正本は GIT-003。

---

## 5. 完了報告の形

作業を終えたら必ず次を報告する。

```text
Issue / 規則 ID:
変更したファイルと振る舞い:
実行した検証コマンドと結果:
ドキュメント・スキーマの変更:
Waivers: none | WVR-NNNN
残るリスク:
```

調査だけを頼まれたときは、編集・コミット・push・PR 作成・外部状態の変更を行わない。

---

## 6. いまの状況

現在のタスクは [docs/todo/current.md](docs/todo/current.md)。GitHub Issue が正で、そこは要約。

2026-09-15: Issue #1（Phase 0〜2）、#3（最初の縦切り・ADR 0007）、#5（見た目・ADR 0008）、#7（編集・ADR 0009）、#11（ファイル・ADR 0010）。2026-09-16: #13（UTF-16 の変換を core に 1 本化）、#16（速さ・ADR 0011）、#19（起動の内訳）、#22（Vim の最初の縦切り・ADR 0012）。2026-09-17: #24（窓を先に見せる起動・ADR 0013）、#28（IME・ADR 0014）。2026-09-18: #43（Vim の 2 本目・ADR 0015）、#44（生成物の SHA・CNF-010）、#47（CI の速さの基準値・ADR 0016）。2026-09-19: #52（カラーテーマ C1・ADR 0017）、#53（VISUAL・ADR 0018）。2026-09-20〜21: #58 画面移動（ADR 0019）、#60〜#70 設定と C2〜C4b（ADR 0020〜0025）、#72 文字検索（ADR 0026）、#76 gg/G（ADR 0027）、#79 o/O（ADR 0028）、#77 / #81、#84 r（ADR 0029）。2026-09-22: #87 `.`（ADR 0030）、#88 検証方針の文言、#93 テキストオブジェクト（ADR 0031）、#94 CI の検査文言の UTF-8、#100 検索（ADR 0032）、#98 fixtures.json の整形（CNF-011）、#91 VISUAL の `.`（ADR 0033）、#92 割り込みの 1 本化、#99 テキストオブジェクトの残差、#108 仮想桁（ADR 0034）。起動すると枠なし窓（Snap と影は OS のまま）に Mica のタイトルバー、タブ 1 本と窓の操作、
piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）、ステータスバーの「通常 | Vim」トグルを Direct2D で描き、OS のライト／ダーク（茄子色 D11・橙 D12）に従う。
Ctrl+O / Ctrl+S / Ctrl+Shift+S と起動引数でファイルを開いて保存し、UTF-8 / UTF-8 BOM / Shift_JIS と CRLF / LF を読んだ形のまま保ち、未保存の印「● 」と「保存しますか」を出す。
速さは `eng/measure-speed.py` が Release の exe で 5 本のベンチを測り、`eng/perf-reference.json` の機械ごとの基準値と比べてゲートで落とす（QLT-014・測るのは差分が速さに関わるときと `-Full` の明示実行で、CI の必須 check は測らない・施主の実機と CI の EPYC 7763 の指紋で active・他の CI host は記録だけと 1 行言って通る・ADR 0016。実機: 起動 191 ms・窓が見えるまで 35 ms・1 打鍵 0.9 ms・16 MiB 250 ms）。描画は `WM_PAINT` で 1 フレームに 1 回。
見た目の正本は `docs/design/2026-09-15-look.md` と `docs/design/2026-09-15-editing-look.md`。カラーテーマとフォントサイズの計画は `docs/plans/2026-09-15-colorschemes.md`（D13 / D14）。
Vim は NORMAL / INSERT（`h j k l 0 $ w b e ^ gg G`・`f/F/t/T`と`;`/`,`・Home / End・回数はオペレータ側と移動側の掛け算・`x`・`r`（回数/選択範囲の文字置換）・`d c y` ＋移動・`dd cc yy`・`D C Y`・`p P`・種類つきの無名レジスタ（本文は LF）・`i a I A`・`o O`（回数付き入力・`i a I A` の回数も反復）・`.`（直前の変更を鍵の列として記録し同じ経路へ再生・`N.` は回数を置き換えて以後も引き継ぐ・VISUAL の変更は範囲の大きさを記録して選び直してから再生し回数は使わない・ADR 0033）・テキストオブジェクト `iw aw iW aW i" a" i' a' i` a` i( a( i{ a{ i[ a[ i< a<`（`b` `B` と閉じ括弧の鍵も同じ表・d/c/y と VISUAL で同じ範囲関数 1 本・VISUAL は常に文字単位で `viwiw` `vi(i(` は 1 つぶん広げる・取消でも Vim と同じにキャレットと選択が動く・`it` `ip` `is` は後続）・検索 `/ ? n N * #`（Ex と同じ入力行から入り確定は 1 つの鍵・`magic` の部分集合の照合器で未対応構文は閉じた失敗・E486 / E35 / E348 と折り返しの報せ・ADR 0032）・`j k H M L` の欲しい列と VISUAL の `.` の桁は仮想桁（Tab は 8・全角は 2・表は固定 Vim の実測値・ADR 0034）・Esc・`u` / Ctrl-r・INSERT 1 回が undo 1 単位・VISUAL `v V`・`H M L`・Ctrl-d/u/f/b・PgUp/PgDn）で、再現度は本物の Vim 9.1 の oracle が生成した fixture 1137 件（`tests/vim/`・`eng/vim-oracle.py --regenerate` が正準形で書き戻す・生成物と json の一致は CNF-010・json の整形は CNF-011）を CTest が再生して守る。IME は IMM32 を ui/win32 が受け、変換中の文字列は本文の外（`core::Composition`）に持って renderer がキャレットの行に差し込んで描き、確定は 1 意図（通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ）。Vim NORMAL では IME を切り INSERT で戻す（ADR 0014）。coreに組み込み9テーマ（ADR 0017）があり、版付き設定の保存/復元（C2・ADR 0020）、NORMALの `:` から `colorscheme` / `set fontsize=` / `set guifont=` の変更と補完（C3a・ADR 0022）を接続済み。通常/Vim全モードからCtrl+Pの設定一覧（C3b・ADR 0023）も利用でき、候補・入力session・評価・保存はExと共用する。`Ctrl-v`（矩形・#112・ADR 0035 提案・進行中）・複数タブ・Ctrl+Pのファイル/履歴統合・一般Exは後続。起動は窓が約 35 ms で見え、本文は `D3D11CreateDevice`（NVIDIA のドライバ初期化 160 ms・呼び方では縮まない）の後に約 200 ms で出る（ADR 0013）。最新は [日報](docs/reports/2026-09-22.md) / [引き継ぎ](docs/handoffs/2026-09-22.md)。

---

## 7. 上位のポリシーと雛形の所在

このリポジトリの規約は **AYANE 厳格規約ポリシー**の 3 回目の適用例である（1 回目 nene-loupe・C++23 / MSVC、2 回目 nene-folio・C23 / clang-cl）。
ポリシー本文・初期化手順・雛形は施主の private ワークスペースにあり、このリポジトリには置かない。
雛形で足りなかったものを見つけたら、このリポを直すと同時に施主へ還流点として報告する。
