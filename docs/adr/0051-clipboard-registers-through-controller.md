# ADR 0051 — クリップボードのレジスタ `"+` `"*` は engine が写しを読み書きし、OS との往復は controller が `ClipboardPort` で行う

- 状態: 受理（設計席 2026-09-29・Issue #210。hide 未確認・ADR 0050 の決定 11 の 1 番目。決定 3 の読む時機は工程 1 の実装席の指摘で設計席が「鍵を流す直前」に改めた）
- 日付: 2026-09-29
- Issue: #210
- 影響する規則: FR-003 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / CPP-002 / CPP-005 / CPP-011 / QLT-001 / QLT-012
- 前提: [ADR 0050](0050-numbered-and-small-delete-registers.md)（書き手 1 本の規則・決定 3 に本 ADR が名指しの書き先を 1 つ足す）・[ADR 0048](0048-named-registers-as-one-text-table.md)（`"` の接頭辞と再生の口）・[ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)（`ClipboardPort`・決定 5）・[ADR 0035](0035-vim-visual-block-as-column-ranges.md)（NORMAL / VISUAL の Ctrl+V は矩形の鍵・決定 8）・[ADR 0036](0036-line-ending-owned-by-text-buffer.md)（LF 文書の `\r` は文字）

## 文脈

`:help quoteplus` `:help quotestar`。Windows では `"+` と `"*` は同じ OS のクリップボードを指す。Nib の Vim NORMAL / VISUAL では Ctrl+V が矩形の鍵なので（ADR 0035）、Vim モードで OS のクリップボードを使う手は `"+` しかない。

本物の Vim 9.1 の実測（Sonnet の probe `out/probes/probe-clipboard-2026-09-29.md`・クリップボードは退避して復元）:

- **書き**: `"+yy` は `"+` `"*` と無名で `"0` は不変。`"+dd` は `"+` と無名と `"1`（繰り下がりあり）。`"+x`（1 行の中）は `"+` と無名だけで `"-` も `"1` も不変。名前つきレジスタと同じ規則（ADR 0050 の決定 3 の「名指しの書き込み」）。`"*` は `"+` とまったく同じ。
- **OS に置く本文**: 行単位は各行の終わりが `\r\n`（末尾も）。文字単位は末尾に改行なし。Vim は私的な形式 `VimClipboard2` に種類を書く。
- **読み**: OS の本文の CRLF は LF に畳む。単独の `\r` は残る。末尾が改行（畳んだ後の `\n`）なら行単位、そうでなければ文字単位（`abc\r\n` → 行の `abc\n`・`ab\r\ncd` → 文字の `ab\ncd`・`abc\r\n\r\n` → 行の `abc\n\n`）。
- **`.`**: `"+p` の後の `.` は再生の時点のクリップボードを読み直す（本文も種類も新しい方）。
- **空**: 本当に空（形式なし）の `"+p` は E353 でバッファ不変。空文字列のテキストはエラーなしで何もしない。
- **矩形**: `<C-v>jl"+y` は私的な形式があるあいだ別の Vim でも矩形で貼れる。私的な形式が無ければ本文（`ab\r\nef\r\n`）から行単位になる。
- **`@+` `@*`**: OS の本文を鍵として実行する。

Nib の今の形（同じ probe の A 節）: core の engine は OS を知らず、`VimEditorView` にクリップボードは無い。`ClipboardPort`（application・UTF-8・`write` / `read` は `std::expected`）の実装は `src/adapters/win32` の 1 か所で、`CF_UNICODETEXT` だけを扱い改行を変換しない。通常モードと INSERT の Ctrl+C / Ctrl+V は `copy_selection` / `paste_clipboard` が本文をそのまま出し入れする（文書の改行の形のまま出す・読んだ本文の `\r` は残る）。engine の状態を外から書く口は `accept(StoreVimRegister)` → `vim_register_stored` など 6 つ。controller の `step_vim` は `vim_step` の結果の状態・効果・報せ・失敗を順に写す。再生（`.` と `@`）の鍵も 1 鍵ずつ同じ `step_vim` を通る。テストの替え玉 `ScriptedClipboard` は読む値と失敗を仕込めるが、書かれた回数は持たない。oracle は実機のクリップボードを書き換えるので fixture にできない。

## 決定

**engine は OS に触れない。写しを読みうる鍵を流す直前に controller が `ClipboardPort` から読んだ本文を engine の状態の写しに置き、engine はそれを普通のレジスタとして読む。engine が `"+` へ書いた本文は 1 打鍵の結果（`VimStep`）に添えて返し、controller が `ClipboardPort` へ出す。写しは命令が終わると消え、OS が正のままである。**

1. **選択**: `VimRegisterTarget` に `clipboard` を足す。`register_selection_of` は `+` と `*` を `clipboard` にする（`name` は打った鍵のまま・`append` は false）。`+` と `*` は同じ 1 つの書き先・読み元で、違いは `.` の記録に残る鍵だけ。
2. **写し（core）**: `VimState.clipboard`（`VimRegister`・空は `{"", uninitialized}`）。`vim_resting_from` は**持ち越さない**（`selected_register` と同じ「命令が終わると消える側」）。回数の桁など命令の途中の鍵は状態を写すので、`"+3p` の間は残る。
3. **読む時機（core の述語 1 本）**: `vim_reads_clipboard(const VimState &) → bool` は「`selected_register` が `clipboard`」か「`@` の名前を待っている」とき真。何が読む状態かを決めるのは engine の側（ARC-004）。controller は `step_vim` で **`vim_step` を呼ぶ直前に**いまの状態でこの述語を見て、真なら `ClipboardPort::read()` を 1 回呼び、`vim_clipboard_loaded(state, value)` で写しを置いてから鍵を流す。読みが失敗（`ClipboardFailure` のどれでも）なら空のレジスタを置く。直前に読むので、`"+` を打ってから `p` を打つまでの間に OS 側が変わっても `p` の時点の中身を貼る（Vim と同じ時機）。述語は状態だけを見るので、`"+3p` の `3` や `"+yy` の `y` の前でも読む（読まれないだけで結果は変わらない・工程 1 の実装席の指摘）。窓で打った鍵も再生の鍵も同じ `step_vim` を通るので、`.` と `@a` の中の `"+p` も再生の時点で読み直す（実測と同じ）。
4. **本文 → レジスタ（core の純関数）**: `vim_register_of_clipboard(std::string_view) → VimRegister`（`src/core/VimClipboardText.hpp` / `.cpp`）。`\r\n` を `\n` に畳み、単独の `\r` は残す。畳んだ後の末尾が `\n` なら `lines`、そうでなければ `characters`。矩形にはしない。空文字列は `{"", characters}`。
5. **読み**: `register_read`（ADR 0050 の決定 5 の 1 本）の `clipboard` は `VimState.clipboard` を返す。`p` `P` と `@+` `@*` が同じ口を通る。空（読みの失敗・本当に空・空文字列）は今の「空のレジスタ」と同じ `refused`（E353 相当・報せの文言は出さない・マクロの中ならそこで止まる）。`@@` の `last_macro` は `+` `*` も覚える。
6. **書き（ADR 0050 の決定 3 に足す）**: 名指しの書き込みが `clipboard` のときは表へ置かず、「OS へ出す本文」として返す。続く `"1` の規則・`"-` の規則・無名は名前つきと同じ（`"+dd` は `"1` へも入り、`"+x` は `"-` に入らない）。`VimStep` に `std::optional<VimRegister> clipboard = std::nullopt` を足す（報せ `notice` と同じ「1 打鍵の結果に添える値」）。`registers_written` の戻り値は次の状態と「OS へ出す本文」の組にし、4 つの呼び出し元が `VimStep` に写す。状態を外向きの箱に使わない。
7. **OS へ出す（controller の 1 か所）**: `step_vim` は効果を写した後、`step.clipboard` があれば `with_document_newlines(text, 文書の改行)` を `ClipboardPort::write` へ渡す。改行の形は Ctrl+C（`copy_selection`）と同じ「文書の形」で、OS へ本文を出す規則を 2 つにしない（ARC-001）。書けなかったときは黙って続ける（本文と無名と `"1` は engine の結果のとおり・`copy_selection` と同じ）。矩形は行を改行で繋いだ本文になる（Ctrl+C の矩形と同じ）。
8. **`.` の記録**: `"+` `"*` は名前つきと同じく記録に残る（ADR 0048 の決定 10）。番号送り（ADR 0050 の決定 8）は掛からない。
9. **契約（新しい scope `--vim-clipboard`・`tests/unit/VimClipboardTests.cpp`）**: fixture にはできない。`ScriptedClipboard` に「書かれた回数」を足し（替え玉だけの変更）、期待値は probe の実測。少なくとも: `"+yy` `"+yw` `"+dd` `"+x` の OS 側の本文と無名・`"0` `"1` `"-`／LF 文書と CRLF 文書で OS 側の改行の形／`"*` が同じ／読みの 5 形（`abc`・`abc\r\n`・`abc\n`・`ab\r\ncd`・`ab\r\ncd\r\n`）と単独の `\r`・`abc\r\n\r\n`／`"+p` は OS へ書かない（書かれた回数 0）／`3"+p`・`"+P`／`"+p` の後に替え玉の中身を変えて `.`／読みの失敗と空文字列の `"+p` は `refused` で本文不変／書きの失敗でも本文と無名は変わる／`@+` と `@@`／マクロ `"ayy@a` の中の `"+p`／矩形の `"+y` の OS 側の本文と、その後の `"+p` は矩形でない／命令が終わると写しが消えている／`vim_register_of_clipboard` の表。
10. **後続（別 Issue）**: 私的な形式か「最後に書いた本文の覚え」で矩形の種類を往復させる・`clipboard=unnamed` の設定・INSERT の Ctrl-R・通常モードと INSERT の Ctrl+V が CRLF の本文を LF の文書へ貼ると `\r` が残ること（今の挙動・本 ADR は変えない）・E353 の報せの文言。

## 強制

- 契約 `--vim-clipboard`（決定 9）: **active**（CTest の既定の `nib_tests` と scope の指定実行・`eng/protected-diff.py` は新しい scope を記録する）。
- core が OS に触れないこと: **active**（`eng/symbols.py`・ARC-003 / ARC-007。`vim_register_of_clipboard` は文字列の純関数）。
- 閉じた enum の写し漏れ（`VimRegisterTarget::clipboard`）: **active**（`switch` の網羅性・CPP-002）。
- Vim のレジスタと OS との往復が controller の `step_vim` の 1 か所（`load_vim_clipboard` と `send_vim_clipboard`）であること・`ClipboardPort` を呼ぶのがそれと、前からある `copy_selection` `cut_selection` `paste_clipboard` と入力行の貼り付け（`accept(PasteCommand)`）だけであること: **planned**（レビュー事項・`grep -n "ports_.clipboard" src/application/EditorController.cpp`）。
- fixture: **不能**（oracle が実機のクリップボードを書き換える・probe で実測）。

## 結果

得られるもの: Vim モードから OS のクリップボードへの読み書き（`"+yy` `"+p` `"+dd` `@+`）。`.` とマクロの中でも再生の時点の中身を読む。engine は純関数のまま。
失うもの・残る穴: 矩形を `"+y` して `"+p` すると矩形でなくなる（Vim は私的な形式で保つ・無名レジスタの `p` は矩形のまま貼れる）。OS へ出す改行は文書の形で、Vim の「必ず CRLF」とは LF の文書で違う（Ctrl+C と同じ）。空文字列のテキストの `"+p` は Vim ではエラーなしだが Nib は `refused` で、マクロの中では Nib だけ止まる。`"+` `"*` を選んでいる間と `@` の名前を待つ間は、鍵ごとにクリップボードを 1 回読む（`"+yy` は 2 回・読むのは `CF_UNICODETEXT` の 1 形式だけ・普通の打鍵では読まない）。再現度は fixture ではなく契約で守るので、Vim の版が変わっても機械は気づかない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| core に型のあるクリップボードのポートを足して engine から呼ぶ | engine が純関数でなくなり、`vim_step` の決定性と symbols の検査（ARC-003 / ARC-007）を失う |
| 毎打鍵、`vim_step` の前に必ずクリップボードを読んで `VimEditorView` に載せる | 1 打鍵ごとに OS を開く。打鍵の速さ（QLT-014）に乗り、巨大な本文がクリップボードにあると全打鍵が重くなる |
| `"+` を選んだ直後（鍵を食べた後）に読む | 次の鍵までの間に OS 側が変わると古い本文を貼る。直前に読めば Vim と同じ時機になり、写しの寿命も問題にならない |
| engine が「OS を読め」の効果を返し、controller が読んでから同じ鍵をもう一度流す | 1 鍵 1 効果（ADR 0012）が崩れ、`.` の記録と録画が同じ鍵を 2 回見る |
| 書きの合図を `VimState` の欄に置き、controller が出してから消す | 状態が外向きの箱になり、「`step_vim` の後は必ず空」という不変条件を型が守れない。`VimStep` は 1 打鍵で消える |
| `"+p` を読んだ後に同じ本文を OS へ書き戻す（読み書きを区別しない） | 文字以外の形式（画像・来歴のタグ）を消す。読みは OS を変えてはならない |
| OS へ出す改行を必ず CRLF にする | Ctrl+C と規則が 2 つになる（ARC-001）。揃えるなら Ctrl+C ごと変える別の判断 |

## 追補（2026-10-28・#848）: `:set clipboard=unnamed` で無名レジスタと OS のクリップボードを同期する

hide の指示（2026-10-22 夜の試用・「`p` で Windows のクリップボードを貼れない」）。決定 10 の後続の 2 番目を、決定 3・6・7 と同じ道で足す。engine は OS に触れないまま（ARC-003 / ARC-007）。

- A. **Ex**: `:set clipboard=unnamed`・`:set clipboard=unnamedplus`・`:set clipboard=unnamed,unnamedplus`（`unnamed` と `unnamedplus` の `,` 区切りの任意の組・Windows では `"*` と `"+` が同じなのでどれも同じ意味）でオン、`:set clipboard=` でオフ、`:set clipboard?` は問い。ほかの値（`exclude:` など・空の項）と `:set noclipboard` は `unsupported_argument`。`ExResult.clipboard`（`std::optional<ExClipboardRequest>`・閉じた enum `enable` / `disable` / `show`・`ExSyntaxRequest` / `ExLanguageRequest` と同じ「今の値を知る controller が答える」request の形。`std::optional<bool>` の 1 欄では「変えない」と問いを区別できない）。controller の `answer_clipboard` が 3 値を `switch` で写し、どの形も今の値を報せ `clipboard=unnamed`（オン）/ `clipboard=`（オフ）で 1 行知らせる。補完の候補に `set clipboard=unnamed` と `set clipboard=` の 2 行。
- B. **状態**: `VimState.clipboard_unnamed`（`bool`・既定 false）。`incsearch` と同じく窓に 1 つで `vim_resting_from` が持ち越す。controller の `answer_clipboard` の 1 か所だけが置く。設定ファイルにもセッションにも入れない。
- C. **書き**: `registers_written`（ADR 0050 の書き手の 1 本）の「OS へ出す本文」に、オンで**レジスタを選ばずに**無名へ書くときを足す（`y d c x X D C Y dd cc yy` と VISUAL / 矩形の `d x y c`）。`"_` は値を書かないので出さず、`"a` の名指しは Vim と同じく出さない。`"+yy` は名指しの側の 1 回だけ。出し方は決定 7 のまま（文書の改行の形・矩形は行を改行で繋ぐ）。
- D. **読み**: 述語を `vim_reads_clipboard(const VimState &, const VimKey &)` にし、今の条件か、`vim_reads_unnamed_clipboard(state, key)`（オン・レジスタを選んでいない・オペレータも次の鍵の待ちも無い NORMAL / VISUAL の `p` `P`・回数の後でも）で真。controller の `step_vim` は真なら `ClipboardPort::read()` を 1 回呼び、unnamed の側なら `vim_unnamed_loaded`（無名の写しを OS の本文で置き換える・種類は決定 4 の `vim_register_of_clipboard`・`"0`〜`"9` `"-` と名前つきと `last_macro` は変えない）、そうでなければ今までどおり `vim_clipboard_loaded` で置いてから鍵を流す。2 つの側は重ならない（unnamed の側はレジスタの選択も `@` の待ちも無い）。読みの失敗と空は無名を空にし、`p` は空のレジスタと同じ `refused`。`p` `P` 以外の鍵では読まない（打鍵の道で OS に触れない）。
- E. **`.` とマクロ**: 再生の鍵も `step_vim` を通るので、再生の時点の `p` で読み直す（決定 3 と同じ）。
- F. **契約**: `--vim-clipboard`（`tests/unit/VimClipboardTests.cpp`）に替え玉 `ScriptedClipboard` だけで足す。Ex の parse は `--ex-settings`、述語は `--vim-macro`（`verify_clipboard_read_predicate`）。
- G. **限界**: 設定に保存しない（起動で既定のオフ）。矩形の種類は往復しない（決定 7 の結果と同じ）。`@"` は読まない。`gp` `gP` と INSERT の Ctrl-R は読まない。Nib の VISUAL にはまだ `p` が無いので、VISUAL の `p` `P` は読んで無名を置き換えるが貼らない（VISUAL の `p` を足したときに同じ述語のまま貼る）。OS 側の空・読めないは無名を空にして E353 相当（無名の前の中身は戻らない）。

強制: 契約 `--vim-clipboard` の追加分は **active**（CTest の既定の `nib_tests`）。OS に触れるのが controller の `load_vim_clipboard` と `send_vim_clipboard` の 2 か所であることは決定の強制の節と同じく **planned**（レビュー事項）。
