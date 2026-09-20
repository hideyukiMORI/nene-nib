# ADR 0029 — rは排他的な次文字待ちと範囲置換の効果で表す

- 状態: 受理
- 日付: 2026-09-21
- Issue: #84
- 影響する規則: FR-003 / ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈と実測

NORMALと文字/行単位VISUALのrを追加する。既存の入力待ち・UTF-8走査・VimVisualRange・controllerのreplaceとEditHistoryを使う。固定Vimのhelpと測定を根拠にし、ソースは読まない。測定は `out/issue84-oracle/`。

NORMALはcount文字を同じ文字へ置換し、最後の置換文字へ移る。行内にcount文字が無ければ失敗し、行をまたがない。Enterはcount文字を1つの改行に置換して次行の先頭へ移る。VISUALは回数を選択の広さに加算せず、既存の選択範囲の文字を置換し、元の改行を保ち、範囲先頭へ戻る。レジスタは不変。Esc/Backspace等の取消はVISUALの選択を維持する。

VISUAL r<Enter>はliteral CRを書き込む。oracleのread_textがCRをLFへ変換する問題と、TextBufferが内容末尾のCRとCRLFを区別できない問題を[Issue #85](https://github.com/hideyukiMORI/nene-nib/issues/85)へ分離した。この2fixtureを採用せず、今回はその入力を置換せずに待ちを解除する。普通のUnicode文字/水平タブとNORMALのEnterが対象。制御文字の引用、Ctrl-e/yの隣行参照、制御文字置換は後続とする。

## 決定

1. VimPrefixへrを追加し、既存のVimInputWaitだけで次文字を待つ。検索待ち・g待ちと同時には存在しない。通常の数字/命令解釈より先に次キーを消費する。NORMALで行内文字数が不足する場合は待ちを開始しない。オペレータ待ちの後のrは移動ではないため既存の取消へ流す。
2. 新しい効果VimReplaceRangeは半開区間・LFの置換本文・LF換算の最終caretだけを持つ。本文/選択/履歴を複製所有せず、controllerの既存replaceへseparate境界で一度だけ渡す。通常モードやINSERTの効果を再配送して削除と挿入に分けない。
3. NORMALの範囲は行末までのcode point数で回数を検証してから既存走査で求める。VISUALの範囲はvim_visual_rangeが正。置換本文はcode point単位で作り、既存のCRLF/LF改行をLFとして保つ。新しいUTF-8デコーダや行索引は追加しない。
4. 新しい効果は既存with_document_newlinesを使う。caret_after_insertをat/body/caret/newline_bytesを受ける同じ処理へ整理し、VimInsertAtとVimReplaceRangeの両方で呼ぶ。改行変換とcaret補正の第2経路を作らない。一般のNORMALキャレットの寄せ方は変えない。
5. 成功時はNORMALへ戻り、待ち/count/operator/wanted columnを消費し、レジスタ・検索記憶・scrollを維持する。取消はfinished_input_waitを使い、元のモード/選択/希望列を保つ。UI/IME配送・描画・保存schemaは変更しない。

## 限定検証と強制

新規oracleと手書きの途中状態/取消後続行/CRLF bytes/undo/redo、直接共有する検索・gの待ちと挿入後caret補正の代表を確認する。既存fixtureは入力・生成metadata・測定ソースを照合して逐語再利用する。対象build/tidy、symbols、conformance、変更C++のformatを実行し、成功結果をpush/review/mergeで再利用する。

型の写し先は既存のvariant/switch網羅性で強制（active）。新しい振る舞いは39fixtureと途中状態・保存bytes・履歴のテストで強制する（active）。selector `--vim-replace` は直接共有する既存12fixtureと待ち処理も含む460 checksに成功した。実機操作はnative pipe接続エラーで未実施。実行コマンドと証拠は [gate-proofs 5-u](../quality/gate-proofs.md)。全件unit・全oracle・無関係な設定/テーマ/性能は対象外。

## 範囲外と却下

R、矩形VISUAL、dot記録、IME方針変更、Ctrl-v等の引用入力と制御文字置換、Issue #85は含めない。削除+挿入の効果列、UIでのr待ち、新しい本文/undo管理は、既存の純粋engineと単一replaceで表現できるので採用しない。
