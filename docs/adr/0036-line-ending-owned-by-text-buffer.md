# ADR 0036 — 改行の形は本文が 1 つ持ち、`\r` が改行の一部かどうかはその形だけで決まる

- 状態: 受理（2026-09-22 の実測で決定 1〜7 を確認し、決定 4 に受け付けの条件を 1 つ足した）
- 日付: 2026-09-22
- Issue: #85
- 影響する規則: FR-003 / FR-008 / ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

Issue #84 の実測で、VISUAL の `r<CR>` は Vim では選んだ各文字を **literal CR**（`\r` 1 バイト）に置き換える（`abcd\nefgh\nijkl` に `lvjr<CR>` で行が `a\r\r\r` / `\r\rgh` / `ijkl`）。NORMAL の `r<CR>` は行を割る（#84 で実装済み）。この 2 つは fixture に採れなかった。理由は 2 つある。

1. **oracle**: `eng/vim-oracle.py` の `run_vim` は Vim の `writefile` の出力を `Path.read_text()` で読み、universal newline の変換で literal CR が LF に化ける。measure の「同じ鍵＋Esc」の比較でも見つからない。
2. **文書の模型**: `TextBuffer::line_end` は `\n` の直前の `\r` を常に改行の一部として除くので、LF 文書の行末に literal CR があるケースと CRLF の改行を区別できない。表示の桁・`$`・キャレットの位置が Vim と食い違う。

ADR 0010 は「改行の判別は core の純関数 `detect_line_ending`、本文のバイト列は変えない、単独の `\r` は改行ではなく文字として残す」と定めた。判別の結果は application の `Document` が持ち、Enter が挿す改行に使っている。`TextBuffer` 自身は改行の形を持たず、行の切り方だけが「`\r\n` も `\n` も改行」で固定されている。Vim は `fileformat`（`unix` / `dos`）で同じことを決め、`unix` では `\r` は常に文字（`^M` と描く）、`dos` では `\r\n` が改行で行末の余分な `\r` だけが文字になる。

固定 Vim 9.1 の oracle は `-S probe.vim input.txt` の順で読むので `set binary` が間に合わず、CRLF の入力は `dos` と判別されて CR が落ちる（既存の約束: CRLF は fixture にできず unit で守る）。LF 文書に literal CR が混ざる入力は、Vim が `dos` と判別しない限り（CR で終わらない行が 1 つでもあれば）`unix` として CR を文字のまま読む。Vim ソースは読まず、help（`:help fileformat` / `:help v_r` / `:help 'fileformats'`）と実測を根拠にする。

## 決定

**改行の形（`LineEnding`）は `TextBuffer` が 1 つ持ち、`\r` が改行の一部かどうかはその形だけで決める。`lf` なら `\r` は常に文字、`crlf` なら `\n` の直前の `\r` 1 つだけが改行の一部。oracle は出力をバイト列で読んで literal CR を保ち、VISUAL の `r<CR>` は各文字を literal CR に置き換える。**

1. **`TextBuffer` が改行の形を持つ**: `from_utf8(text)` が既存の純関数 `detect_line_ending` で判別して `LineEnding` を保持し、`insert` / `erase` はそれを引き継ぐ（バイト列を変えても形は変えない・ARC-009）。`line_ending()` で読める。`Document` は自分で判別せず `TextBuffer` の値を使う（判別の経路を 1 本にする・ARC-001）。`empty()` は `crlf`（ADR 0010 の決定 4 の「`\n` が無ければ `crlf`」と同じ）。
2. **行の切り方**: `line_end` は `crlf` のときだけ `\n` の直前の `\r` を 1 つ除く。`lf` では除かない。`line_terminator_end` / `line_text` / `position_of` / `offset_of` / `newline_count` は同じ規則に従う（改行の数は `\n` の数のまま）。`crlf` の文書で `\r\r\n` と並べば、最初の `\r` は文字で 2 つめが改行の一部（Vim の `dos` と同じ）。**レジスタの本文を LF に揃える経路（`without_newline_carriage_returns`）も同じ規則**で、`crlf` の文書の改行の `\r` だけを落とし、`lf` の文書の literal CR は文字として残す（`r` の置換本文 `replacement_text` も同じ）。
3. **描画と桁**: `\r` は文字なので `Column` は 1 つ数え、仮想桁は ADR 0034 の表のとおり 2（`^M`）。描画（DirectWrite）はこの Issue では変えず、制御文字を `^M` の形で描くことは別 Issue に切る（表示の桁と engine の桁は独立・ADR 0034）。
4. **oracle**: `run_vim` は `out.txt` を `read_bytes().decode("utf-8")` で読み、`\n` で分ける（`\r` を保つ）。fixture の `text` は LF 文書に限る既存の約束のうえで、literal CR を含む text は次の 2 つをどちらも満たすものだけを受け付け、生成器（`check_literal_cr`）が検査して拒否する。**(a) CR で終わらない行が 1 つ以上ある**（Vim は全部の行が CR で終わるときだけ `dos` と判別して CR を落とす。`:help 'fileformats'` と実測）。**(b) 最初の行が CR で終わらない**（`detect_line_ending` は最初の `\n` の直前だけを見るので、`a\r\nbc\ndef` を CRLF と読む。Vim は同じ入力を `unix` と読んで CR を残すので、この 1 点だけは私たちのほうが違う）。`register` の報告（NUL → LF の戻し）は変えない。測定の境界より上の変更なので、既存 fixture の再利用は `measurement_sources_match` の規則どおり 1 回きりの全件再測定になる（`--regenerate` の全件・時間は記録する）。既存の値は 1 行も変わらないことを差分で示す。
5. **VISUAL の `r<CR>`**: 文字単位・行単位とも、範囲の各文字（改行を除く）を `\r` に置き換える効果は既存の `VimReplaceRange` のまま、置換文字を `\r` にする。NORMAL の `r<CR>`（行を割る）は変えない。`.` は鍵の列なので追加なし。
6. **保存**: バイト列をそのまま書く（ADR 0010 の決定 5）。literal CR は文字として残る。
7. **範囲外**: CRLF の文書の fixture（`-S` の順で測れない・既存どおり unit）、`fileformats` の Vim 流の判別（全行 CRLF のときだけ `dos`）への変更（ADR 0010 の判別を保つ・混在文書は Vim と違いうることを「結果」に残す）、制御文字の描画、`:set ff=`。

## 強制

- 行の切り方が 1 か所であること: **active**（`TextBuffer` の対象 unit・`lf` と `crlf` の両方で `\r` の扱いを確かめる）
- oracle が literal CR を保つこと・`dos` になる入力を拒否すること: **active**（`tests/conformance/test_vim_oracle.py` の正例・反例）
- VISUAL の `r<CR>` が Vim と一致すること: **active**（oracle fixture・CNF-010）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: LF 文書の literal CR が Vim と同じ位置・桁で扱われ、VISUAL の `r<CR>` が fixture で守られる。改行の判別の経路が 1 本になる。
失うもの・残る穴: 混在文書（最初の改行が LF で後ろに CRLF が混ざる、またはその逆）は ADR 0010 の判別（最初の `\n`）に従うので、Vim の「全行 CRLF のときだけ `dos`」と違いうる（fixture では測れない）。`\r` の描画は当面 DirectWrite の既定のまま。oracle の測定境界を変えるので、既存 fixture 1 回の全件再測定が要る。

## 補足（2026-09-22・実測で分かったこと）

**`-crlf` の 10 件は CRLF の fixture ではなかった。** `line-jump-G-crlf` / `open-line-crlf-below` / `open-line-crlf-above` / `visual-wanted-crlf` / `visual-yank-crlf` / `replace-char-crlf` / `replace-char-visual-crlf` / `dot-crlf-change-word` / `dot-crlf-open-below` / `dot-crlf-remove-line` は、どれも最後の行が CR で終わらないので **Vim は `unix` と読み、CR を文字として残していた**（実測。`out/issue85-oracle/probe.json` の `reject-first-line-cr`）。それを `Path.read_text()` の universal newline が LF に戻していたため、私たちの「CRLF 文書」の答えと偶然一致していた。読み方を直すと期待値が変わり、しかも決定 4 の (b) で入力そのものが受け付けられない。10 件は fixture から外し、**engine 自身の契約として `tests/unit/NibTests.cpp` の `verify_vim_crlf_documents`（同じ `verify_vim_fixture` で再生する 10 件）へ移した**。決定 7 が言う「CRLF は既存どおり unit」の実体がここに揃う。

**レジスタも同じ分岐が要った。** `register_of` が呼ぶ `without_carriage_returns` は CR を無条件に落としていたので、LF 文書の `yy` / `dd` / `x` のレジスタから literal CR が消えた。決定 2 と同じ規則にして `without_newline_carriage_returns(text, ending)` にした（`crlf` の文書の `\r\n` の `\r` だけを落とす）。

**VISUAL の `r<CR>` は取消ではない。** ADR 0029 の `r` の取消の一覧から `<Esc>` 以外の `<CR>` を外した（`--vim-replace` の契約 `cancellation and unsupported targets preserve the visual selection and wish` から `enter` を削除）。NORMAL の `r<CR>` は決定 5 のとおり変えていない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `TextBuffer` は変えず、`\r\n` の直前の `\r` を常に改行の一部とする（現状） | LF 文書の行末の literal CR が消えて見え、Vim と桁がずれる。VISUAL の `r<CR>` を fixture にできない |
| `\r` を常に文字として扱い、`\r\n` を「改行＋文字」と読む | CRLF 文書の全行に `^M` が付いて見える。Vim の `dos` とも違う |
| oracle の出力を `writefile` の `b` フラグや NUL 置換で回避する | 行の中の `\r` と改行の区別は読み方の問題で、バイト列で読めば足りる。フラグを増やすと `-S` の順の問題と絡む |
| `Document` が改行の形を持ち `TextBuffer` に渡す | 行の切り方は `TextBuffer` の中にあるので、形も同じ所に置かないと 2 か所になる |
