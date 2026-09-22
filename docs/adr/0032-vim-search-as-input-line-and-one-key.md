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

- **決定 3 の追記**: 見つからない検索も `last_search` を更新する（Vim は E486 でも `@/` を書き、次の `n` は同じ失敗を繰り返す）。空の `/<CR>` `?<CR>` は直前のパターンを再利用し、無ければ E35（`No previous regular expression`）。`*` / `#` はキャレットを語の先頭へ寄せてから探す（空白上の `*` は次の語ではなく語頭から count 回）が、オペレータの範囲は元のキャレットを端に使う（`d#` の範囲）。語が無い `*` / `#` は E348（`No string under cursor`）。回数の一致が自分自身へ折り返して範囲が空なら本文もレジスタも変えない。
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
