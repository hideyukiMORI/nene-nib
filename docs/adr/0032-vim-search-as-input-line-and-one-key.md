# ADR 0032 — 検索は Ex と同じ入力行から入り、確定は engine の 1 つの鍵として届く

- 状態: 提案（実測で決定 1〜7 を確認したら受理へ更新する）
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
5. **メッセージ**: 見つからない（`E486: Pattern not found: foo`）・折り返し・未対応構文は `command_message` に出し、次の入力で消える（Ex と同じ）。`hlsearch` / `incsearch` は既定オフなので描画は変えない。
6. **IME**: 検索入力中は Ex と同じく閉じたまま。パターンの非 ASCII は UTF-8 入力 / 貼付で受ける。
7. **oracle**: fixture の鍵に `/foo<CR>` を書けば `:normal!` がそのまま検索する。パターンの `\` は JSON のエスケープに注意。見つからない検索はビープするので、その後ろの鍵は `:normal!` を命令の切れ目で分けて測る（#87 の教訓）。

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
