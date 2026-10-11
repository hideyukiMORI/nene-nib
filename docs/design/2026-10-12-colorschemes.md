# 配色の拡充と行番号欄（2026-10-12）

Issue #419、FR-016、ADR 0117。既存Theme/ThemeCatalogを使い、色の出典と編集画面への適応を記録する。

## 収載の基準

Vim標準は[公式runtime/colors](https://github.com/vim/vim/tree/master/runtime/colors)の調査時点28名称を対象にする。
IDE系はVS Codeの標準テーマ、Darcula/IntelliJ、GitHub、One Light、Nord、Gruvbox、Tokyo Night、Catppuccin、Ayu、Material、Rosé Pine、Everforest、Kanagawa、Cobalt2、SynthWave '84を対象にし、上流の公開配色を確認して明暗を揃える。
既存9テーマの本文とUIを基礎として保持し、行番号欄の配色を調整する。組み込みの正確な一覧と出典版は実装の表と照合してこの文書へ追記する。

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
