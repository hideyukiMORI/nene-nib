# ADR 0017 — テーマは core の値型 `Theme`（UI トークン・本文トークン・出典）で、有名テーマの UI トークンは整数演算の `derive_ui` から導き、組み込み 9 テーマの表と名前の表は 1 か所に置く

- 状態: 受理
- 日付: 2026-09-19
- Issue: #52
- 影響する規則: ARC-001 / ARC-003 / ARC-007 / CPP-002 / CPP-003 / CPP-011 / CPP-012 / QLT-008 / QLT-009

## 文脈

施主決定 D13 は `:colorscheme <name>` で Solarized・Monokai・Dracula・One Dark・Night Owl を選べること。計画 `docs/plans/2026-09-15-colorschemes.md` は C1（模型と組み込みテーマ・core だけ）→ C2（設定の保存）→ C3（`:colorscheme` と Ctrl+P）→ C4（利用者のテーマファイル）に分けた。
いまの core は `Palette`（UI トークン 16 個。ADR 0008 の決定 8 で「テーマ 1 つ分の値型」）と `BuiltinTheme`（2 つ）と `palette_for(Appearance)`（OS の明暗 → 1 対 1）だけで、
本文トークン（ハイライトの役割名）・テーマの名前・出典と許諾・有名テーマから UI トークンを導く規則が無い。有名テーマは本文の色（背景・前景・アクセント・コメント・構文の色）しか定義しないものが多く、
タイトルバー・ステータスバー・パネルの色は Nib が決めるしかない。
制約: core は libm のシンボルを出せない（`std::lround` で ARC-003 が落ちた・ADR 0008 の決定 5）。ui/win32 は色のリテラルを持たない（ADR 0008 の決定 8）。

## 決定

**テーマは core の値型 `Theme{name, appearance, ui: Palette, body: SyntaxPalette, source: ThemeSource}`。有名テーマの UI トークンは整数演算だけの `constexpr` 純関数 `derive_ui` が本文の 3 色（背景・前景・アクセント）と明暗から導き、必要なテーマだけ個別に上書きする。組み込み 9 テーマと名前の表は `BuiltinTheme` の 1 か所。コントラスト比は tests が要求する。**

1. **`SyntaxPalette`**（core・公開 aggregate・`RgbColor` 16 個）: `foreground` `background` `cursor` `selection` `current_line` `line_number` `comment` `keyword` `string` `number` `type` `function` `constant` `operator` `error` `warning`。
   base16 の 16 色を Nib の役割名で持つ。ハイライトの縦切りがこれを本文に塗る（この Issue では塗らない）
2. **`ThemeSource`**（`std::string_view` の `author` / `license` / `url`）。色の値そのものは著作物ではないが、名前と配色の出典を表に残し、テーマの実装コード（Vim script・VS Code の JSON）は写さない
3. **`Theme`**（`name: std::string_view`・`appearance: Appearance`・`ui: Palette`・`body: SyntaxPalette`・`source: ThemeSource`）。名前は Vim 流の小文字ハイフン（`solarized-dark`）
4. **`derive_ui(RgbColor background, RgbColor foreground, RgbColor accent, Appearance) → Palette`** は `constexpr` の純関数で、**整数の線形補間 `mix(from, to, percent)` だけ**を使う（sRGB の値のまま。ガンマは扱わない）。規則は明暗ごとの百分率の表 1 つ:

   | トークン | 計算 | ダーク | ライト |
   | --- | --- | --- | --- |
   | `background` `tab_active` | 背景そのまま | | |
   | `text` | 前景そのまま | | |
   | `accent` | アクセントそのまま | | |
   | `muted` | 前景 → 背景へ | 30% | 30% |
   | `gutter` | 前景 → 背景へ | 55% | 55% |
   | `current_line` | 背景 → 前景へ | 7% | 7% |
   | `toggle` | 背景 → 前景へ | 14% | 9% |
   | `panel` | 背景 → 前景へ／背景 → 白へ | 6%（前景へ） | 100%（白） |
   | `panel_border` | 背景 → 前景へ | 22% | 15% |
   | `title_bar` | 背景 → 黒へ | 38% | 8% |
   | `status` | 背景 → 黒へ | 20% | 4% |
   | `selection` | アクセント・不透明度 | 71 | 56 |
   | `search` | アクセント → 白へ 50%・不透明度 | 89 | 77 |
   | `on_accent` | アクセントの明るさ（`(299R+587G+114B)/1000`）が 160 以上なら黒、未満なら白 | | |
   | `ime` | 前景 → アクセントへ | 25% | 25% |

   百分率は既存 2 テーマ（採用案）の値を近似したもので、**導出した 7 テーマの UI の見た目は仮**。C3 で `docs/design/look/build_boards.py` の絵にして hide が見る（D12）。ここで決めるのは規則の形（整数・決定的・1 表）であって値ではない
5. **既存 2 テーマは全部上書き**（`ubuntu_aubergine_palette` / `neutral_light_palette` の値は 1 バイトも変えない。`verify-window.py` の画素の表明がそのまま通る）。有名テーマは `derive_ui` の結果を `Theme` の表に置き、上書きが要るときはその 1 か所で書く（`derive_ui` の中にテーマ名の分岐を書かない）
6. **組み込み 9 テーマ**: `ubuntu-aubergine` `neutral-light`（採用案・本リポの MIT）・`solarized-dark` `solarized-light`（Ethan Schoonover・MIT）・`monokai`（Wimer Hazenberg・色の値を使う。名前は原作者に帰属）・`dracula`（Zeno Rocha ほか・MIT）・`one-dark`（Atom・MIT）・`night-owl` `night-owl-light`（Sarah Drasner・MIT）。
   `BuiltinTheme` は閉じた `enum` で 9。`theme_of(BuiltinTheme) → const Theme&` と、**名前の表 `theme_named(std::string_view) → std::optional<BuiltinTheme>`**（`_` を `-` と同じに受ける。大文字小文字は区別する＝名前は小文字だけ）は同じ表から引く（ARC-001。C3 の Ex の補完・Ctrl+P・C2 の設定の検証が使う）。
   本文トークンは各テーマが公開している役割の割り当て（Solarized の usage 表・Dracula の spec・One Dark の syntax・Monokai の配色・Night Owl の VS Code テーマ）から写し、割り当てが無い役割は同じテーマの近い色で埋めて表のコメントに書く
7. **コントラスト比は tests が要求する**（WCAG 2 の相対輝度と `(L1+0.05)/(L2+0.05)`。浮動小数と `std::pow` は tests 側）: 9 テーマ全部で本文の `foreground` / `background` が 4.5 以上、UI の `text` / `background` が 4.5 以上。core にはコントラストの計算を置かない（libm を出さない）。
   加えて名前の重複なし・`theme_named` の往復（9 名 ＋ `_` の別綴り ＋ 無い名前は `nullopt`）・`derive_ui` の決定性（`static_assert` で `constexpr` に評価）
8. **UI はまだ `palette_for(Appearance)`**（OS の明暗 → `ubuntu-aubergine` / `neutral-light`）。テーマの選択状態・保存・`:colorscheme` は C2 / C3。画面は変わらない

## 強制

- ARC-003 / ARC-007: core から libm・時刻・OS のシンボルが出ない — **active**（既存の `eng/symbols.py`。`derive_ui` は整数だけ）
- CPP-002: `BuiltinTheme` の `switch` に `default` を書かない — **active**（コンパイルと clang-tidy）
- コントラスト比 4.5 以上・名前の重複なし・全トークンが埋まっている — **active**（この Issue で。`nib_unit`・QLT-008 / QLT-009）
- 「ui/win32 に `Palette` / `SyntaxPalette` 以外の色のリテラルを書かない」の字句検査（計画 第 5 節の CNF）— **planned**（別 Issue）

## 結果

得られるもの: テーマの模型が 1 つに固まり、C2（名前を保存）・C3（名前で選ぶ・補完）・C4（ファイルから同じ `Theme` を作る）・ハイライト（`SyntaxPalette` を塗る）が表と経路を足すだけで増える。
失うもの: `derive_ui` は sRGB の整数補間なので、知覚的に均一ではない（同じ百分率でも明るいテーマと暗いテーマで見えの差が違う）。値の妥当性は C3 の絵で hide が見る。`Theme` の `name` / `source` は `string_view` なので、C4 のファイル由来のテーマは所有する文字列を別に持つ形が要る（C4 の ADR で）。
正直に: 有名テーマの色の値は各出典から写すが、出典の版（Dracula の spec の改訂・One Dark の Atom 版と VS Code 版）で値が違うことがある。表の `url` が指す版を正とし、違う版の値を「間違い」とは言わない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 有名テーマ 7 つの UI トークン 16 個を全部手で決める | 112 個の値を人の目で決めることになり、テーマを足すたびに設計の判断が要る。導出 ＋ 上書きなら判断は上書きの分だけ |
| `derive_ui` を浮動小数（HSL / Lab）で書く | core から libm が出て ARC-003 が落ちる（ADR 0008 の決定 5）。整数の sRGB 補間で足りるかは C3 の絵で見て、足りなければ整数の表（256 段の輝度表）を足す |
| コントラスト比を core で計算して `Theme` の生成時に拒む | 同じ libm の理由。組み込みテーマは tests で守れば十分。C4 の利用者ファイルの検証は adapters か application で（C4 の ADR） |
| テーマを JSON / TOML で持って core が読む | パーサを core に持ち込む（依存）。組み込みは `constexpr` の表、ファイルは C4 で adapters が読んで同じ `Theme` を作る（ADR 0008 の決定 8） |
| `Palette` に本文トークンを足して 32 個にする | UI と本文は変わる理由が違う（UI は Nib の採用案、本文は各テーマの定義）。別の値型にして `Theme` が両方を持つ |
| 名前を `enum` だけにして文字列の表を C3 で作る | 名前の表が 2 つになる（ARC-001）。文字列 ↔ `enum` は core の 1 表で、C3 はそれを引くだけ |

## 関連

ADR 0008（`Palette`・テーマの拡張性）・計画 `docs/plans/2026-09-15-colorschemes.md`（D13 / D14）・採用案 `docs/design/2026-09-15-look.md`・D16（`title_bar`）。
