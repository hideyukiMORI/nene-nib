# ADR 0032 — 検索は Ex と同じ入力行から入り、確定は engine の 1 つの鍵として届く

- 状態: 受理（2026-09-22・実測 218 ケースと fixture 135 件で決定 1〜7 を確認）
- 日付: 2026-09-22
- Issue: #100
- 影響する規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

FR-003 の T2「検索 `/ ? n N * #`」を実装する。Ex の `:` は `VimOpenCommandLine` 効果で `CommandLine` を開き、controller が `CommandInput`（`CommandLine` | `CommandPalette` の閉じた和型・ADR 0023）を所有し、renderer がステータス左に入力と結果メッセージを描く（ADR 0022 の決定 7）。入力中は `accept(VimKeyPress)` が早期 return するので、`VimState`（保留オペレータ・回数・記録中の `.` の鍵）は開いたまま保たれる。`.` は鍵の列を再生する（ADR 0030）。

固定 Vim 9.1 の既定は `magic` / `noignorecase` / `nosmartcase` / `wrapscan` / `nohlsearch` / `noincsearch`。help（`:help pattern` / `:help search-commands`）と実測を根拠にし、Vim ソースは読まない。実測で食い違ったら決定を直し、fixture を都合よく変えない。

## 決定

**検索の入力は Ex と同じ入力行の仕組みで受け、Enter の確定は `VimKey` の 1 つの鍵として engine に届く。パターンの照合は core の純関数で `magic` の部分集合だけを扱い、未対応の構文は黙って別の意味にせず閉じた失敗で拒否する。**

1. **入力行**: `CommandInput` に `SearchLine`（文字列・caret・方向 `VimSearchDirection{forward, backward}`）を足す。左右 / Home / End / Backspace / Delete / 1 行の貼付は `CommandLine` と同じ純関数の規則を共用し、補完（Tab）は無い。空入力の Backspace と Esc は取消。renderer はプロンプトを `/` `?` にするだけで、配置・clip・caret 追従は Ex と同じ 1 本（ARC-001）。
2. **入口**: NORMAL / VISUAL / 行単位 VISUAL / オペレータ保留中の `/` `?` は効果 `VimOpenSearch{direction}` を返し、`VimState` は変えない（保留・回数・記録中の鍵はそのまま）。INSERT では文字。
3. **確定は鍵**: Enter で controller は `VimKey` の新しい選択肢 `VimSearchPattern{pattern, direction}` を `accept(VimKeyPress)` へ 1 回送る。engine は既存の motion と同じ経路で範囲を作る（exclusive。`d/foo<CR>` は一致の先頭の手前まで・VISUAL では端点を動かす・回数は n 個目の一致）。見つからなければ取消（`VimNoEffect`）とメッセージ。`last_search`（パターンと方向）を `VimState` に持ち、`n` / `N` はそれを使う（`?` の後の `n` は後ろ向き、`N` は前向き）。`*` / `#` はキャレットの語（既存の語の種類表。語が無ければ取消）を `\<語\>` のパターンにして同じ経路を通し、`last_search` も更新する。この形なら `d/foo<CR>` も `*` も `.` の記録に鍵として残り、追加の記録無しに再生される（ADR 0030）。
4. **パターン**: core の純関数 `vim_search(text, from, pattern, direction, count) -> optional<Offset>` と、`magic` の部分集合の照合器（`src/core/VimPattern*`・1 ファイル 1 型）。対応: リテラル・`.`・`*`（直前の原子の 0 回以上）・`^`（先頭でだけ）・`$`（末尾でだけ）・`[...]` と `[^...]`（範囲 `a-z` を含む）・`\<` `\>`・`\.` `\*` `\[` `\/` `\\` などのエスケープ・`\d \D \w \W \s \S`。一致は行をまたがない（Vim の `\n` は範囲外）。`wrapscan` は既定どおり有効で、折り返したらメッセージ「search hit BOTTOM, continuing at TOP」（後ろ向きは TOP / BOTTOM）。未対応の構文（`\(` `\)` `\|` `\{` `\+` `\=` `\?` `~` `\v` `\m` `\c` `\C` `\%` `\_` と `/` の後の offset）は**黙って別の意味にせず**、閉じた失敗で拒否してメッセージを出す。`std::regex` は使わない（ロケール依存のシンボルが core へ出る。ADR 0022 で `std::format` を退けた理由と同じ）。
5. **メッセージ**: 見つからない（`E486: Pattern not found: foo`）・折り返し・未対応構文は `command_message` に出し、次の入力で消える（Ex と同じ）。`incsearch` は既定オフなので描画は変えない（`hlsearch` の描画は Issue #123 / ADR 0037 が既定オンで入れた。この決定の範囲はメッセージだけである）。
6. **IME**: 検索入力中は Ex と同じく閉じたまま。パターンの非 ASCII は UTF-8 入力 / 貼付で受ける。
7. **oracle**: fixture の鍵に `/foo<CR>` を書けば `:normal!` がそのまま検索する。パターンの `\` は JSON のエスケープに注意。見つからない検索はビープするので、その後ろの鍵は `:normal!` を命令の切れ目で分けて測る（#87 の教訓）。

## 補足（2026-09-22・実測 218 ケースの後・実装前）

固定 Vim 9.1 を 218 ケース起動した（`out/issue100-oracle/probe.py` 120・`probe2.py` 41・`probe3.py` 27・`probe4.py` 30。E486 でビープする検索は `-es` を異常終了させるので、段ごとに `try | … | catch` で包み、命令の切れ目で `:normal!` を分けた。Vim ソースは読んでいない）。決定 3 の骨格（到達位置・`n` / `N` の向き・回数の積・exclusive・VISUAL の端点・`\<語\>`・`.` への乗り方）は一致した。実測で確定した細部と、設計リナが決めた 2 点を決定に足す。

- **決定 3 の追記**: 見つからない検索も `last_search` を更新する（Vim は E486 でも `@/` を書き、次の `n` は同じ失敗を繰り返す）。空の `/<CR>` `?<CR>` は直前のパターンを再利用し、無ければ E35（`No previous regular expression`）。`*` / `#` はキャレットを語の先頭へ寄せてから探す（空白上の `*` は次の語ではなく語頭から count 回）が、オペレータの範囲は元のキャレットを端に使う（`d#` の範囲）。語が無い `*` / `#` は E348（`No string under cursor`）。回数の一致が自分自身へ折り返して範囲が空のときの扱いは追補（#1344）で改めた（`y` は空の文字単位を yank し、`c` は INSERT に入る）。
- **決定 4 の追記（走査の規則）**: 一致は行ごとに 0 桁目から数え直し、「pos 以降で最も左の一致」を取り、開始が必要な桁（前向きはキャレット＋1 バイト、後ろ向きは最後の一致）に足りなければ pos を一致の終端（長さ 0 なら 1 文字先）へ進めて繰り返し、pos が行の長さに達したらその行は打ち切る（`"aaaa"` の `/aa` は列 3、`"abc"` の `/.*` は動かない）。`^` `$` は錨の位置以外ではただの文字、閉じない `[` はリテラル。`\<` `\>` と `*` の語は `iskeyword` ではなく既存の文字の種類の表 `vim_character_class`（ADR 0031）で切る（`\<あいう\>` は `あいう漢字` に一致しない）。
- **決定 1 の追記（実装の順序）**: `EditorFrame.command_line` は `core::CommandLine` そのものを持ち、`command_line_of()` は `const CommandLine &` を返す。`SearchLine` を足す前に、描画と窓が使う入力行の見え方を「プロンプト文字・文字列・caret」の小さな値 1 つに畳み、`CommandInput` の 3 つの値がそれを返す形にする（ARC-001。描画の経路を 2 本にしない）。#100 の最初の commit はこの整えで、振る舞いは変えない。
- **fixture にできないもの**: 未対応構文（`\( \| \{ \+ \v \c` と offset。Vim では動くので期待値が食い違う。本実装の拒否は unit で守る）、CRLF（`-S` の読み込み順で `set binary` が間に合わず Vim が CR を落とす。unit で守る）、取消（`:normal!` の中では Esc が検索を実行してしまう。unit で守る）。`~` は Vim も E33 で失敗する。

## 補足（2026-09-22・実装の後）

実装は決定 1〜7 のまま通った。fixture 135 件は初回の再生で全件が固定 Vim 9.1 と一致し、`--vim-search`
の対象 unit は 1375 checks・unit 全体は 10149 checks で成功した（詳細は
[gate-proofs 5-z](../quality/gate-proofs.md)）。実測と食い違った 1 点と、実装で決めた 4 点を残す。

- **決定 4 の追記の訂正**: 「`\<あいう\>` は `あいう漢字` に一致しない」は**誤り**だった。境界は文字の
  種類が変わるところなので、ひらがな → 漢字も境界であり、`あいう漢字` の `あいう` は `\<あいう\>` に
  一致する（`probe4.py` の `star-mixed-scripts`: `あいう漢字 x あいう漢字` の `*` が `\<あいう\>` で
  19 桁目＝2 つめの `あいう` に着く。`bound-kana-kanji-start` は `\<漢字` が同じ境界で始まることを示す）。
  規則そのもの（`iskeyword` ではなく `vim_character_class` の表で切る）は正しい。fixture
  `search-pattern-boundary-kana-kanji` / `-kanji-start` と `--vim-search` の unit がこの形で守る。
- **決定 1 の追記（取消が捨てるもの）**: 検索の入力行の取消は、保留中のオペレータと回数と組み立て中の
  `.` の鍵も捨てる（`probe.py` の `cancel-d-search-esc`: `d/<Esc>` のあとの `x` が本文を消すので、
  オペレータは残っていない）。捨てる範囲は engine の純関数 `vim_cancelled_input` が決め、controller は
  それを呼ぶだけにした。VISUAL は VISUAL のまま残る。
- **決定 4 の追記（区切りと未対応の `\`）**: offset の拒否は「打たれた向きの区切りの文字」で見る
  （`/` の検索では `/`、`?` の検索では `?`）。したがって前向きの検索では `?` はただの文字である。
  逆に、後ろ向きの検索で `?` そのものを探すことはこの部分集合ではできない（`\?` は量指定子として
  拒否される）。未対応の `\` は英数字を既定で拒否するので、`\r` `\n` `\t` も拒否になる（Vim は
  `alpha\r$` を E486 にするので「見つからない」ことは同じで、報せの文言だけが違う）。
  **（2026-11-13・#1605 の追補で直した: `\t` `\r` `\e` `\b` は 1 文字に当たり、拒否は `\n` だけ。）**
- **決定 4 の純関数の形**: 引数 4 つの上限（CPP-012）と「解析の失敗」と「見つからない」を
  分けて報せるために、決定 4 が書いた 1 本を 2 本にした。`VimPattern::parse(pattern, 打たれた向き)`
  が `std::expected<VimPattern, VimPatternFailure>` を返し、`vim_search(text, from, pattern, 向き)`
  が 1 回ぶんの `optional<VimSearchHit>{位置, 折り返したか}` を返す。回数は呼ぶ側が着いた位置から
  もう一度呼んで数える（`3/x` と `/x` ＋ `3n` が同じ答えになるのはこれ・実測）。
- **決定 5 の追記（報せの運び方）**: 折り返しの報せは効果（移動・削除）と同時に出るので、`VimEffect`
  の選択肢にはできない。1 打鍵の結果 `VimStep` に `notice` を添え、controller が `command_message` へ
  写す（Ex の結果と同じ 1 本）。文言は `vim_search_message` 1 か所だけが持つ。
- **鍵から動作への分岐（CPP-012）**: `VimAction` が 54 個になり、NORMAL と VISUAL の分岐を 1 つの
  `switch` に書くと関数長 60 行（T8）で落ちる（48 個・7 群の時点でちょうど 60 行だった）。CPP-012 の
  とおり「動作 → 大分類」を `constexpr` の表にし、NORMAL と VISUAL は `VimActionGroup` を網羅する
  `switch` で写す。分類が増えたら両方の `switch` がコンパイルで落ちる。表そのものの欠落と重複は
  `static_assert` が落とす（動作の個数 `vim_action_count` を末尾の値から導き、表の大きさをそれに
  固定して、0 以上 個数未満のどの値も表にちょうど 1 行あることを `constexpr` の関数で確かめる。
  動作を 1 つ足して行を足さない形で実際にコンパイルが落ちることを確認した）。
- **fixture にできなかった候補**: 空入力の Backspace の取消（`2l` `/<BS>` `x`）は、命令の切れ目で
  区切った形と 1 回の `:normal!` の形で答えが違うので `add-fixtures.py` が機械的に拒否した（候補 136 件
  のうち 1 件）。取消は `--vim-search` の unit が守る。

## 補足（2026-10-28・Issue #961）

- **決定 3 の追補（`*` `#` の語と文字列）**: `*` `#` が拾う本文は Vim の `find_ident_under_cursor`
  （`FIND_IDENT | FIND_STRING`）と同じ 2 周で決める。(1) キャレットの下か同じ行の後ろの最初の語
  （`vim_word_at`・文字の種類が 2 以上＝`w` の語の文字・表は `vim_character_class` の 1 つ）、
  (2) 無ければキャレットの下か後ろの最初の空白でない文字から同じ種類の連なりの先頭まで戻り、
  空白の手前までの文字列（`vim_string_at`）、(3) どちらも無ければ E348。語は `\<…\>` で囲み、
  文字列は囲まない。どちらも Vim の `nv_ident` と同じく `/ . * ~ [ ^ $ \` を（`#` は `?` も）
  `\` で escape してからパターンにする（`)` だけの行の `*` は折り返して自分に当たり、失敗しない）。
- **語の表の訂正**: 全角の記号の行を Vim の `utf_class_tab` と同じ 4 行（U+FF01〜FF0F・FF1A〜FF20・
  FF3B〜FF40・FF5B〜FF65）に直した。全角の数字 `１２` は語の文字で、`［` は記号である（実測）。
  表は 1 つなので `w` `b` `e` とテキストオブジェクトと `\<` `\>` も同じく Vim に揃う。
- **決定 4 の追記の訂正（区切りの `\`）**: `\` の付いた打たれた向きの区切りは文字そのもの
  （Vim の `skip_regexp`）。後ろ向きの検索の `\?` は量指定子ではなく `?` を探す（`#` の文字列が
  `?` を escape するため）。前向きの `\?` は従来どおり量指定子として拒否する。
- **fixture**: `star-multibyte-` の 17 件（全角の数字・かな・漢字・前が空白・`#`・文字列の
  fallback と折り返し・語が文字列に勝つ形・`_`・escape・`#` の `?`・`w` の全角の数字と括弧）。
  E348 は Vim の `-es` がエラーで終わるので fixture にできず、`--vim-search` の unit が守る。

## 強制

- 網羅性: **active**（`VimKey` の visit・`CommandInput` の visit・方向と原子の種類の switch）
- 照合器の正しさ・未対応構文の拒否・折り返し・`n` / `N`・再生: **active**（照合器の対象 unit と oracle fixture・CNF-010）
- ロケール・OS を core に入れないこと: **active**（`eng/symbols.py`。照合器が `__std_*` を出したら時刻・OS・スレッドに触れないことを確かめてから allowlist に足す）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: T2 の検索の中心（`/ ? n N * #`）が d/c/y・VISUAL・`.` と組み合わさる。入力行は Ex と 1 本なので描画・編集・IME の規則が増えない。
失うもの・残る穴: 正規表現は部分集合で、未対応の構文は拒否される（受理される部分集合の中では Vim と同じ答え、外では明示の失敗）。`:s` `:g`・検索履歴・offset・`\c` などは後続。多バイト文字の `.` は 1 code point に一致させ、結合文字は未測。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `std::regex` | ロケール依存のシンボルが core へ出る（ADR 0022 と同じ理由）。Vim の方言とも一致しない |
| 検索を controller で本文に対して行い、結果の位置だけ engine に渡す | オペレータ保留・回数・VISUAL・`.` の記録がすべて engine 側にあるので、二重の経路になる |
| 完全な Vim 正規表現 | 「難所」であり、まず T2 で使う部分集合を閉じた失敗つきで入れ、未対応は拒否する |
| `CommandLine` に prompt 種別を持たせて流用 | 補完と `ThemeCatalog` を検索が抱えることになる。編集の純関数だけを共用し、型は分ける |
| 未対応の構文をリテラルとして照合する | Vim と違う答えを黙って返す。拒否してメッセージを出すほうが正直 |

## 追補（2026-10-29・#1041）: `ignorecase` と `smartcase`

1. **大小の旗は照合器の引数 1 つ。** `VimPatternCase{sensitive, ignore, smart}` を `VimPattern::parse(pattern, separator, letter_case)` が受け、`\c` `\C` と合わせて「畳むか」を parse の 1 回で決めて値に持つ。旗をパターンの文字列に埋め込まない。`\c` `\C` は旗より強く、両方あれば `\c` が勝つ（`/\C\cabc` は畳む・実測）。`\c` `\C` は未対応の構文から外す（`VimPatternFailure::ignore_case` を消す）。
2. **smart の「大文字がある」** は Vim の pat_has_uppercase と同じく `\` とそれに続く 1 文字を数えない（`\S` `\C` は数えない）。`[B]` の中の文字は数える。ASCII の外の大文字（`É`）も数える（実測）。
3. **畳み方。** literal は `vim_folded`（大文字にしてから小文字・`ς` `σ` `Σ` が同じ・トルコ語の `İ` `ı` は畳まない・Vim の utf_fold）で比べる。`[...]` は文字か畳んだ形かその大文字が区間に入れば成員で、`[^a]` は `A` を外す（実測）。`\d \w \s` は畳まない（Vim の文字クラスと同じ）。区間の側にだけ別の綴りがある組（`[ς]` と `σ`）は当たらない（区間を歩かない限界）。畳むパターンは英字か ASCII の外を含めば `literal_bytes` を持たない（#583 の行の絞り込みは効かない）。
4. **呼び手。** 設定 `ignorecase` `smartcase` から `search_case(settings)` が旗を作り（`smartcase` は `ignorecase` がオンのときだけ）、`/` `?` の Enter・incsearch の preview・`hlsearch` の当たりが同じ旗を使う。`*` `#` は `ignorecase` だけを見て `smartcase` を見ず（`VimSmartcase::ignored`）、その後の `n` `N` と空のパターンの `/<CR>` も同じ扱いを引き継ぐ（Vim の no_scs・`*n` で実測）。
5. **設定が変わったら直前の検索を解き直す。** `/abc` の後の `:set ic` で `n` は大小を畳む（実測）。`EditorState::with_settings` が旗の変わったときだけ `last_search` を `recased` する 1 か所で、engine と描画は解き直した値を読む。
6. **契約。** fixture `search-case-` 30 件（oracle が `:normal!` の中の `:set ic<CR>` をそのまま測る・前置きの欄は足さない）・`--vim-search` の照合器の試験・`--vim-search-highlight` の当たりの数と preview・`--ex-settings` の `:set ic` `scs` の形。

## 追補（2026-10-30・#1138）: 結合文字の後ろで終わる当たり

1. **一致の終わりが Vim の 1 文字の途中なら一致にしない。** 本文の側の終わりの位置のコードポイントが幅 0（`DisplayWidth::zero`・ADR 0053 と同じ表・`vim_character_continues`）で行頭でなければ、その当たりを捨てて次の候補を探す。`/e` は `e` + U+0301 の基底に当たらず、パターンが結合文字を含む `/é` は今のまま当たる。大小を畳んでも同じ。照合器の `matched` の反復の無い道と `VimPatternEvaluation` の受理の 2 か所で同じ関数 `VimPattern::ends_a_character` を呼ぶので、`/` `?`・`n` `N`・`*` `#`・incsearch・`hlsearch` の塗りが同時に揃う。ASCII のバイトでは表を引かない。
2. **見るのは終わりだけ（Vim 9.1 の既定の NFA の `NFA_MATCH` の検査）。** 依頼の時点の案（regexp_bt.c の `EXACTLY` の「literal の直後」の検査）は oracle と合わない: `/e.` は `x éy` の `é` に当たる（`.` が結合文字を食べて終わりが文字の境に来る・実測 (1,3)）。fixture `search-composing-` 16 件（反復の道の `/e*` `/ee*` を単独で固定する 4 件を含む・`\+` は今どおり閉じた失敗なので `/ee*` で測る）。
3. **対応しないもの。** `\Z`（結合文字を無視）と `\%C` は今どおり閉じた失敗。当たりの始まりが結合文字の上に来る形（`/.y` が `x éy` で Vim は `é` の先頭 (1,3)・Nib は U+0301 の上 (1,4)）と、大小を畳んだときの結合文字を含むパターン（`:set ic` の `/é` は Vim では `É` に当たらない）は残差で、この追補は扱わない。
7. **結合文字を伴う文字は大小を畳まない（2026-10-30・#1147）。** パターンの literal の結合文字（幅 0・ADR 0053 と同じ表）と、その直前の繰り返しの無い literal の基底は、`ignorecase` でも `\c` でも畳まずに比べる（原子の印 `exact` を parse の終わりの 1 か所で付け、比べるのは `consumes` の 1 か所）。Vim 9.1 の既定の NFA は基底と結合文字の列を 1 つの原子 `NFA_COMPOSING` にして `mc == sta->c` で比べるので、`:set ic` の `/é`（e + U+0301）は `É`（E + U+0301）に当たらず（当たりが無ければ E486）、`/É` も `é` に当たらず、反復のある `/x*é` も同じ（反復の道も `consumes` を通る）。前後の英字（`/xé` の `x`・`/éx` の `x`）・`[e]` の集合（`/[e]` + U+0301 は `É` に当たる）・合成済みの `é`（U+00E9）は今どおり畳む（実測・`/xé` は `x XÉ Xé xé` で `Xé` の (1,8)）。「繰り返しの無い」の条件（`/e*` + U+0301 の `e` は印を付けず畳む）は Vim の `nfa_regatom` の原子の切り方の読みで、実測とは合わない: Vim 9.1 は `:set ic` の `/e*` + U+0301 を `x É é` でも E486 にし（`noic` では結合文字だけの原子が基底を問わず `É` の (1,3) に当たる）、Nib は条件を保っても消しても当たる。この形は #1146（結合文字だけの原子・当たりの始まり）と同じ残差として扱い、fixture と試験に入れない。`CaseMappingTable` は変えない。fixture `search-composing-ic-` 13 件と `--vim-search` の照合器の試験。

## 追補（2026-10-30・#1146）: 当たりの始まりと `.` と集合と `\<` `\>`

1. **一致の始まりは Vim の 1 文字の先頭だけ。** 照合器は結合文字（幅 0・ADR 0053 の表）の上の位置からは一致を試さない（Vim の regexec の列は `mb_ptr2len`＝`utfc_ptr2len` で 1 文字ずつ進む）。判定は追補（#1138）の終わりの検査と同じ `VimPattern::ends_a_character` の 1 つで、反復の無い道（`matched`）と `VimPatternEvaluation` の始まりの候補の 2 か所が呼ぶ。
2. **`.` と集合（`[...]` と `\d \w \s` とその否定）は 1 文字の残りの結合文字まで食べ、literal は code point 1 つだけを食べる。** Vim の NFA の `clen` は `utfc_ptr2len` で、literal の一致だけが `utf_ptr2len` に縮める。始まりの規則だけでは `/.y` が `x éy` で当たらなくなるので、この 2 つは対で要る（実測: `/.y` `/[a-z]y` `/\Sy` は `é` の先頭 (1,3)・`/[e]` は `x é e` の `é` (1,3)・`/e` は追補（#1138）のまま `é` に当たらない・`/e.` は `.` が結合文字を食べて当たる）。食べる位置は `VimPattern::eaten` の 1 本。反復の道は「原子へ向かって結合文字を食べている途中」の候補を同じ優先順の列に残す（列の番号を原子の数 + 1 だけずらした印・Vim の `add_off` と同じ形）。**改め（2026-11-04・#1146 と #1147 の衝突）:** 閉じ大括弧 `]` の直後に続くパターンの結合文字は、集合の後ろの原子にならず `]` と 1 文字として読み飛ばされる（Vim 9.1 の `nfa_regatom` は集合の終わりで `MB_PTR_ADV(regparse)`＝`utfc_ptr2len` で進む）。この文の「集合は残りの結合文字まで食べる」だけでは `/[e]` + U+0301 が `é` にも `É` にも当たらなくなり、#1147 の決定 7（`:set ic` の `/[e]` + U+0301 は `É` に当たる）と実測に合わないので、決定 7 はそのままにしてこちらを改める（実測・`matchstrpos` の NFA: `[e]` + U+0301 は `x É é` の ic で É のバイト 2〜5・`x e y` で `e` の 2〜3・`[e]` + U+0302 + `y` は `x éy` で 2〜6・`[e]` + U+0301 + `*` は `x e y` で空の 0〜0（`*` は集合に掛かる）。`\S` `\a` `\w` の後ろの結合文字は読み飛ばされず、`\S` + U+0301 + `y` は `x éy` で当たらない＝Nib と同じ）。読み飛ばすのは `parsed_set` の終わりの 1 か所（`vim_character_end`）。契約に fixture `search-composing-start-set-mark-plain` `-other-mark-y` `-mark-star-y` の 3 件（決定 4 の 15 件と合わせて 18 件）と `--vim-search` の照合器の試験 1 件を足す。`.` の直後の結合文字は Vim では `.` が消えて結合文字だけの原子になり（同じ関数の `c == Magic('.') && utf_iscomposing(peekchr())`）、`/.` + U+0301 + `y` は `x éy` で 2〜6 に当たるが、Nib は `.` が結合文字まで食べた後の literal が合わず当たらない。これは決定 5 の結合文字だけの原子と同じ残差として扱う。`/.` + U+0301 + `y`（と `/.` + U+0301 単独）は #1146 より前の main では `.` が code point 1 つを食べた後に literal の U+0301 が当たって偶然に Vim と一致していた（`x éy` で (1,3)）が、#1146 の後は E486 になる（決定 5 と同じ残差・残差を解くときは決定 5 と同じ Issue にまとめる）。
3. **`\<` `\>` は 1 文字の境でだけ真で、手前の文字の種類は手前の 1 文字の先頭（基底）で見る。** Vim の `reg_prev_class` は `mb_head_off` で結合文字を越える（実測: `x -́bb` の `/\<.b` は `b` (1,6)・`ab -́ab` の `*` は 2 つめの `ab` (1,7)・`-́ ab c` の `/\>` は `ab` の後ろ (1,7)・`x -️amma`（`-` + U+FE0F）の `/\<amma` は `a` (1,7)・`x ⚠️bb` の `/\<.b` は `.` が `⚠️` を食べて (1,3)）。
4. **契約。** fixture `search-composing-start-` 15 件（`/.y` と `?` `n` `:set ic`・集合 `[a-z]` `[e]`・`\S`・`\<` `\>`・`*` の語・U+FE0F・反復の道 `/[a-z]*y` `/\s.*y`）は修正前（src だけを origin/main に戻した binary で今の 15 件を再生）に 9 件が落ちた。`--vim-search` の照合器の試験 5 件。
5. **残差。** 結合文字だけの原子（パターンの結合文字の前に基底が無い形・`/e*́`）は Vim では基底を問わず結合文字を持つ 1 文字に当たる（`noic` の `x É é` で `É` (1,3)・`ic` では E486・#1147 の席の実測）。Nib はこの原子を code point 1 つの literal として扱うので、始まりの規則の後は `noic` で `é` (1,7)・`ic` で `É` (1,3) に当たり、どちらも Vim と合わない（修正前の `noic` は結合文字の上から始まってカーソルが `É` の先頭に寄り、偶然に一致していた）。判断 A のとおり特別な規則は足さず、fixture にしない。先頭が結合文字のパターン（`/́` 単独・`/[́]`）も同じ残差で、修正前は U+0301 の上から当たって（カーソルは `é` の先頭に寄る）偶然に一致していたが、修正後は始まりが境に限られるので E486 になる（Vim は `:h patterns-composing` のとおり、その結合文字を持つ文字に当たる）。格子と巨大な行の塗りの窓（#556）は一致の長さの上限を原子ごとに code point 1 つで見積もるので、結合文字の多い本文で余裕の外から始まる当たりは塗られないことがある（ADR 0139 の窓の境の限界の内）。

## 追補（2026-11-10・#1344）: オペレータの範囲が空になる検索

1. **空の範囲も検索以外のオペレータと同じ 1 本の道を通る。** `d` `c` `y` の後ろの `n` `N` `*` `#` `/pat<CR>` `?pat<CR>` が始点へ戻った（当たりが 1 つだけ・回数が周の倍数・当たりの上で同じ語の `*`）とき、`search_operated` は先に返らず、`performed` へ空の文字単位の範囲を渡す。1 桁目の `d0` `c0` `y0`・同じ位置の `` d`a `` と同じ規則で、第 2 の規則を書かない。Vim の `do_pending_operator`（ops.c）は空の範囲を `oap->empty` にするだけで、`cpo` に `E` が無い既定では失敗にしない（`empty_region_error` が偽）。
2. **オペレータごとの答え（Vim 9.1 の実測）。** `d`: `op_delete` が `oap->empty` で何も書かずに返る。本文とレジスタは変わらず（無名と `"-` と `"1` は実測・`jdd/foo<CR>d1n"1p` は `"1` の `zz` を貼る・#1369）、失敗ではないので後ろの鍵が効き（`d1nx`）、`.` に残る（`d1n0.` は行頭から次の当たりまで消す）。`y`: `op_yank` は空の文字単位を書く（無名と `"0`・`"a` の名指しも空になる・キャレットは始点）。`c`: `op_change` は `op_delete` の空の返りの後に INSERT へ入る（レジスタは変えない・`.` と `u` も普通の変更と同じ）。`>` `<`: 範囲が空でも行単位の命令なのでキャレットの行を 1 段ずらす（`abc foo def` の `/foo<CR>>1n` は行頭に Tab を足し、`\tabc foo def` の `<1n` は外す・`abc foo def\nxyz` の `/foo<CR>>1nj.` は `\t\tabc foo def\n\txyz`・実測・fixture は #1369 の `prefix-escape-search-empty-shift-` の 3 件）。折り返しの報せは当たりを探した結果のまま出す。
3. **修正前の形。** 空の範囲を「何もしない」で返していたので、`y` はレジスタを書かず、`c` は INSERT に入らずに後ろの `X` が NORMAL の `X` として手前の 1 文字を消した（`abc foo def` の `c1nX<Esc>` が `abcfoo def`）。
4. **`[` `]` の後ろの Esc は打ち消し。** 失敗にせず、回数・保留中のオペレータ・待ちを消すだけ（`[<Esc>x` `]<Esc>x` `3[<Esc>x` `d[<Esc>x` は `x` が効く・`z<Esc>` `m<Esc>` と同じ・実測）。Esc 以外の鍵は今のとおり失敗（族は #1342）。
5. **契約。** fixture `search-operator-empty-` 29 件（`y` 10・`c` 10・`d` 5・`[` `]` の Esc 4）は修正前の再生器で 24 件が落ちる。`--vim-text-objects` に `[` `]` の Esc の試験。#1334 の再レビューの検索の 566 件の組（`D:/NeNeNib/scripts/20261110/review-1334-2/srch.json`）は違いが 0 件になった（`[[` `]]` の族の 3 件は #1342 の残り）。

## 追補（2026-11-12・#1506）: 基底の無い結合文字の原子

1. **基底の literal の直後でない結合文字は「結合文字だけの原子」になる。** パターンの結合文字（幅 0・ADR 0053 と同じ表）が、繰り返しの無い literal（基底か、基底に続く結合文字）の直後に無いとき（パターンの先頭・`*` の後ろ・`^` `\<` `\>` の後ろ）、照合器はそれを原子の種類 `VimPatternAtomKind::composing` にし、続く結合文字も同じ原子に足す。`.` の直後の結合文字は `.` を消して同じ原子にする。Vim 9.1 の `nfa_regatom` の既定の枝（`utf_iscomposing(c)` で `NFA_COMPOSING` を作る）と、`c == Magic('.') && utf_iscomposing(peekchr())` で `.` を捨てる枝のとおり。基底の文字と直後の結合文字の列は今どおり literal の列で比べる（追補 #1138 / #1147）。読むのは `parsed_item` の 1 か所（`follows_base` と `appended_mark`）。
2. **当たり方は Vim の `NFA_COMPOSING` の基底を無視する枝。** 原子は位置の code point（基底でも、1 文字の途中の結合文字でもよい）を比べずに飛ばし、同じ Vim の 1 文字に続く結合文字を `MAX_MCO`（6）個まで集めて、原子の結合文字が全部その中にあれば当たる（順は問わない・本文の側に余る結合文字は問わない・大小は `ignorecase` でも `\c` でも畳まない）。当たれば 1 文字を丸ごと食べる（`.` と集合と同じ `eaten`・`whole_character` が真）。当たりの始まりは追補（#1146）のとおり 1 文字の頭なので、カーソル・オペレータの範囲・`x` で消える本文・`hlsearch` と incsearch の塗りは、その結合文字を持つ 1 文字の全体になる。判定は `VimPattern::carries_marks` の 1 本で、`consumes` が `matched` と `VimPatternEvaluation` の両方から呼ぶ（読むのは 1 文字の中だけで、行を写さない）。
3. **実測（Vim 9.1・`-es -u NONE`）。** `x が ab` の `/゙` は `が` の頭 (1,3)・`/゙<CR>x` は `が` を消してレジスタ `が`・`d/゙` は `x ` を消す・`n` `N` `?` は同じ原子・`/́` は `é` (1,3)・`:set ic` と `/\ć` も `É` (1,3)・`/́̂` は `ế`（e + U+0302 + U+0301）に当たる・`/.́y` と `/.́` は `é` (1,3)・`/^゙` は行頭の `が`・`/゙*b` は `ががb` の頭・`/\<́` は語の頭の `é`・`/́b` は `éb` の `é`・行頭の孤立した U+0301 は自分が基底なので、続く結合文字が無ければ当たらない（`́a` は飛ばし、`́́a` は (2,1)）・7 つめの結合文字は見ない（`MAX_MCO`）。`/a゙` と `/゙゚`（`が` に `゚` が無い）と `/か` は今どおり E486（oracle は E486 の鍵を測れないので照合器の試験で守る）。
4. **追補（#1146）の決定 5 と、`.` の直後の結合文字の残差を解く。** `noic` の `/e*́` は `x É é` の `É` (1,3)（実測と一致）・`/.́y` は `x éy` の (1,3)（#1146 の後に E486 になっていた形）。`:set ic` の `/e*́` は Vim 9.1 では E486 だが、これは `nfa_regmatch` の literal の枝が同じ位置の後ろの状態と共有する `clen` を `utf_ptr2len` に縮めて、結合文字の原子が集める列を空にするため（source の読み・Vim の不具合とみる）。Nib はこの形を写さず `É` (1,3) に当たり、残差として fixture にしない。
5. **通常モードの検索と置換（ADR 0174）と `*` `#` の語は今のまま。** 同じ照合器なので、逃がさなければ通常モードの Ctrl+F / Ctrl+H に結合文字だけを打つと、その結合文字を持つ 1 文字の全体に当たるようになる（性質の試験 `--substitute-property` の模型と 2 件で食い違った）。`literal_pattern` は語の先頭の結合文字（幅 0）を `\` で逃がし、`\` の付いた結合文字は今どおり code point 1 つの literal になる（`parsed_escape`）ので、通常モードと `*` `#` の当たり方は #1506 の前と同じ。通常モードの当たり方を Windows の標準（D82・メモ帳）に揃えるかは確かめておらず、設計席の判断を待つ。
6. **契約。** fixture `search-combining-lead-` 29 件（Issue の表の 10 件と対照 2 件・`n` `N`・`d` `x`・`\c` と `:set ic`・2 つの結合文字の順・`.`・`^`・`*`・`\<`・語の `*`・余る結合文字・孤立した結合文字・7 つめ）。修正前の照合器（src の 6 ファイルを origin/main に戻した binary）で 26 件が落ちる（対照の 3 件 `only-cursor` `glued-control` `word-star-control` だけが通る）。`--vim-search` の照合器の試験 1 件（当たりの範囲）。格子と巨大な行の塗りの窓（#556）の一致の長さの上限は、この原子も `.` と同じく code point 1 つで見積もる（決定 5 の限界の内）。

## 追補（2026-11-13・#1606）: 行の内容の終わりの幅の無い当たり

1. **当たりが行の内容の終わり（Vim の NUL）なら、着く位置は最後の文字の上。** `/$` `?$` `/\>` `/a\{0}$` のように当たりの始まりが行の内容の終わりのとき、NORMAL とオペレータ待ちでは最後の文字の上（Vim の 1 文字の先頭）に寄せ、その寄せた位置がオペレータの範囲の端になる。Vim の `normal_search` は `do_search` の後に必ず `check_cursor()` を呼び、`check_cursor_col` は VISUAL（`selection` が `old` でない）と `restart_edit`（INSERT の `Ctrl-o`）のときだけ NUL を許す。exclusive の範囲は端の文字を含めないので、`abcd` の `d/$` は `d` を残してレジスタ `abc`・`?$<CR>yn` は空を yank する。VISUAL（`v/$d` は改行まで消す）と INSERT の `Ctrl-o` の命令の間（`0i<C-o>d/$` は行の全体）は寄せない。規則は `VimStepSearch.cpp` の `searched_landing` の 1 か所で、`/` `?` `n` `N` `*` `#` とオペレータの後ろの検索が同じ道を通る（NORMAL の寄せは既存の `rested_in`、`Ctrl-o` の判定は `vim_insert_paused`）。
2. **起点の行の前向きでは、NUL の当たりを最後の文字の桁で比べる。** Vim の `searchit` は起点の行の前向きの当たりを `matchpos.col - (ptr[matchpos.col] == NUL) < start_pos.col + extra_col`（`extra_col` は起点の 1 文字の長さ・`mb_ptr2len` は結合文字を含む）で捨てる（コメントは「otherwise "/$" will get stuck on end of line」）。起点が最後の文字の上か行の内容の終わりなら、その行の NUL の当たりは起点の行の答えにしない（`VimSearch.cpp` の `first_match_past`）。後ろ向きと、起点でない行（折り返して戻った起点の行を含む）は今のまま。照合器（`VimPattern*`）の答えは変えない。
3. **実測（Vim 9.1・`-es -u NONE`）。** `abcd` / `xy` の `/$<CR>n` は (2,2)・`ab` / `xy` / `zz` の `/$<CR>nn` は (3,2)・`あいう` の `d/$` はレジスタ `あい`・`abé`（e + U+0301）の `d/$` はレジスタ `ab`・`2d/$` は 2 行目の最後の文字の手前まで・`abc` の `$/x*` は折り返して (1,1)・`:s/$/X/` `:%s/$/;/` `:s/x*/-/g` は前から一致していた。
4. **契約。** fixture `search-zero-width-` 44 件（Issue の表の 8 件・`/^` `/\>` `/\<` `/x*` `/a\=`・空の行・最後の行・回数・折り返し・`d` `c` `y` `dn` `dN` `.`・VISUAL・INSERT の `Ctrl-o`・`:s` 4 件）。修正前の再生器で 20 件が落ちる。incsearch の preview の当たりの位置は同じ `vim_find_match` を通るので決定 2 が効き、塗りの見た目は変えない（preview の Vim との比較は確かめていない）。

## 追補（2026-11-13・#1605）: 略記 `\t` `\r` `\e` `\b` と集合の中の `\`

1. **決定 4 の追記の「`\r` `\n` `\t` も拒否になる（見つからないことは同じ）」を直す。** Vim 9.1 は `\t` を Tab に当てる（`ab<Tab>cd` の `/\t` は (1,3)・`:%s/\t/  /g` は本文を変える）ので、拒否は「見つからない」と同じでなかった。`\r` も literal CR に当たる。
2. **規則（Vim 9.1.0004 の source と実測）。** 集合の外: `peekchr` は `\` の次が `META_flags` に無く `REGEXP_ABBR`（`nrtebdoxuU`）にあれば `backslash_trans` で 1 文字にする。`META_flags` に無いのは `b e r t` だけなので、`\t` は Tab（U+0009）・`\r` は CR（U+000D）・`\e` は Esc（U+001B）・`\b` は後退（U+0008）の literal の原子になる。`\n` は `META_flags` にある（行またぎ・ADR 0184 の段 5）ので今どおり断る。集合の中（`nfa_regatom` の `[` の枝）: `\` の次が `REGEXP_INRANGE`（`]^-n\`）か `REGEXP_ABBR` なら組で読む。`\]` `\^` `\-` `\\` はその文字、`\t` `\r` `\e` `\b` は外と同じ 1 文字（区間の端にもなる・`[\t-z]`）、`\n` は行またぎで断る（`[\n]` も `[^a\n]` も閉じた失敗 `escape`）。`\d` `\o` `\x` `\u` `\U` は `coll_get_char` が数を読み、数が続かなければ `\` とその文字の 2 つの成員（`[\d]` は `\` と `d`・`[\x]` は `\` と `x`・実測）。数が続く形（`[\d97]` `[\x61]`）は文字の番号で、Nib はまだ読まないので `escape` で断る。ほかの `\`（`[\a]`）は `\` そのものの成員（実測）。閉じの判定も同じ組で読むので、`[a\]b` は閉じない集合で `[` は文字（`x[a]bc` の (1,2)・実測）。
3. **CRLF の文書。** 行末の改行の CR は行の内容に入らない（`line_text` と `VimSearchWalk`・ADR 0036 の決定 2）ので `\r` は当たらず、行の途中の CR にだけ当たる（`ab<CR>cd<CR><LF>` の `/\r` は (1,3)・`ab<CR><LF>` は E486・`:%s/\r/-/g` は途中の CR だけ・Vim 9.1 の `ff=dos` の実測）。CRLF は fixture にできないので unit で守る。
4. **読む所は parse の 2 か所だけ。** 外は `parsed_escape`（表 `abbreviations` を `escape_failure` の前に引く）、中は `parsed_set` の成員の読み `member_at`。道の選び方（ADR 0184 の決定 1）と評価器は変えない（略記は literal の原子なので、量の無いパターンは今の `sequence` の道のまま）。検索 `/ ? n N`・`:s`・incsearch・hlsearch は同じ照合器で同じ答え。置き換えの側の `\t` `\r` `\n`（ADR 0168）は変えない。
5. **通常モードの Ctrl+F / Ctrl+H は今のまま。** `literal_pattern` が `\` を逃がすので、打った `\t` はバックスラッシュと `t` の 2 文字に当たる（Windows の標準・D82）。打った Tab の文字は今どおり Tab に当たる。
6. **契約。** fixture `pattern-escape-` 28 件（`:%s/\t/  /g`・`[\t ]\+`・`d/\t`・`/ ? n` の `\t`・`[\t]` `[^\t ]` `[\t-z]`・`\t\+` `\t\{2}` `\t\{2,}` `\t*`・`/ ?` の `\r` と `[\r]` と `:%s/\r/-/g` の literal CR・`[\e\b\r\\]`・`[\]\^\-]`・閉じない `[a\]b`・置き換えの `\t`・`.`・`smartcase`・`[\d]` `[\x]`・VISUAL）。本文に Esc と後退を持つ形は oracle の `literal` が C++ に書けない（`eng/vim-oracle.py` を変えると折り返す窓の生成物が記録する共有の digest が変わり、全件の再測定になる）ので、`\e` `\b` の当たりは `--vim-search` の試験と性質の試験で守る。修正前の照合器（main `e43e93e8` の binary に fixture を足したもの）では、最初に足した 28 件のうち 26 件が落ちた（通るのは今の読みと同じ `[\d]` `[\x]` の 2 件）。その後 Esc と後退の本文の 2 件を外し、3 件（`[\e\b\r\\]` の本文を CR に替えた件・`\t\{2,}`・`?\r`）に替えた。この 3 件は修正前の binary では流していない。`--vim-search` に略記と集合の中の `\`・`\n` の拒否・数の続く `[\d97]` の拒否・CRLF の行末・通常モードの逃がしの試験、`--substitute-property` に略記の pattern の置換が「略記をその文字そのもので書いた pattern」の当たりの模型と同じになる家族（本文に Tab・CR・Esc・後退・seed 129115・300 件）。

## 追補（2026-11-13・#1617）: `gn` `gN` と表に無い `g` の 2 鍵目

1. **表に無い `g` の 2 鍵目は失敗して後ろの鍵を打ち切る。** Vim の `nv_g_cmd` の default は `clearopbeep`（保留中のオペレータと回数を消して鳴らす・`:normal!` とマクロでは後ろの鍵を捨てる）。今までは `g` を打ち消すだけで失敗にせず、`ab ab` の `/b<CR>cgnX<Esc>` は `c` を失った後ろの `X` が NORMAL の `X` として `a` を消した（`b ab`・Vim は `aX ab`）。`g_prefix_step` の表に無い鍵は `escaped_or_refused`（Esc は鳴らさずに打ち消すだけで `g<Esc>G` の `G` は効く）。実測: `abc def` の `dgxx` `gxx` は本文を変えない・`vgxd` は VISUAL のまま。`z` の表（`z_prefix_step`）・`[` `]`（`bracket_step`）・テキストオブジェクトの表に無い鍵（`awaited_step`）は前から失敗していたので、穴は `g` の表だけ。これは ADR 0027 の決定 6（`g` の後の未対応の鍵は消費して待ち・回数・オペレータを解き、モード・選択・欲しい列・検索の覚えを保つ）を「失敗の印を立てる」へ追補する。解く物と保つ物は決定 6 のまま変えず、失敗の印が `:normal!`・マクロ・`.`・`map` の右辺の再生を打ち切る（1 鍵ずつ打つ対話では後ろの鍵は今どおり効く・ADR 0027 の「対話の続きと `normal!` の捨て方を混同しない」）。Vim が表示だけをする 6 鍵は決定 7 で表に足した。表に足さず失敗のまま残す鍵と理由は #1617 の `out/1617-fix/g-keys.md`（Vim に無い鍵の `default` のほか、`gp` `gP` `g*` `g#` `go` `` g` `` `g'` `gR` `gr` `gq` `gw` `g?` `g@` `g+` `g-` `gd` `gD` `gQ` `g<Tab>` と Select モードの `gh` `gH` `g<C-h>` `g<BS>`。どれも Nib に無い命令で本文かキャレットかモードを変えるので、後ろの鍵を予期しない状態で流さない）。
2. **`gn` `gN` は `g` の表の 2 行と検索の大分類の動作 2 つ**（`select_next_match` `select_previous_match`）。NORMAL・オペレータ待ち・VISUAL はどれも `search_action` から `VimStepSearch.cpp` の `matched_object` の 1 本へ来る。当たりを選ぶ範囲は `VimSearchObject.cpp` の `vim_search_object` の 1 本（ADR 0031 のテキストオブジェクトの範囲関数と同じ形）で、探すのは `vim_find_match` だけ（新しい探し方を書かない）。後ろ向きの `SEARCH_END` の起点の行は `vim_last_match_ending_by`（同じ区切りの走査で終わりが起点を越えない最後の当たり・決定 8）、当たりの終わりは `vim_match_from`（行の at 以降で最も左の一致を 1 回の照合で引く・決定 9）。どちらも `VimSearch.cpp` の同じ走査（`visit_line_matches` と照合器）の読む口で、新しい探し方ではない。
3. **規則（Vim 9.1.0004 の search.c `current_search`）。** `gn` は 1 回目に折り返さずに後ろ向きの `SEARCH_END`（起点の行では「終わり − 1 < 起点の桁」の当たりの最後・起点ちょうどの空の当たりも入る・起点が行頭なら起点の行を見ない・決定 8）で 1 つ手前の当たりを探し、2 回目にそこから前向きに回数ぶん（折り返す）。だからキャレットが当たりの中（最後の文字の上を含む）なら今の当たり、そうでなければ次の当たりになる。1 回目が無ければ本文の頭の手前（Vim の lnum 0・1 行目の 0 桁目の当たりも数える）から。`gN` は向きを逆にした同じ形で、1 回目が無ければ本文の末尾から。本文の頭から前向きに探した最初の当たりが空のパターン（`is_zero_width` の move が真の形・キャレットの位置の当たりでは決めない）は `SEARCH_END` を使わない。VISUAL では探し始めを 1 文字先（`gN` は手前・Vim の `incl` `decl`）にずらして anchor を動かさず、前向きで選択が逆向きなら 1 回目を飛ばしてキャレットを当たりの始まりに置く。NORMAL では anchor を当たりの始まり、キャレットを最後の文字（`gN` は始まり）に置く。種類はいつも文字単位で、行単位と矩形の VISUAL からも文字単位になる。欲しい列は動かさない（`current_search` は curswant を書かない・`0gnj` は 0 桁目へ）。検索の強調・覚えた向き・ジャンプの列は変えない（`SEARCH_KEEP`）。最後の検索が無ければ E35、パターンがどこにも当たらないか 2 回目が無ければ報せを出さずに失敗し、オペレータは消えて VISUAL は選択のまま残る（`clearopbeep`）。
4. **オペレータ待ちは VISUAL の範囲として効かせる。** Vim は `current_search` が VIsual を立て、`do_pending_operator` がその選択に効かせる（`is_VIsual`）。同じ道として、選んだ当たりを文字単位の選択に置いて VISUAL のオペレータの 1 本（`visual_operated` を `detail::visual_operator_applied` で開く）へ流し、VISUAL の鍵がオペレータで抜けるときと同じく覚え直す仮想桁（`operator_columns`・#1436・Vim の `oap->start_vcol` `end_vcol`）も `visual_operation_noted` と同じ `columns_noted` で置く。後で打つ VISUAL の `A` `I` はこの桁を読む（`abcdef` 3 行の `/e<CR>0ygn0vjAX<Esc>` は 2 行目が `aXbcdef`・#1617 のレビュー M2 の前は `Xabcdef`）。行の内容の終わりの当たり（`/$`）は改行まで届き（`dgn` は行をつなぐ）、最後の行では空の範囲（何も消さずレジスタも変えない）になる。回数は当たりを選ぶのに使い、`>` `<` では字下げの段数にもなる（`2>gn` は 2 段・Vim の `op_shift` の `cap->count1`）。抜けた選択は `gv` と Ex の範囲 `'<,'>` が読む前の選択になる（`dgngv` は消した所を選ぶ。`` `< `` `` `> `` の移動は ADR 0152 の `kept_unset` で前から Vim と違い、`gn` の外の違い）。小さな削除は `"-`（`"1` へは行かない）。
5. **`.` は鍵の列の記録（ADR 0030）のまま。** Vim の `prep_redo` は `gn` `gN` を VISUAL の大きさではなく鍵として積むので、`cgnX<Esc>` の `.` は次の当たりを選び直して同じ本文を打ち、当たりが尽きたら何もせずに失敗する。マクロの中も同じ。
6. **実測（Vim 9.1・`-es -u NONE`・約 150 形）と契約。** fixture `gn-` 87 件（Issue の形・キャレットが当たりの始まり / 中 / 最後の文字 / 後ろ・`gN`・回数と折り返しと巨大な回数・`d c y gU g~ >`・`.` と `"_` と `"a` と `"-`・当たりが無い・`/$` `/^` `/x*` `/^$` の幅の無い当たり・全角と結合文字・`ignorecase` `smartcase`・`*` `#` `?` `:s` の後・`u`・`gv`・欲しい列・マクロ・重なる当たり・VISUAL の広げ方と `o` の後と行単位と矩形・表に無い `g` の鍵）。修正前の再生器（`nib_vim_replay`）では、再生できる 86 件のうち 78 件が落ちた（レジスタつきのマクロの 1 件は再生器が流さない・通った 8 件は今の振る舞いと答えが偶然同じ形）。`ignorecase` `smartcase` は fixture の `settings` が C++ の表に載らないので、鍵の頭の `:set` で置く（`:set ignorecase smartcase` と 1 つに並べた形は Nib の再生で本文が変わらなかったので、2 つの `:set` に分けた・原因は確かめていない）。`--vim-search` に `vim_search_object` を直に問う試験。
7. **表示だけの `g` の鍵は何もしない成功（#1617 のレビュー M1）。** `ga` `g8` `g<C-g>` `g<`（情報を出すだけ）・`gV`（Select モードの印を変えるだけ）・`gs`（眠るだけ）は Vim では後ろの鍵が効く（`abc def` の `gax` は `bc def`）。決定 1 の失敗はこれを打ち切っていたので、`g` の表に動作 `show_information` `show_character_bytes` の 6 行を足し、新しい大分類 `display` で写す。NORMAL は回数とレジスタの指定を消すだけ、VISUAL は選択のまま（`vlgad` は `c def`）。オペレータ待ちは、Vim の `nv_g_cmd` が動かずに返った後の `do_pending_operator` と同じく、キャレットの上の空の範囲（exclusive）に効かせる（`dgax` は何も消さず、`cgaX<Esc>` は `Xabc def`、`ygaP` は空の yank、`>gax` は行を字下げ、`g~gax` は 0 桁目でだけ 1 文字を反転・`0` を 0 桁目で打ったのと同じ `performed` の道）。`8g8` は Vim が不正なバイト列を探して見つからずに鳴る形で、Nib の本文はいつも正しい UTF-8 なので失敗（VISUAL は残る）。`g<Down>` `g<Up>` は Vim の `nv_g_cmd` で `gj` `gk` と同じ case なので、特別な鍵の表 `g_special_bindings` に足した。`g<C-g>` は fixture にせず `--vim-search` の試験（生の 0x07 の鍵を `:normal!` の意味で流す）で守る。fixture の記法の表 `KEY_TABLE` に `<C-g>` を足すと `eng/vim-oracle.py` が変わり、折り返す窓の fixture（402 件）が記録する共有の digest が変わって全件の再測定になるため（決定 6 の `\e` `\b` と同じ理由）。残る違い: `cgaX<Esc>j.` は Vim では 2 行目が `X` になる（`.` の再生が行単位の変更になる・原因は確かめていない）が、Nib は `Xghi jkl`。
8. **幅のあるパターンの途中の空の当たり（#1617 のレビュー M3）。** `/x*` のように本文の頭の最初の当たりが空でない（`SEARCH_END` を使う）パターンでも、行の途中には空の当たりがある。Vim の `searchit` の後ろ向きの `SEARCH_END` は起点の行で「終わり − 1 < 起点の桁」（`endpos.col - 1 < start_pos.col + extra_col`・extra_col は 0）の当たりを数えるので、起点ちょうどの空の当たりも答えになり（終わり − 1 = 起点 − 1）、2 回目の前向きは起点の文字を越えた当たりから探す。答えが空の当たりなら pos は始まりのまま（`SEARCH_END` は空の当たりに効かない）。`backward_end_once` は起点の行を `vim_last_match_ending_by`（行頭からの区切りの走査で、終わりが起点を越えたら止める・Vim の後ろ向きのループと同じく、区切りの走査は行の内容の終わりで止まるので、手前に当たりがあれば内容の終わりの空の当たりは数えない）で引き、無ければ上の行から `vim_find_match` の後ろ向きで探す。回数の 2 つ目からは前の答えの pos（最後の文字）から同じ問いを繰り返し、折り返して最初の答えへ戻ったら残りの回数の周回を飛ばす。VISUAL の `dec_cursor`（キャレットを当たりの終わりの手前へ）は空の当たりでも 1 文字戻り、行頭なら前の行の内容の終わりへ行く。実測: `xab ab` の `/x*<CR>$dgn` は `ab ab`・`0lldgn` は `xabab`・`xab xab` の `0ldgN` は `xa xab`・`xx a xx` の `0lllldgn` は `xx a ` ほか 20 形（fixture `gn-empty-match-inside-wide-pattern-`）。
9. **費用（#1617 のレビュー M4）。** 1 回の `gn` は行を写さず、当たりを全部数えない。探すのは `vim_find_match`（`n` `N` と同じ 1 回ぶんの走査）と、後ろ向きの `SEARCH_END` の起点の行の `vim_last_match_ending_by`（同じ走査を 1 回）で、当たりの終わりは `vim_match_from` の 1 回の照合（行は `VimSearchWalk` の借用の view）、文字の境は `vim_character_after` / `vim_character_before` の窓で読む。1 回の `gn` `gN` は Vim と同じく行の走査 2 回ぶん（1 回目と 2 回目）。`gn` の 1 回目（後ろ向き・折り返さない）は、本文の最初の当たり（幅の判定で引いた物）より前に答えが無いと分かれば探さない（`vim_find_match` は折り返して本文の残りを歩くため）。費用は `nib_perf_probes` の `vim-gn-1mib-line-start` ほか 8 本（1 MiB の 1 行・同じキャレットの `n` `N` と並べる）。
10. **契約の追加。** fixture `gn-` を 87 → 147 件（表示だけの鍵 28・覚え直す桁 10・空の当たり 20・`g<Down>` `g<Up>` 2）。直す前の HEAD（`f2151f39`）の再生器では、足した 60 件のうち 44 件が落ちた。`--vim-search` に `vim_search_object` の空の当たりと `vim_match_from` `vim_last_match_ending_by` と `g<C-g>` の試験。
11. **`'wrapscan'`（#1561 と合わせて・#1617）。** Vim の `current_search` は 1 回目だけ `p_ws` を偽にし、2 回目は設定の値で探す。`VimSearchObjectRequest.wrapscan` に `VimState.wrapscan` を渡し、1 回目の問いは `VimMatchRequest.wrapscan` を偽、2 回目は設定の値にする。実測（Vim 9.1）: `x foo y` の `:set nows<CR>/foo<CR>$dgnx` は何も変えず後ろの `x` も捨てる（`ws` なら `x y`）・`x foo y foo` の `$hdgn` は `x foo y `・`x foo` の `0gNd` は何も変えない（`ws` なら `x `）・`foo x\nfoo` の `G$dgn` は 2 行目の `foo` を消し `G$2dgn` は何も変えない・VISUAL の `$vgnd` は選択のまま・`foo y foo` の `$2gN` は 1 つ目の `foo`。fixture `gn-nowrapscan-` と `gn-wrapscan-` 9 件（`:set nows` は鍵の頭に置く）。

## 追補（2026-11-14・#1550・#1643）: 入力行の `c_CTRL-U` `c_CTRL-W` と `/` `?` の生の Tab

1. **編集を 2 つ足す。** `CommandEdit` に `erase_to_start`（Vim の `c_CTRL-U`）と `erase_word`（`c_CTRL-W`）を足し、文字列とキャレットの規則は決定 1 の `edited_input` の 1 本に置く。`c_CTRL-U` はキャレットの前を全部消して後ろを残す（`:xyzd<Left><C-u><CR>` は `:d`）。`c_CTRL-W` はキャレットの手前の空白（Vim の `vim_isspace`: 空白・Tab・改行の類）を消し、その手前の同じ種類の文字の連なりを消す。種類と 1 文字の切り方は INSERT の `Ctrl-w`（#1040）と同じ関数 `vim_word_erase_begin`（`VimWordMotion` に移した・`vim_character_class` の word と `vim_character_start`）で、表を足さない。どちらも行頭では何もせず、空にしても行は閉じない（閉じるのはその後の空の行の `<BS>`・Vim の `cmdline_erase_chars` は `c != Ctrl_W` のときだけ空の行を閉じる）。VISUAL の `:` が開いた `'<,'>` は記号の連なりなので、`<C-u>` でも `<C-w>` でも 1 鍵で消える（`v:<C-u>set so=2<CR>j` は範囲なしの `:set`）。
2. **効くのは Vim の `:` `/` `?` の行だけ。** 窓は入力行が開いている間の Ctrl+U / Ctrl+W を `EditCommand` で送り（`press_command_control_key`・操作の表 `operation_bindings` より先に入力行が鍵を取る今の形のまま）、controller の入口 `rerouted_edit` が入力行の種類で決める。通常モードの「検索: 」「置換後: 」「行: 」と Ctrl+P の面では何もしない（前から何もしなかった・Windows の標準・D82）。面は core の `CommandPalette::edited` でも何もしない。
3. **再生・録画・map は打った鍵の 1 本の口。** 再生（`.`・マクロ・`:normal!` の意味の fixture）と map の右辺の `<C-u>` `<C-w>` は `command_key` から同じ `EditCommand` に写す（今までは捨てていた）。録画（`q`）は Vim 9.1 の `feedkeys(…, 'xt')` の実測どおり `^U`（0x15）`^W`（0x17）を録る（`qavj:<C-u>s/a/X/<CR>q` の a は `vj:^Us/a/X/^M`）。検索の行は前から確定したパターンの 1 鍵で録る（#1528・Vim は `/a3^Wa2^M` と打った鍵で録る・効きは同じ）。履歴は `<C-u>` で空にした後の `<Up>` が空の接頭辞（最新）を引き、`<Up>` の後の `<C-w>` `<C-u>` は引いた行を編集する（Vim の実測と同じ・履歴の歩きは編集で終わる）。
4. **`/` `?` の Tab は文字の Tab（#1643）。** Vim 9.1.0004 の `nextwild` は検索の行で `EXPAND_NOTHING` になり FAIL を返すので、打った Tab もふつうの文字として入る（source の読み・窓の打鍵は測っていない）。`:normal!` と再生の Tab も文字（実測: `x<Tab>b<Tab>c` の `/<Tab>c<CR>x` は `x<Tab>bc`・`ab<Tab>cd<Tab>ef` の `d/<Tab>e<CR>` は `<Tab>ef`）。Nib は打った Tab（`complete_next`）を Vim の検索の行では `CommandText` の Tab に読み替え（録画も文字の Tab）、`SearchLine::inserted` は Vim の検索の行だけ Tab を制御文字に数えない（`inserted_tab_input`）。通常モードの「検索: 」「行: 」は今のまま Tab を断り、Shift+Tab は何もしない。`:` の行で打った Tab は補完のまま（決定 1・ADR 0150 の決定 3）。
5. **残る違い（直していない）。** `:` の行の再生（`:normal!`・マクロ・map）の Tab は、Vim では文字の Tab（`:s/a/<Tab>/` は `<Tab>1 a`）だが、Nib の `:` の行は Tab を制御文字として断る（Ex の評価が Tab を空白として読むかを決めていないため）。入力行の Tab の描き方（Vim は `^I`）も変えていない。fixture の harness（`press_vim_keys`・`:normal!` の意味）の入力行の `<Esc>` は、Vim では行を実行する（`x<Tab>b<Tab>c` の `/<Tab><Esc>x` は `xb<Tab>c`）が、Nib は取り消す（`<Esc>` で行を実行するのは `replay_depth_` のあるマクロと `.` の再生だけ・前からの違いで、`:set so=2<Esc>j` の既存の fixture は答えが偶然同じ）。この形は fixture にしていない。
6. **契約。** fixture `cmdline-edit-` 61 件（`<C-u>` 17・`<C-w>` 25・履歴との組 6・マクロの再生 4・`/` `?` の Tab 9・閉じる形と空の行の 7 件は viewport つき（`-es` の `:normal!` は Ex モードで空の行の `<BS>` が閉じない）・マクロの本文は `<CR>` で終えず後ろに 1 鍵を置く（`<CR>` で終わるレジスタの `@a` は oracle の Vim が 1 行下へ動く・`:noh<CR>` でも同じ・既存の `macro-ex-` と同じ形））。修正前の main（`1abf1869`）の再生器 `nib_vim_replay` では、61 件のうち 51 件が落ちた（再生器は register 欄を運ばないので落ちるマクロ 5 件を含む・マクロを除くと 56 件のうち 46 件。通る 10 件は今の振る舞いと答えが偶然同じ形）。実測の表は `D:/NeNeNib/outputs/20261113/1550-impl/vim-measure.md`（84 形）。`--command-line-edit` に core の編集・面と通常モードの入力行が変わらないこと・窓の意図・録画の本文・map の右辺 5 形（`:normal!` は map を引かないので fixture にできない）。
