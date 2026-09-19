# 計画: `:colorscheme` で有名なカラーテーマを選べるようにする（2026-09-15）

> Status: C1〜C4b実装（施主決定 D13・2026-09-15、最新 #70 / ADR 0025）。統合状態はGitHubが正。それぞれ焦点IssueとADRを持つ。
> 関連: [採用した見た目](../design/2026-09-15-look.md)・[ADR 0008](../adr/0008-adopted-look-tabs-titlebar-statusbar-mica.md) 決定 8（テーマは `Palette` の値型で、UI は色の定数を持たない）

## 1. 施主の要望

Solarized 系・Monokai・Dracula・One Dark・Night Owl などの有名なカラーテーマを、Vim の `:colorscheme <name>` で選んで切り替えられること。

## 2. テーマの模型（決めておくこと）

| 層 | 内容 | 所有 |
| --- | --- | --- |
| **UI トークン** | 採用案の 12 個（`background` `text` `muted` `gutter` `current_line` `title_bar` `tab_active` `status` `accent` `selection` `toggle` `panel` / `panel_border`） | `core::Palette`（今ある） |
| **本文トークン** | `foreground` `background` `cursor` `selection` `current_line` `line_number` `comment` `keyword` `string` `number` `type` `function` `constant` `operator` `error` `warning`（16 個。base16 の 16 色を Nib の役割名で持つ） | `core::SyntaxPalette`（ハイライトの縦切りで足す） |
| **テーマ** | 名前・明暗（`dark` / `light`）・UI トークン・本文トークン・出典と許諾 | `core::Theme` |
| **導出** | 有名テーマは本文トークン（背景・前景・アクセント・コメント）しか定義しないものが多い。UI トークンは**決定的な導出関数** `derive_ui(background, foreground, accent, appearance)` で作り、必要なテーマだけ個別に上書きする | `core::ThemeDerivation`（純関数・単体テスト） |

- 組み込みテーマは `constexpr` の表。名前は Vim 流の小文字ハイフン（`ubuntu-aubergine` `neutral-light` `solarized-dark` `solarized-light` `monokai` `dracula` `one-dark` `night-owl` `night-owl-light`）。`_` も同じ名前として受ける
- 出典と許諾は表に残す。色の値そのものは著作物ではないが、名前と配色の出典を書き、テーマの実装コードは写さない

| テーマ | 出典 | 許諾 | 明暗 |
| --- | --- | --- | --- |
| Solarized（dark / light） | Ethan Schoonover | MIT | 両方 |
| Monokai | Wimer Hazenberg（原作） | 色の値を使う。名前は原作者に帰属 | dark |
| Dracula | Zeno Rocha ほか | MIT | dark |
| One Dark | Atom（GitHub） | MIT | dark |
| Night Owl（＋ Light Owl） | Sarah Drasner | MIT | 両方 |
| Ubuntu Aubergine / Neutral Light | Nib の採用案 | 本リポの MIT | dark / light |

## 3. 選択の規則（`:colorscheme`）

- `:colorscheme` だけ → 現在のテーマ名を表示。`:colorscheme <name>` → 切り替えて設定に保存。`:colorscheme system` → OS のライト／ダーク追従に戻す（既定。ダーク＝`ubuntu-aubergine`・ライト＝`neutral-light`）
- 明示的に選んだテーマは OS の切り替えで変わらない（`system` にしたときだけ追従）
- 名前は `Ctrl+P` の `:` 接頭辞でも絞り込み・選択できる（通常モードでも同じ一覧を使う。Issue #66・ADR 0023・ARC-001）。存在しない/壊れたテーマは `ThemeLookupFailure` の名前と理由で断り、既定へ黙って落とさない（Issue #70・ADR 0025）
- 利用者のテーマは版付きのファイル（`%LOCALAPPDATA%\NeNeNib\themes\<name>.v1.theme`）から同じ `Theme` を作る。形式はADR 0024、選択/保存/起動時の128件カタログはADR 0025

## 4. 縦切りの順（依存の順）

| 順 | 縦切り | 何ができるか | 前提 |
| --- | --- | --- | --- |
| C1 | テーマの模型と組み込み 9 テーマ（core だけ） | `Theme` / `SyntaxPalette` / `derive_ui` と組み込みの表。**全テーマで本文の前景／背景のコントラスト比 4.5 以上を単体テストが要求する**。UI はまだ `Palette` だけを使う | 無し（いつでも） |
| C2 | 設定の保存形式（版付き）とテーマ・本文フォントの永続化 | `settings.v1` にテーマ名・フォント名・pt。起動時に読む。読めない版は型のある失敗（ARC-009）。直接のサイズ操作も接続（Issue #60・ADR 0020） | C1 |
| C3a | Ex入力とテーマ/フォント設定（Issue #64・ADR 0022） | NORMALの `:` で切替・表示・Tab補完。本文とは独立。C2と同じ保存へ接続 | C2・Vim NORMAL |
| C3b | Ctrl+Pの共通一覧と `:` 接頭辞（Issue #66・ADR 0023） | 通常/Vim全モードから同じ候補・入力編集・テーマ/設定評価を利用。ファイル等の統合は別Issue | C3a・共通一覧 |
| C4a | 利用者テーマの形式・所有・読込（Issue #68・ADR 0024） | `themes/<name>.v1.theme` を既存FilePortから読み、所有するThemeDocumentへ厳密に検証して既存Themeへ写す | C2 |
| C4b | 利用者テーマの一覧・選択接続（Issue #70・ADR 0025） | 起動時カタログ、設定保存/復元、Ex/Ctrl+P、名前付きの失敗表示へC4aを接続 | C3b・C4a |

ハイライト（`SyntaxPalette` を実際に本文へ塗る）はハイライトの縦切りで、C1 の型をそのまま使う。

## 5. 機械で守ること

- ui/win32 に `Palette` / `SyntaxPalette` 以外の色のリテラルを書かない（CNF の字句検査で `RgbColor{` を ui で拒否する予定）
- 組み込みテーマの表は単体テストで「名前の重複なし」「全トークンが埋まっている」「本文のコントラスト比 4.5 以上」「UI の `text` / `background` も 4.5 以上」を要求する（QLT-008 / QLT-009）
- テーマ名の一覧は `core::ThemeCatalog` が既存の組み込み表と利用者recordを統合し、Ex の補完・Ctrl+P・設定の検証が同じ表を使う（ARC-001）

## 6. フォントサイズ（施主決定 D14・2026-09-15）

同じ設定の縦切り（C2）で扱う。テーマと同じく application が所有する状態で、UI は写すだけ。

- 状態: `FontSize`（core の値型。8〜40 pt に検証するファクトリ。既定 13.5 pt）。DPI との掛け算は `Direct2DRenderer` が描くときに行い、状態は pt のまま
- 意図: 増加（+1 pt）/ 減少（−1 pt）/ 既定へ戻す。C2 では通常・Vim とも Ctrl+`+` / Ctrl+`-` / Ctrl+`0` と Ctrl+ホイール。C3aで `:set fontsize=<pt>` / `:set guifont=<name>:h<pt>` を同じ保存へ接続（名前中の空白を受け、最後の`:h`でptと分ける）。Ctrl+Pの一覧はC3b
- 保存: `settings.v1` に pt を書く（C2）。読めない値は既定へ黙って落とさず、型のある失敗で返す（ARC-009）
- レイアウト: 本文は `pt × 96 / 72` DIP、行高はこの DIP に 1.6 を掛けて整数 DIP に丸める（13.5 pt → 18 DIP → 行高 29 DIP）。行番号の欄・文字・右余白は、既定 13.5 pt の 56 / 12 / 16 DIP を基準に pt の比で拡縮する。`TitleBarLayout` / `StatusBarLayout` は固定のまま（UI の文字は変えない）
- 順: C2 は保存と直接のサイズ操作、C3 は Ex / Ctrl+P。Ctrl+ホイールは C2 に含める（ADR 0020）
