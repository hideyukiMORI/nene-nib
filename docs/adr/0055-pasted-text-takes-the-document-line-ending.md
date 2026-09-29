# ADR 0055 — 貼り付けた本文の改行は文書の改行の形に揃える

- 状態: 受理（設計席 2026-09-29・Issue #235・**施主決定 D19**「貼り付けたときに行末へ `^M` が出る件を直す」）
- 日付: 2026-09-29
- Issue: #235
- 影響する規則: FR-002 / ARC-001 / ARC-009 / CPP-007 / QLT-001 / QLT-012
- 前提: [ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)（`ClipboardPort`・決定 5）・[ADR 0010](0010-file-slice-fileport-encoding-detection-atomic-save.md)（読んだ形のまま保つ）・[ADR 0036](0036-line-ending-owned-by-text-buffer.md)（改行の形は `TextBuffer` が持つ・LF 文書の `\r` は文字）・[ADR 0051](0051-clipboard-registers-through-controller.md)（`"+p` は CRLF を LF に畳む・決定 4）

## 文脈

Windows のアプリはクリップボードの本文の改行を CR+LF で置く。Nib の通常モードと Vim の INSERT の Ctrl+V（`EditorController::paste_clipboard`）は、読んだ本文をそのまま `replace` へ渡す。改行が LF の文書（ADR 0036: `\r` は常に文字）へ貼ると、各行の終わりに `\r` が文字として残り、`^M` と描かれる（ADR 0040）。逆に、LF の本文を CRLF の文書へ貼ると、その行だけ改行が LF になり、文書の中で改行の形が混ざる。

Vim の `"+p`（ADR 0051）は CRLF を LF に畳んでレジスタに置き、貼るときに `with_document_newlines` で文書の形へ直すので、`^M` は出ない（2026-09-29 に実機で確認: CR+LF の 2 行を LF の文書へ `"+p` して 2 行になる）。同じクリップボードの本文が、Ctrl+V と `"+p` で違う結果になっている。

施主は「直す」と決めた（D19）。VS Code などの編集器も、貼り付けた本文の改行を文書の形に揃える。

## 決定

**通常モードと INSERT の貼り付けは、クリップボードの本文の改行を文書の改行の形に揃えてから本文へ入れる。揃え方は `"+p` と同じ 2 段（CRLF を LF に畳む → LF を文書の形へ直す）で、畳む関数は core の 1 本を共有する。**

1. **畳む関数は 1 本（core）**: `clipboard_line_feeds(std::string_view) → std::string`（`src/core/ClipboardText.hpp` / `.cpp`）。`\r\n` を `\n` に畳み、単独の `\r` は文字として残す（ADR 0051 の決定 4 と同じ規則・Vim の実測）。`vim_register_of_clipboard` はこの関数を呼んでから種類を決める形に直し、畳む規則を 2 か所に書かない（ARC-001）。
2. **貼り付け**: `paste_clipboard` は `with_document_newlines(clipboard_line_feeds(本文), 文書の改行)` を `replace` へ渡す。LF の文書へは LF、CRLF の文書へは CRLF で入る。undo の単位・選択の置き換え・キャレットの位置は今のまま。
3. **単独の `\r`**: 改行として扱わず、文字として残る（`^M` と描かれる）。古い Mac の改行（CR だけ）の本文は揃えない。
4. **変えないもの**: コピーと切り取り（Ctrl+C / Ctrl+X・`"+y`）が OS へ出す本文は文書の形のまま。入力行（`:` と検索）への貼り付け（`accept(PasteCommand)`）は変えない。ファイルを開く・保存する経路は変えない（ADR 0010・読んだ形のまま）。IME の確定と打鍵は変えない。
5. **契約**: `ScriptedClipboard` に仕込んだ本文で、LF の文書へ CRLF の 2 行・CRLF の文書へ LF の 2 行・CRLF の文書へ CRLF・単独の `\r` を含む本文・末尾だけ CRLF・選択を置き換える貼り付け・Vim の INSERT の Ctrl+V・貼り付けの後の Ctrl+Z が 1 回で戻ること。`clipboard_line_feeds` の表。`"+p` の既存の契約（`--vim-clipboard`）は不変。

## 強制

- 契約（決定 5）: **planned**（本 Issue の実装で **active**・既存の scope の翻訳単位）。
- 畳む関数が 1 本であること: **planned**（レビュー事項・`grep -rn "clipboard_line_feeds" src` が `ClipboardText.*` と `VimClipboardText.cpp` と `EditorController.cpp` だけ）。
- fixture: **不能**（通常モードの貼り付けは oracle の対象ではない）。

## 結果

得られるもの: ほかのアプリからコピーした本文を貼っても `^M` が出ない。文書の中で改行の形が混ざらない。Ctrl+V と `"+p` が同じ本文を同じ結果で貼る。
失うもの・残る穴: 貼った本文のバイト列はクリップボードの中身と同じではなくなる（改行だけ）。CR をわざと貼る手段が Ctrl+V からは無くなる（CRLF の CR だけ・単独の CR は残る）。入力行への貼り付けは改行を含む本文をそのまま受ける（今の挙動）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 今のまま（読んだ本文をそのまま入れる） | 施主決定 D19。LF の文書で各行に `^M` が出て、ほとんどの使い手には不具合に見える |
| LF の文書へ貼るときだけ CR を落とす | CRLF の文書へ LF の本文を貼ると改行の形が混ざる穴が残る。両方向を 1 つの規則で揃える |
| 単独の `\r` も改行として揃える | `"+p`（Vim の実測）と規則が分かれる。CR だけの改行の本文は今の Windows ではまれ |
| コピーも同時に CR+LF に揃える | 別の判断（施主は「今までの Ctrl+C と同じ」のままと確認）。出す側は変えない |
