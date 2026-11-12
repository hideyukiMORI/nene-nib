# Vim 9.1 の `:s` の族の実測（2026-11-12・#1007 段 1）

Vim 9.1.0004（Windows・`--not-a-term`・`-u NONE`）に `eng/vim-oracle.py` の `measure`（viewport の形）で流した答え。測る道具は `D:/NeNeNib/scripts/20261112/1007/measure.py` と `measure_extra.py`。行・桁は 1 始まり。⏎ は改行、→ は Tab、^@ は NUL。fixture（`substitute-` の接頭辞・170 件）はこの表から本文に NUL と BS を含む 2 形（`rep-nl` `rep-backslash-b`・oracle の literal が制御文字を書けない・契約 `--ex-substitute` で守る）と、段 1 が断る 2 形（`s-c-confirm` `multiline-pattern`）と設定つきの 1 形（`s-I-over-ic`）を除いた 157 件。

## 主な形（139 形）

| 名前 | 本文 | 鍵 | Vim の本文 | 行,桁 |
|---|---|---|---|---|
| s-current | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-current-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>` | X1 X2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-percent | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/<CR>` | X1 a2⏎  b X⏎c X a⏎X⏎→x X y⏎lXst a | 6,1 |
| s-percent-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/g<CR>` | X1 X2⏎  b X⏎c X X⏎X⏎→x X y⏎lXst X | 6,1 |
| s-line-n | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:2s/a/X/<CR>` | a1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-n-m-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:2,4s/a/X/g<CR>` | a1 a2⏎  b X⏎c X X⏎X⏎→x a y⏎last a | 4,1 |
| s-dot | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:.s/a/X/<CR>` | a1 a2⏎  b a⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| s-dollar | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:$s/a/X/<CR>` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎lXst a | 6,1 |
| s-dot-dollar | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:.,$s/a/X/<CR>` | a1 a2⏎  b a⏎c X a⏎X⏎→x X y⏎lXst a | 6,1 |
| s-dot-plus | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:.,.+2s/a/X/<CR>` | a1 a2⏎  b X⏎c X a⏎X⏎→x a y⏎last a | 4,1 |
| s-visual-line | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `Vj:s/a/X/<CR>` | a1 a2⏎  b X⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| s-visual-char-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `vjj:s/a/X/g<CR>` | X1 X2⏎  b X⏎c X X⏎a⏎→x a y⏎last a | 3,1 |
| s-visual-marks-typed | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `Vj<Esc>G:'<,'>s/a/X/<CR>` | a1 a2⏎  b X⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| s-marks | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `majjmb:'a,'bs/a/X/g<CR>` | X1 X2⏎  b X⏎c X X⏎a⏎→x a y⏎last a | 3,1 |
| s-count-colon | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `3:s/a/X/<CR>` | a1 a2⏎  b X⏎c X a⏎X⏎→x a y⏎last a | 4,1 |
| s-semicolon | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:2;+1s/a/X/<CR>` | a1 a2⏎  b X⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| s-plus | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:+1s/a/X/<CR>` | a1 a2⏎  b a⏎c a a⏎X⏎→x a y⏎last a | 4,1 |
| s-minus | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:-1s/a/X/<CR>` | a1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-plus-dollar-minus | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:.+1,$-1s/a/X/g<CR>` | a1 a2⏎  b X⏎c X X⏎X⏎→x X y⏎last a | 5,2 |
| s-line-zero | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:0s/a/X/<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-beyond | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:9s/a/X/<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 3,1 |
| s-backwards | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:4,2s/a/X/<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 3,1 |
| s-mark-unset | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:'zs/a/X/<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 3,1 |
| s-three-addresses | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:1,2,4s/a/X/g<CR>` | a1 a2⏎  b X⏎c X X⏎X⏎→x a y⏎last a | 4,1 |
| s-plus-beyond | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:.,.+9s/a/X/<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 3,1 |
| s-bare-plus | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:.,+2s/a/X/<CR>` | a1 a2⏎  b X⏎c X a⏎X⏎→x a y⏎last a | 4,1 |
| s-hash | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s#a#X#<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-at-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s@a@X@g<CR>` | X1 X2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-comma | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s,a,X,<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-no-trailing-delimiter | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-no-replacement | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a<CR>` | 1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-empty-replacement | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/<CR>` | 1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-escaped-delimiter-in-pattern | p/x q/x | `:s/\\/x/Y/<CR>` | pY q/x | 1,1 |
| s-slash-with-hash | p/x q/x | `:s#/#-#g<CR>` | p-x q-x | 1,1 |
| s-escaped-slash-in-replacement | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\//g<CR>` | /1 /2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-letter-delimiter | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s xaxXx<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-empty-uses-search | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `/b<CR>gg:s//X/<CR>` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-empty-uses-substitute | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j:s//Y/<CR>` | X1 a2⏎  b Y⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-empty-no-previous | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s//X/<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-search-after-substitute | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/b/X/<CR>/a<CR>:s//Y/<CR>` | a1 a2⏎  X Y⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-i | foo bar foo⏎FOO Bar⏎foo | `:s/FOO/X/i<CR>` | X bar foo⏎FOO Bar⏎foo | 1,1 |
| s-I-over-ic | foo bar foo⏎FOO Bar⏎foo | `:s/FOO/X/I<CR>` | foo bar foo⏎X Bar⏎foo | 2,1 |
| s-gi | foo bar foo⏎FOO Bar⏎foo | `:%s/foo/X/gi<CR>` | X bar X⏎X Bar⏎X | 3,1 |
| s-n-count | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/n<CR>` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-gn-count | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/gn<CR>` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-amp-keeps-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>j:s/a/Y/&<CR>` | X1 X2⏎  b Y⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-count | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/ 2<CR>` | X1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| s-count-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g 3<CR>` | X1 X2⏎  b X⏎c X X⏎a⏎→x a y⏎last a | 3,1 |
| s-count-no-space | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g3<CR>` | X1 X2⏎  b X⏎c X X⏎a⏎→x a y⏎last a | 3,1 |
| s-count-clamped | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:4s/a/X/ 10<CR>` | a1 a2⏎  b a⏎c a a⏎X⏎→x X y⏎lXst a | 6,1 |
| s-count-zero | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/ 0<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-e-no-match | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/zzz/X/e<CR>x` | 1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-no-match | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/zzz/X/<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-trailing | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/q<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-trailing-comment | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/ "note<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| s-c-confirm | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/c<CR>` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/[&]/g<CR>` | [a]1 [a]2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-zero | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/[\\0]/<CR>` | [a]1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-escaped-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\&/<CR>` | &1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-tilde | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j:s/a/~~/<CR>` | X1 a2⏎  b XX⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| rep-tilde-chain | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j:s/a/<~>/<CR>j:s/a/~/<CR>` | X1 a2⏎  b <X>⏎c <X> a⏎a⏎→x a y⏎last a | 3,1 |
| rep-escaped-tilde | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\~/<CR>` | ~1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-tilde-no-previous | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/<~>/<CR>` | <>1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-cr | one two three⏎next | `:s/ /\\r/g<CR>` | one⏎two⏎three⏎next | 3,1 |
| rep-cr-middle-lines | line01 a b a⏎line02 a b a⏎line03 a b a⏎line04 a b a⏎line05 a b a⏎line06 a b a⏎line07 a b a⏎line08 a b a⏎line09 a b a⏎line10 a b a | `:2,3s/ a/\\r/g<CR>` | line01 a b a⏎line02⏎ b⏎⏎line03⏎ b⏎⏎line04 a b a⏎line05 a b a⏎line06 a b a⏎line07 a b a⏎line08 a b a⏎line09 a b a⏎line10 a b a | 7,1 |
| rep-nl | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\n/<CR>` | ^@1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-tab | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\t/<CR>` | →1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,2 |
| rep-backslash | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\\\/<CR>` | \\1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-other-escape | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\q\\z/<CR>` | qz1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-backslash-b | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\b/<CR>` | 1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-u | foo bar foo⏎FOO Bar⏎foo | `:s/foo/\\u&/<CR>` | Foo bar foo⏎FOO Bar⏎foo | 1,1 |
| rep-U | foo bar foo⏎FOO Bar⏎foo | `:s/foo/\\U&x/<CR>` | FOOX bar foo⏎FOO Bar⏎foo | 1,1 |
| rep-U-E | foo bar foo⏎FOO Bar⏎foo | `:s/foo/\\U&\\Ex/<CR>` | FOOx bar foo⏎FOO Bar⏎foo | 1,1 |
| rep-U-e | foo bar foo⏎FOO Bar⏎foo | `:s/foo/\\U&\\ex/<CR>` | FOOx bar foo⏎FOO Bar⏎foo | 1,1 |
| rep-l | foo bar foo⏎FOO Bar⏎foo | `:s/FOO/\\l&/<CR>` | foo bar foo⏎fOO Bar⏎foo | 2,1 |
| rep-L | foo bar foo⏎FOO Bar⏎foo | `:s/FOO/\\L&/<CR>` | foo bar foo⏎foo Bar⏎foo | 2,1 |
| rep-u-L | foo bar foo⏎FOO Bar⏎foo | `:s/FOO/\\u\\L&/<CR>` | foo bar foo⏎Foo Bar⏎foo | 2,1 |
| rep-L-u | foo bar foo⏎FOO Bar⏎foo | `:s/FOO/\\L\\u&/<CR>` | foo bar foo⏎Foo Bar⏎foo | 2,1 |
| rep-u-literal | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\ux/<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-U-literal | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/\\Uxy\\Ez/<CR>` | XYz1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| rep-U-non-ascii | é ß ǆ | `:s/é/\\U&/<CR>` | É ß ǆ | 1,1 |
| rep-U-sharp-s | é ß ǆ | `:s/ß/\\U&/<CR>` | é ß ǆ | 1,1 |
| rep-u-dz | é ß ǆ | `:s/ǆ/\\u&/<CR>` | é ß Ǆ | 1,1 |
| rep-U-composing | é x | `:s/é/\\U&/<CR>` | É x | 1,1 |
| rep-groups | a1 b2 | `:s/\\(a\\)\\(1\\)/\\2\\1/<CR>` | 1a b2 | 1,1 |
| rep-group-missing | a1 b2 | `:s/\\(a\\)1/[\\1\\2]/<CR>` | [a] b2 | 1,1 |
| empty-star-g | abc | `:s/x*/-/g<CR>` | -a-b-c | 1,1 |
| empty-star-g-mixed | axxb | `:s/x*/-/g<CR>` | -a-b | 1,1 |
| empty-star | abc | `:s/x*/-/<CR>` | -abc | 1,1 |
| empty-dollar | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/$/!/<CR>` | a1 a2!⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| empty-caret | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/^/> /<CR>` | > a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| empty-caret-percent | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/^/# /<CR>` | # a1 a2⏎#   b a⏎# c a a⏎# a⏎# →x a y⏎# last a | 6,1 |
| empty-dollar-percent-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/$/;/g<CR>` | a1 a2;⏎  b a;⏎c a a;⏎a;⏎→x a y;⏎last a; | 6,1 |
| empty-word-start-g | ab cd | `:s/\\</[/g<CR>` | [ab [cd | 1,1 |
| anchor-start | a a a⏎b a | `:%s/^a/X/<CR>` | X a a⏎b a | 1,1 |
| anchor-end | a a a⏎b a | `:%s/a$/X/<CR>` | a a X⏎b X | 2,1 |
| star-greedy-g | aaa b aa | `:s/a*/X/g<CR>` | X XbX X | 1,1 |
| wide | あいあ⏎うあ | `:%s/あ/い/g<CR>` | いいい⏎うい | 2,1 |
| composing-e | ée x | `:s/e/X/g<CR>` | éX x | 1,1 |
| composing-dot | éa | `:s/./X/g<CR>` | XX | 1,1 |
| wide-caret | 　　あ い | `:s/い/X/<CR>` | 　　あ X | 1,1 |
| caret-first-nonblank |    b a | `:s/a/X/<CR>` |    b X | 1,4 |
| caret-tab | →→foo a | `:s/a/X/<CR>` | →→foo X | 1,3 |
| caret-last-line | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/<CR>` | X1 a2⏎  b X⏎c X a⏎X⏎→x X y⏎lXst a | 6,1 |
| caret-last-line-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/g<CR>` | X1 X2⏎  b X⏎c X X⏎X⏎→x X y⏎lXst X | 6,1 |
| undo | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/g<CR>u` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| undo-redo | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/g<CR>u<C-r>` | X1 X2⏎  b X⏎c X X⏎X⏎→x X y⏎lXst X | 1,1 |
| undo-cr | one two three⏎next | `:s/ /\\r/g<CR>u` | one two three⏎next | 1,1 |
| pcmark | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/<CR>''` | X1 a2⏎  b X⏎c X a⏎X⏎→x X y⏎lXst a | 2,3 |
| pcmark-n | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a/X/n<CR>G''` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| search-n-after | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>n` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,4 |
| search-N-after | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `?b<CR>gg:s/a/X/<CR>n` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 6,6 |
| dot-after | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `x:s/a/X/<CR>j.` | 1 X2⏎ b a⏎c a a⏎a⏎→x a y⏎last a | 2,1 |
| endcolumn-repeat | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j$:s<CR>` | X1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,5 |
| normal-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j&` | X1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| normal-amp-drops-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>jj&` | X1 X2⏎  b a⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| normal-amp-count | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j2&` | X1 a2⏎  b X⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| normal-g-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>g&` | X1 X2⏎  b X⏎c X a⏎X⏎→x X y⏎lXst a | 6,1 |
| normal-g-amp-g | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>g&` | X1 X2⏎  b X⏎c X X⏎X⏎→x X y⏎lXst X | 6,1 |
| normal-g-amp-after-search | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>/b<CR>g&` | X1 a2⏎  X a⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| normal-amp-no-previous | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `&x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| normal-amp-dot | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>x&j.` | 1 X2⏎ b a⏎c a a⏎a⏎→x a y⏎last a | 2,1 |
| ex-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>j:&<CR>` | X1 X2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| ex-amp-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>j:&&<CR>` | X1 X2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| ex-tilde | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>/b<CR>:~<CR>` | X1 a2⏎  X a⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| ex-s-bare | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j:s<CR>` | X1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| ex-s-flags | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j:s g<CR>x` | X1 a2⏎   X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| ex-sg | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>j:sg<CR>` | X1 a2⏎  b X⏎c a a⏎a⏎→x a y⏎last a | 2,3 |
| ex-percent-amp-amp | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>:%&&<CR>` | X1 X2⏎  b X⏎c X X⏎X⏎→x X y⏎lXst X | 6,1 |
| ex-substitute-long | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:substitute/a/X/<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| ex-su | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:su/a/X/<CR>` | X1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| ex-s-no-previous | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| ex-amp-count | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/<CR>:& 3<CR>` | X1 X2⏎  b X⏎c X a⏎a⏎→x a y⏎last a | 3,1 |
| empty-doc-no-match |  | `:s/a/X/<CR>x` |  | 1,1 |
| empty-doc-caret |  | `:s/^/X/<CR>` | X | 1,1 |
| delete-all | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:%s/a//g<CR>` | 1 2⏎  b ⏎c  ⏎⏎→x  y⏎lst  | 6,1 |
| multiline-pattern | a⏎b⏎c | `:%s/\\n//<CR>` | abc | 1,3 |
| same-line-again-no-match | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | `:s/a/X/g<CR>:s<CR>x` | X1 X2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| cr-cursor-two-lines | line01 a b a⏎line02 a b a⏎line03 a b a⏎line04 a b a⏎line05 a b a⏎line06 a b a⏎line07 a b a⏎line08 a b a⏎line09 a b a⏎line10 a b a | `:2,3s/ b /\\r/<CR>` | line01 a b a⏎line02 a⏎a⏎line03 a⏎a⏎line04 a b a⏎line05 a b a⏎line06 a b a⏎line07 a b a⏎line08 a b a⏎line09 a b a⏎line10 a b a | 5,1 |

## 追加の形（24 形）

| 名前 | 鍵 | Vim の本文 | 行,桁 |
|---|---|---|---|
| visual-amp | `:s/a/X/<CR>jVj&` | 'X1 a2\n  b a\nc a a\na\n\tx a y\nlast a' | 3,1 |
| visual-g-amp | `:s/a/X/<CR>jVjg&` | 'X1 X2\n  b X\nc X a\nX\n\tx X y\nlXst a' | 6,1 |
| undo-blank-line | `:2,3s/a/X/<CR>u` | 'a1 a2\n  b a\nc a a\na\n\tx a y\nlast a' | 2,1 |
| undo-blank-line-cursor-elsewhere | `:2,3s/a/X/<CR>Gu` | 'a1 a2\n  b a\nc a a\na\n\tx a y\nlast a' | 2,1 |
| redo-blank | `:2,3s/a/X/<CR>uG<C-r>` | 'a1 a2\n  b X\nc X a\na\n\tx a y\nlast a' | 2,1 |
| op-amp | `:s/a/X/<CR>jd&x` | 'X1 a2\n  b a\nc a a\na\n\tx a y\nlast a' | 2,1 |
| all-blank-target | `:%s/x//<CR>` | 'a\n   ' | 2,3 |
| blank-line-caret | `:2s/^ /\t/<CR>` | 'xa\n\t ' | 2,2 |
| trailing-blank-target | `:s/a/ /<CR>` | '    ' | 1,4 |
| count-amp-beyond | `:s/a/X/<CR>5G9&` | 'X1 a2\n  b a\nc a a\na\n\tx a y\nlast a' | 5,2 |
| dot-marks | `:2,3s/a/X/<CR>gg'.` | 'a1 a2\n  b X\nc X a\na\n\tx a y\nlast a' | 2,3 |
| amp-pending-g | `:s/a/X/<CR>jg&` | 'X1 a2\n  b X\nc X a\nX\n\tx X y\nlXst a' | 6,1 |
| empty-repl-amp-n | `:%s/a//gn<CR>x` | 'a1 a2\n   a\nc a a\na\n\tx a y\nlast a' | 2,3 |
| s-semicolon-dot | `:2;.+1s/a/X/<CR>` | 'a1 a2\n  b X\nc X a\na\n\tx a y\nlast a' | 3,1 |
| cr-in-group | `:s/\(a\)/\1\r\1/<CR>` | 'a\nab' | 2,1 |
| upper-then-cr | `:s/a/\u\rx/<CR>` | '\nxb' | 2,1 |
| tilde-in-pattern | `:s/~/X/<CR>` | 'a~b' | 1,1 |
| e-flag-amp-amp | `:s/zzz/X/e<CR>:&&<CR>x` | '1 a2\n  b a\nc a a\na\n\tx a y\nlast a' | 1,1 |
| group-star | `:s/\(a*\)\(a\)b/[\1|\2]/<CR>` | '[aa|a]' | 1,1 |
| group-nested | `:s/\(a\(b\)c\)d/\2\1/<CR>` | 'babc' | 1,1 |
| group-anchor | `:s/\(^a\)/X\1/<CR>` | 'Xabc' | 1,1 |
| star-after-open | `:s/\(*b\)/X/<CR>` | 'aX' | 1,1 |
| undo-marks-a | `jjma:%s/a/X/<CR>'a` | 'X1 a2\n  b X\nc X a\nX\n\tx X y\nlXst a' | 3,1 |
| cr-marks-below | `5Gmagg:s/ /\r/<CR>'a` | 'a1\na2\n  b a\nc a a\na\n\tx a y\nlast a' | 6,2 |

## レビュー V-1564 の追加の形（14 形）

`count-2147483646-from-3` だけは Vim が 32 ビットの行番号の溢れで断る形で、fixture にしない（Nib は本文の終わりで止めて置換する・ADR 0168 の追補 14）。

| 名前 | 鍵 | Vim の本文 | 行,桁 |
|---|---|---|---|
| count-2147483646 | `:s/a/X/ 2147483646<CR>x` | X1 a2⏎  b X⏎c X a⏎X⏎→x X y⏎Xst a | 6,1 |
| count-2147483647 | `:s/a/X/ 2147483647<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-2147483648 | `:s/a/X/ 2147483648<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-4294967297 | `:s/a/X/ 4294967297<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-huge | `:s/a/X/ 99999999999999999999999<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-2147483646-from-3 | `:3s/a/X/g 2147483646<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-zero-e | `:s/a/X/e 0<CR>x` | 1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-zero | `:s/a/X/ 0<CR>x` | a1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-zero-e-g | `:%s/a/X/ge 0<CR>x` | 1 a2⏎  b a⏎c a a⏎a⏎→x a y⏎last a | 1,1 |
| count-zero-amp-e | `:s/a/X/e<CR>j:&& 0<CR>x` | X1 a2⏎ b a⏎c a a⏎a⏎→x a y⏎last a | 2,1 |
| question-delimiter | `:s?a\?b?X?<CR>` | X ab | 1,1 |
| question-delimiter-g | `:s?a\?b?X?g<CR>` | X X | 1,1 |
| question-in-slash | `:s/a?b/X/<CR>` | X ab | 1,1 |
| bracket-slash | `:s/[/]/X/<CR>` | aXb | 1,1 |
