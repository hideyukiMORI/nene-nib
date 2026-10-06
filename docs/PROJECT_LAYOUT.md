# モジュール構成と依存規則 — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版
> パッケージルート（名前空間）: `nenenib`

モジュールグラフはアーキテクチャの一部である。**パッケージの命名規約だけでは依存の境界にならない。**
承認された一覧の機械可読な正本は `eng/architecture.json` で、規約検査がそれと実際のビルドグラフを突き合わせる（ARC-002）。

---

## 1. 承認されたモジュール

置換後に同じモジュールになる区画（時刻と永続化のアダプタ）は 1 つに畳んである。Issue #3（ADR 0007）で 5 層すべてに実ターゲットができた。空の将来用モジュールは作らない（ADR 0003）。

```text
src/app（wWinMain）
    合成ルート。ポートに実装を結び、SIMD の実装を選び、ワーカーを起動し、UI を起動する。端末出力と終了コードを持つ唯一の場所

src/ui/win32
    画面。枠なし窓・自前のタイトルバーとタブ・Direct2D / DirectWrite の描画・DXGI の提示・IME。状態を描き、意図を発行する

src/adapters/win32
    ファイル（メモリマップ・原子的な書き戻し）・設定・履歴・ブックマークの保存・ジャンプリスト・時刻ポート・固定ワーカー 1 本の実装。
    現在時刻・乱数・環境を読め、ファイルに触れ、スレッドを作れる唯一のモジュール

src/application
    ポートの宣言、状態の所有者（EditorState / VimState）、意図を受ける reducer、表示値の生成、結果型

src/core
    値・不変条件・閉じた選択肢。piece table・Vim エンジン・キーマップの表・Ctrl+P の順位付け・文字コード判別・行索引。
    配色トークン（UI の Palette・本文の SyntaxPalette・Theme と組み込み 9 テーマの表・整数の derive_ui・ADR 0017）と、
    タイトルバー／ステータスバーの配置とヒットテストの純関数（DIP と物理画素の幾何・ADR 0008）。
    標準ライブラリ以外に依存しない純関数層。src/core/simd だけが SIMD の組み込み関数を書ける（CPP-018）
```

将来の取り込み依存（ソースを取り込む C のライブラリ）は `third_party/<name>` を独立したモジュールにし、そこにだけ緩い警告集合を当てる（ADR 0003）。
現在は無い。

---

## 2. 許可された依存グラフ

```text
src/core
    -> 標準ライブラリのみ（許可シンボルは eng/symbol-allowlist.json）

src/application
    -> src/core

src/adapters/win32
    -> src/application, src/core

src/ui/win32
    -> src/application, src/core

src/app
    -> すべて（明示的な合成のためだけに）
```

ここに無い依存はすべて禁止である。とくに:

- core / application -> Win32 / DirectX / COM / `<filesystem>` / `<thread>`: **禁止**（`#include` は言語では塞げないので、`eng/symbols.py` のシンボル検査と字句検査で塞ぐ）
- core -> application: 禁止
- application -> アダプタ: 禁止
- ui -> アダプタ: 禁止
- アダプタ -> 別のアダプタ: 禁止
- 何か -> 合成ルート: 禁止

OS ライブラリは `nenenib_system_link` でだけ結び、`eng/architecture.json` の `platformLibraries` に無ければ configure が落ちる:
`adapters_win32` は kernel32 / shell32 / advapi32 / ole32、`ui_win32` は user32 / d2d1 / dwrite / dxgi / d3d11 / dcomp / dwmapi / imm32、`app` は user32。
gdi32 はどこにも無い（ADR 0002）。

---

## 3. 各モジュールの責務

### `src/core`

意味の正本を持つ。値型・閉じた選択肢・拒否理由と結果型。piece table・Vim エンジン（バッファ・カーソル・レジスタ・モード・オペレータ）・
キー列 → 動作の表・Exの解析/入力編集/補完・Ctrl+P の順位付け・文字コード判別・行索引の**純関数**。UI 状態・ファイル・現在時刻・スレッドを持たない。
実行時依存の許可表は空で、外部シンボルは `eng/symbol-allowlist.json` の固定STL（`operator new` / `delete`・例外・`memcpy` 系・純粋な範囲走査・`from_chars` の不変数値表）だけ（ARC-003・ADR 0022）。
ファイル由来のテーマ名・出典・色は `ThemeDocument` が所有し、既存 `Theme` のviewへ渡す（ADR 0024）。名前の生成は `ThemeName` の一経路。

### `src/core/simd`

SIMD の組み込み関数を書ける唯一の場所。関数ごとに `[[gnu::target]]` を宣言し、同じ処理の fallback を持つ（CPP-018・ADR 0006）。

### `src/application`

振る舞いの調整を持つ。ポートの宣言・状態の所有者（`EditorState` / `VimState`）・意図を受ける reducer・表示値の生成・結果型。
`EditorState` は本文と独立した任意の `CommandInput`（`CommandLine` / `CommandPalette` の和型）と結果メッセージも所有する。ExとCtrl+Pは入力編集・コマンド候補・`evaluate_ex` を共有し、相対フォント変更もcontrollerの `persist_settings` を共用する（ADR 0022 / 0023）。

Ex の `:e` / `:b` / `:ls` は core の `ExPaletteRequest`（出どころと検索文字）を controller の `run_palette_request` → `open_palette` に写す。記号への変換は `palette_input_for` が `palette_marks` を逆引きし、候補生成・照合・確定は Ctrl+P と同じ経路を使う（ADR 0064）。

Ex の保存・終了は core の `ExDocumentName` の表から `ExDocumentVerb` を返し、controller の `run_document_request` が現在の文書の条件を確認する。GUIとExの保存は `save_document` の一つで、成功したときだけ保存位置を更新する。終了は既存の `CloseTab` を通り、最後のタブの `EditorFrame.closing` はuiの `deliver` の末尾から `finish_tab_action` → `leave` へ一回だけ渡す。通常の閉じる操作と `:tabclose` は従来の確認を保つ（ADR 0066）。

通常モードの結合文字は core の `ordinary_character_boundary` が移動・着地・方向ごとの削除境界を返す。`CaretMoveRequest` の単位で通常と Vim INSERT の code point 移動を区別する。controller は結果を既存の選択と `replace` に渡し、本文・undo の道を増やさない（ADR 0065）。
ワーカーへの要求値と完了の受け取り（版番号）もここ（ADR 0004）。Win32・永続化・ファイル・スレッドを知らない。

### `src/adapters/win32`

時刻ポート・永続化ポート・ワーカーポートの実装を持つ。🔑 **決定性の禁止とスレッドの禁止を適用しない唯一のモジュール**であり（ARC-007 / CPP-013）、
その差分は `eng/symbols.py` と `eng/conformance.py` の区画判定に明示されている。保存形式の版と移行（ARC-009）も DTO もここに閉じる。
ファイルのメモリマップ・原子的な書き戻し・履歴・ジャンプリスト（`ICustomDestinationList`）・OS のテーマ読み取りもここだけが行う。

### `src/ui/win32`

application が作った表示値を Direct2D / DirectWrite で描き、キー・マウス・IME の操作を意図として渡す（ARC-011・CPP-017）。可変性の隔離区画（ARC-005）。

本文の文字組み資源はDirect2DRendererの`layout_of`が所有し、現在/直前の描画で使う全文と幾何が一致するときだけ再利用する。フォント・DPI変更で破棄し、画面を離れた資源は次の描画で手放す。本文の正本や編集状態は持たない（ADR 0069）。

### `src/app`

依存を結ぶ。端末出力と終了コードを扱ってよい唯一の場所（ARC-006）。

---

## 4. ソースの置き場

| 種類 | 置き場 |
| --- | --- |
| production | `src/core` / `src/application` / `src/adapters/win32` / `src/ui/win32` / `src/app`（2026-09-15 の最初の縦切りで全層を作った。ADR 0007。Win32 の版 metadata だけを `src/app` の template から build 下へ生成する） |
| テスト | `tests/build`（C++23 基盤のスモーク）/ `tests/unit`（OS 非依存の中核。ASan / UBSan 付き。`--coverage-negative` で失敗系を省く反例。Vim の fixture もここ）/ `tests/conformance`（検査器自身の正例・反例。規約検査の対象外だが決定性の禁止は適用する）/ `tests/ui`（module `window_tests`・CTest `nib_window`。ui/win32 の描画の保持の契約を窓を作らずに試す。OS の資源は DirectWrite の factory と WIC のソフトウェアの描画先まで・ADR 0077） |
| 検査設定 | `.clang-format` / `.clang-tidy` / `eng/*.json`。参照の一覧は `eng/config-bindings.json`（CNF-007） |
| 実測と証明 | `eng/measure-language.ps1` ＋ `eng/probes/language.json`（Phase 0）/ `eng/prove-gates.py`（ゲートの反例の証明。`check.ps1 -Full` の中で走り、関連する変更のときに選ぶ）/ `eng/protected-diff.py`（base と head の間の保護対象の差分と scope ごとの checks 数を `out/protected/` に記録する。ゲートではない・Issue #130） / `eng/usage-report.py`（transcript の usage を席ごとに turns・文脈の最大・cache read・seat_tokens で集計し `out/usage/` に記録する。読むだけ・ゲートではない・Issue #146）。結果は `docs/quality/` |
| 生成物 | `build/`（CMake・オブジェクト・検証 exe）/ `out/`（Phase 0 の実測・証明 fixture・測定ビルド・出力）。製品 C++ コードの生成は未採用 |
| 利用者データ | `%LOCALAPPDATA%\NeNeNib\`。`settings.v1` は ADR 0020 の UTF-8 / version=1（テーマ・本文フォント名・pt）。`session.v1`（前回のタブ・ADR 0059）と `history.v1`（閉じたファイルの履歴・ADR 0060）は採用済み。`bookmarks.v1`（明示登録・ADR 0063）も採用済み。リポジトリには入れない |
