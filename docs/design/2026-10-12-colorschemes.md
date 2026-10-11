# 配色の拡充と行番号欄（2026-10-12）

Issue #419、FR-016、ADR 0117。既存Theme/ThemeCatalogを使い、色の出典と編集画面への適応を記録する。

現在は#421の[映画30種追加](2026-10-12-cinema-collection.md)により**全125種・映画37種**。以下の#419時点95種に追加30種を合わせた構成で、既存95種の色と順序は変更していない。

## 収載の基準

Vim標準は[公式runtime/colors](https://github.com/vim/vim/tree/master/runtime/colors)の調査時点28名称を対象にする。
IDE系はVS Codeの標準テーマ、Darcula/IntelliJ、GitHub、One Light、Nord、Gruvbox、Tokyo Night、Catppuccin、Ayu、Material、Rosé Pine、Everforest、Kanagawa、Cobalt2、SynthWave '84を対象にし、上流の公開配色を確認して明暗を揃える。
既存9テーマの本文とUIを基礎として保持し、行番号欄の配色を調整する。組み込みの正確な一覧と出典版を以下に記録する。

## 映画からの着想

| テーマ名 | 参考作品 | 配色の方向 |
| --- | --- | --- |
| alloy-core | Iron Man | 炭色・深い赤・金・冷たいシアン |
| verdant-rage | The Incredible Hulk | 墨緑・鮮やかな緑・紫 |
| scarlet-web | Spider-Man | 夜の紺・赤・青・暖かい白 |
| relic-sand | Indiana Jones | 明るい古紙・革の茶・赤銅・オリーブ |
| soap-rebellion | Fight Club | 暗い灰緑・石鹸のピンク・酸味のある黄緑 |
| code-rain | The Matrix | 緑を帯びた黒・蛍光緑・淡いミント |
| arcane-library | Harry Potter | 夜の青・琥珀・紫・古紙の白 |

独自の配色と名称で、作品公式のテーマではない。出典ポスターの画像/ロゴ/台詞は同梱しない。
[MonaLisa](https://github.com/ptdewey/monalisa-nvim)は同名の公開テーマを参照する。MonoLisaフォントとは別のものとして扱う。

## 行番号と構文の役割

行番号背景を本文から一段分け、数字は背景に4.5:1以上のコントラストを持たせる。現在の行番号は本文の前景で強調する。
暗いテーマでも欄を明るくする場合があり、明るいテーマでも欄を白く抜く場合がある。明暗から一律に決めず各配色の階調で選ぶ。
現在行の帯は本文だけを横切り、折り返し行でも行番号欄の面を保つ。

Python/TypeScriptの後続ハイライトはSyntaxPaletteの既存役割へ接続する。キーワード、文字列、コメント、数値、型、関数、定数、演算子を別に用意し、色見本にも両言語の短いコードを使う。
今回の見本は役割の配色を比較するもので、製品に言語解析が実装された証拠とはしない。

## 見本と色表

[配色見本（HTML）](colorschemes.html)をダウンロードしてブラウザで開く。外部通信なしで全125種の検索・分類・明暗の絞り込み、Python/TypeScriptの役割見本と現在の単色本文の切り替えができる。映画37種とMonaLisaは比較カードも置き、追加30種を先頭に並べる。

色の正本は `src/core/BuiltinThemes.hpp` の `builtin_themes` と、その本文トークン定数 `BuiltinThemePalettes.hpp`。HTMLはC++の表をコンパイルして取り出した125件のスナップショットで、製品の描画経路や言語解析の代替実装ではない。出典調査の記録 [theme-sources.json](../theme-sources.json) は#419の86件と#421の30件、計116件の値・役割・出典・調整履歴であり、ビルドや実行時には読まない。

### 初回95種の収載名

追加30種は[映画コレクションの一覧](2026-10-12-cinema-collection.md)を参照する。

既存9種: `ubuntu-aubergine`, `neutral-light`, `solarized-dark`, `solarized-light`, `monokai`, `dracula`, `one-dark`, `night-owl`, `night-owl-light`。

**Vim（28種）**: `blue`, `catppuccin`, `darkblue`, `default`, `delek`, `desert`, `elflord`, `evening`, `habamax`, `industry`, `koehler`, `lunaperche`, `morning`, `murphy`, `novum`, `pablo`, `peachpuff`, `quiet`, `retrobox`, `ron`, `shine`, `slate`, `sorbet`, `torte`, `unokai`, `wildcharm`, `zaibatsu`, `zellner`。

**IDE（50種）**: `abyss`, `vscode-dark-2026`, `vscode-light-2026`, `vscode-dark-modern`, `vscode-dark-plus`, `vscode-dark`, `vscode-high-contrast`, `vscode-high-contrast-light`, `vscode-light-modern`, `vscode-light-plus`, `vscode-light`, `kimbie-dark`, `monokai-dimmed`, `quiet-light`, `red`, `tomorrow-night-blue`, `catppuccin-latte`, `catppuccin-frappe`, `catppuccin-macchiato`, `nord`, `gruvbox`, `gruvbox-light`, `tokyo-night`, `tokyo-night-storm`, `tokyo-night-moon`, `tokyo-night-light`, `github-dark`, `github-light`, `github-dark-dimmed`, `one-light`, `intellij-light`, `darcula`, `rose-pine`, `rose-pine-moon`, `rose-pine-dawn`, `everforest-dark`, `everforest-light`, `kanagawa`, `kanagawa-dragon`, `kanagawa-lotus`, `material-oceanic`, `material-darker`, `material-lighter`, `material-palenight`, `material-deep-ocean`, `ayu-dark`, `ayu-mirage`, `ayu-light`, `cobalt2`, `synthwave-84`。

**MonaLisa（1種）**: `monalisa`。

**Cinema（7種）**: `alloy-core`, `verdant-rage`, `scarlet-web`, `relic-sand`, `soap-rebellion`, `code-rain`, `arcane-library`。

`catppuccin` はVim標準のMochaを採り、Latte/Frappe/Macchiatoを別名で加えた。Vimの明暗両対応テーマは最初の完全なGUI配色を採用し、`default` はVimのdark GUI既定値を使う。名前を揃えることと、Vimの全GUI/cterm変種を再現することは区別する。SynthWave '84の発光等、テーマ固有の描画効果は含まない。

### 出典と版

上流の実装コードは取り込まず、公開色値・役割対応から固定値を記述した。正式な著者名、ライセンス、色ファイルのURLは全Themeにも保持する。[THIRD_PARTY_NOTICES.txt](../../THIRD_PARTY_NOTICES.txt)に出典と許諾文を収載し、配布ZIPにも含める。Materialは公開色見本の参照であり、MITと表記しない。映画画像は参照のみで製品・見本に含めない。

| 出典 | 固定版/参照先 | 対象 |
| --- | --- | --- |
| vim/vim | [99b9dd00cd18](https://github.com/vim/vim/blob/99b9dd00cd180bade5b8b08c00e4e8ae2e553dbe/runtime/colors/blue.vim) | `blue`, `catppuccin`, `darkblue`, `default`, `delek`, `desert`, `elflord`, `evening`, `habamax`, `industry`, `koehler`, `lunaperche`, `morning`, `murphy`, `novum`, `pablo`, `peachpuff`, `quiet`, `retrobox`, `ron`, `shine`, `slate`, `sorbet`, `torte`, `unokai`, `wildcharm`, `zaibatsu`, `zellner` |
| microsoft/vscode | [6133b4256005](https://github.com/microsoft/vscode/blob/6133b425600558c15ca5238ce483a2f5bdb8db9c/extensions/theme-abyss/themes/abyss-color-theme.json) | `abyss`, `vscode-dark-2026`, `vscode-light-2026`, `vscode-dark-modern`, `vscode-dark-plus`, `vscode-dark`, `vscode-high-contrast`, `vscode-high-contrast-light`, `vscode-light-modern`, `vscode-light-plus`, `vscode-light`, `kimbie-dark`, `monokai-dimmed`, `quiet-light`, `red`, `tomorrow-night-blue` |
| ptdewey/monalisa-nvim | [2ffe6db37fca](https://github.com/ptdewey/monalisa-nvim/blob/2ffe6db37fcad17da0d210f5b3a357712d0b8a2b/fnl/monalisa/init.fnl) | `monalisa` |
| catppuccin/palette | [07d02aa110ef](https://github.com/catppuccin/palette/blob/07d02aa110ef9eb7e7427afca5c73ba9cf7f8ebd/palette.json) | `catppuccin-latte`, `catppuccin-frappe`, `catppuccin-macchiato` |
| nordtheme/nord | [1cef71605416](https://github.com/nordtheme/nord/blob/1cef71605416a222e57225b544540ce0fcec18d4/src/nord.scss) | `nord` |
| morhetz/gruvbox | [ef8864bb42bf](https://github.com/morhetz/gruvbox/blob/ef8864bb42bf244f0295d1c5a403b27e3d139695/colors/gruvbox.vim) | `gruvbox`, `gruvbox-light` |
| folke/tokyonight.nvim | [cdc07ac78467](https://github.com/folke/tokyonight.nvim/blob/cdc07ac78467a233fd62c493de29a17e0cf2b2b6/lua/tokyonight/colors/night.lua) | `tokyo-night`, `tokyo-night-storm`, `tokyo-night-moon` |
| enkia/tokyo-night-vscode-theme | [7c0f11eaef32](https://github.com/enkia/tokyo-night-vscode-theme/blob/7c0f11eaef322f293621ca7befe462214b7ea468/themes/tokyo-night-light-color-theme.json) | `tokyo-night-light` |
| primer/github-vscode-theme | [cd78e5e4e7bc](https://github.com/primer/github-vscode-theme/blob/cd78e5e4e7bcf132a6f428ae0f32264bb1b729cf/src/theme.js) | `github-dark`, `github-light`, `github-dark-dimmed` |
| atom/one-light-syntax | [d84579027410](https://github.com/atom/one-light-syntax/blob/d84579027410c576086dfca14d934c4bd74b0438/styles/colors.less) | `one-light` |
| JetBrains/intellij-community | [8886d47ff4b7](https://github.com/JetBrains/intellij-community/blob/8886d47ff4b7a538583b793ba29e9cc642ae01d9/platform/platform-resources/src/DefaultColorSchemesManager.xml) | `intellij-light`, `darcula` |
| rose-pine/palette | [92af52b465ab](https://github.com/rose-pine/palette/blob/92af52b465ab6e47437aca223c9b8d3009a2023b/palette.json) | `rose-pine`, `rose-pine-moon`, `rose-pine-dawn` |
| sainnhe/everforest | [85a86eb62409](https://github.com/sainnhe/everforest/blob/85a86eb62409e3ec88713bff3d1b9d7374e112e4/autoload/everforest.vim) | `everforest-dark`, `everforest-light` |
| rebelot/kanagawa.nvim | [bb85e4bfc8d8](https://github.com/rebelot/kanagawa.nvim/blob/bb85e4bfc8d89b0e62c8fa53ccdd13d12e2f77b3/lua/kanagawa/colors.lua) | `kanagawa`, `kanagawa-dragon`, `kanagawa-lotus` |
| Material Theme public palette | [公開色見本](https://material-theme.com/docs/reference/color-palette/) | `material-oceanic`, `material-darker`, `material-lighter`, `material-palenight`, `material-deep-ocean` |
| ayu-theme/vscode-ayu | [2c6feb7215bf](https://github.com/ayu-theme/vscode-ayu/blob/2c6feb7215bf7ca6c8c9a51066f255025df3a5b9/ayu-dark.json) | `ayu-dark`, `ayu-mirage`, `ayu-light` |
| wesbos/cobalt2-vscode | [c4e9574372b8](https://github.com/wesbos/cobalt2-vscode/blob/c4e9574372b85afad1682ed0fdd1ac0411c62512/theme/cobalt2.json) | `cobalt2` |
| robb0wen/synthwave-vscode | [ecfa2fe1279f](https://github.com/robb0wen/synthwave-vscode/blob/ecfa2fe1279f7233663fa3f98a96e6756000567b/themes/synthwave-color-theme.json) | `synthwave-84` |

GitHub配色の実体は公式テーマが使う `@primer/primitives` 7.10.0。npm配布物のSHA512整合性を確認し、dark/light/dark_dimmedのscaleを参照した。参照ファイルごとのURL/SHA256と調査スクリプトは作業証跡へ保存する。

### 適応の規則

背景・前景・16役割を上流から割り当て、足りない役割は同じ配色の近い役割へ対応させた。VS Code/TextMateはcomment/keyword/string/constant.numeric/entity.name.type/entity.name.function/constant.language/keyword.operator、VimはComment/Statement/String/Number/Type/Function/Constant/Operatorを基本対応とする。言語ごとのトークン列そのものは今後実装する。

追加86種は本文前景と構文10役割が本文背景に対して4.5:1以上になるよう、必要な色だけ黒または白への最小の1%刻みの混合で調整した。元の値と変更後はJSONの`adjustments`に記録する。指定のないカーソル/選択/現在行は同系色から埋める。選択背景と現在行背景は文字そのものではないため4.5:1の対象にしない。

UIは既存の整数`derive_ui`で一貫して導出する。行番号背景は上流指定または本文の階調から選び、数字と現在行の前景の両方を4.5:1以上とする。通常番号の`ui.gutter`と将来の本文役割`body.line_number`は別の意味である。新規テーマでは同じ番号色を採り、既存9種では本文の原色を維持してUI側だけ調整する。

黒/白の背景を持つ配色では、帯やパネルが本文と同色でも有効である。`on_accent`は本文背景ではなくaccentとの区別を確認する。本文前景、通常/現在行番号のコントラストと、行番号背景の区別は全125種で検査する。

### 既存9種の行番号欄

| 名称 | 本文背景（維持） | 行番号背景 | 通常番号 |
| --- | --- | --- | --- |
| ubuntu-aubergine | `#300A24` | `#27081D` | `#8B7986` |
| neutral-light | `#F4F5F7` | `#E6E9EE` | `#63696F` |
| solarized-dark | `#002B36` | `#00232C` | `#77898F` |
| solarized-light | `#FDF6E3` | `#FDFAEF` | `#6A7474` |
| monokai | `#272822` | `#20201C` | `#8B8777` |
| dracula | `#282A36` | `#20222C` | `#7B88B2` |
| one-dark | `#282C34` | `#20242A` | `#838B9D` |
| night-owl | `#011627` | `#011220` | `#6B7F91` |
| night-owl-light | `#FBFBFB` | `#FCFCFC` | `#67777F` |

### 映画参照

公式サイトのポスター/パッケージ図版を目視した。色をそのまま採取した複製ではなく、下記の特徴から編集画面の色を独自に設計した。法的な非侵害の保証や権利者からの許諾取得を示すものではない。

| 配色 | 公式参照 | 着想の具体例 |
| --- | --- | --- |
| alloy-core | [Marvel: Iron Man](https://marvel.disney.co.jp/movie/ironman) | 赤と金の装甲、青白い光、深い影 |
| verdant-rage | [Marvel: The Incredible Hulk](https://marvel.disney.co.jp/movie/hulk) | 深緑の大きな面、青緑の夜景、補助色に紫 |
| scarlet-web | [Sony: Spider-Man](https://www.sonypictures.com/movies/spiderman) | 赤と青の衣装、暖かい黄白色。本文は紺に落とす |
| relic-sand | [Lucasfilm: Raiders of the Lost Ark](https://www.lucasfilm.com/productions/raiders-of-the-lost-ark/) | セピアの石壁と革、暖色の題字、古紙の余白 |
| soap-rebellion | [20th Century: Fight Club](https://www.20thcenturystudios.com.au/movies/fight-club) | ピンクの石鹸、黄緑の光、暗い灰緑 |
| code-rain | [Warner Bros.: The Matrix](https://www.warnerbros.co.jp/home_entertainment/ecjq-5h552pk/) | 青緑の影、緑の文字列、冷たい白 |
| arcane-library | [Warner Bros.: Harry Potter and the Philosopher's Stone](https://www.warnerbros.co.jp/home_entertainment/efaxd_xdd/) | 夜の青、金の題字、紫の影と古紙色 |

## 互換性と使い方

`:colorscheme monalisa` / `:colorscheme alloy-core` / `:colorscheme vscode-dark-modern`などで選ぶ。Ctrl+Pで `:colorscheme ` から絞り込む経路も同じ。名前の`_`は`-`に正規化し、選択は従来のsettings.v3へ保存して次回復元する。

利用者テーマv1には任意の `ui.gutter_background=#RRGGBB` を追加できる。省略時は最終的な`ui.background`と同じ背景になる。古いアプリは追加キーを未知として拒否する。新しい必須キーや形式版の変更はない。

125種の組み込み名と`system`は予約名。従来の利用者テーマが追加名と同じ場合は組み込みが選ばれ、利用者ファイルは上書きせず予約名エラーを表示する。利用者側のファイル名と`name`を別の同じ名前へ変更すれば選択できる。
