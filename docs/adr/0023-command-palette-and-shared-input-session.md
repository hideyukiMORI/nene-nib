# ADR 0023 — Ctrl+Pのコマンド一覧と入力sessionの共有

- 状態: 受理
- 日付: 2026-09-20
- Issue: #66
- 規則: ARC-001 / ARC-004 / ARC-008 / ARC-009 / ARC-011 / CPP-002 / CPP-004 / CPP-007 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013

## 文脈

C3aのEx設定を、通常モードとVimの各モードから使えるようにする。見た目は採用済みのCtrl+P面（docs/design/2026-09-15-look.md、ADR 0008）。このIssueは設定コマンド一覧の縦切りであり、FR-006のファイル/履歴/ブックマーク/フォルダ統合は別Issue。

## 決定

1. coreの `CommandPalette` は `CommandLine` を入力編集に使い、候補の選択位置だけを追加で持つ。UTF-8/一行/256 bytesの規則と編集アルゴリズムを複製しない。Tab/Shift+Tab/上下は候補選択、左右/Home/End/Delete/Backspaceは入力編集。空欄のBackspaceは一覧を閉じない。
2. コマンド候補の正本を `ex_command_candidates` にまとめ、Exのprefix補完とpaletteが共用する。9テーマとsystemはBuiltinThemes由来。paletteでは先頭の任意の `:` を除いた入力をASCIIの大小文字を区別しない部分列で照合し、先頭一致/間の短さ/名前の順で決定的に並べる。OSやlocaleを読まない。
3. `CommandChoice` は表示名・実行文字列・`fill`/`execute`を持つ。`set fontsize=` / `set guifont=`の候補は入力を補って一覧内に留まり、値入力後はその全文を既存 `evaluate_ex` へ送る。テーマと問い合わせはEnterで評価する。未知値や保存失敗の扱いはExと同じ。専用フォント一覧を作らない。
4. applicationの任意の `CommandInput`（CommandLineまたはCommandPaletteの閉じた和型）をEditorStateだけが所有する。Exとpaletteを同時に保持できない。EditorFrameの一行表示とpalette候補はこの状態から導く。設定の評価/保存は `evaluate_ex` / `persist_settings` に一本化する。
5. Ctrl+Pは通常/NORMAL/INSERT/VISUALから開け、元の編集モード・Vim状態・本文・選択・undoを保つ。開く時は `:` を入力に置く。IME変換中は開かず、開いている間はIMEを閉じ、終了後は元モードに従い復元する。本文のキー・マウス・wheelをpalette内へ配送し、欄外クリック/Esc/Ctrl+Cは取消。候補クリックは選択してEnterと同じ処理。
6. 配置はcoreの純関数。幅640 DIP、上96、検索52、行40、案内34、角丸10を採用案から使い、左右16 DIPと本文の上下に収まるよう縮める。選択候補を可視行へ追従し、query/候補/補足はpanel内へclipする。色は既存Palette、選択はselection。本文サイズと独立したUI文字を使う。
7. この段階のCtrl+Pはコマンド一覧であることをREADMEに記す。ファイル一覧、`:e`/`:b`/`:ls`、履歴の保存、利用者テーマを実装済みと扱わない。将来のファイル統合でも入力と一覧の描画を複製しない。
8. 後の変更（[ADR 0060](0060-ctrl-p-lists-files-and-marks-select-the-source.md)・#258）: 決定 5 の「開く時は `:` を入力に置く」は、ファイルの統合で「入力は空で開き、`:` を打つとコマンド一覧」に置き換わる。コマンドの候補・照合・評価はこの ADR のまま。
9. 後の変更（[ADR 0061](0061-the-palette-takes-ime-input-and-opens-with-it-off.md)・#264）: 決定 5 の「開いている間は IME を閉じ」は、「面は IME がオフの状態で開き、開いている間は使う人に任せる。変換と確定は面の入力欄が受ける」に置き換わる。変換中は開かないことと、終了後に元のモードに従って戻すことはこの ADR のまま。
10. 後の変更（#1527・2026-11-14）: `:` の面で打った引数つきの行は、Ex の評価（`evaluate_ex`・既定の設定）が受け、**答えが設定の族だけ**のとき（設定を変えるか表示する答え: `set` `se` `setl` `setg` の族・`colorscheme`・`syntax`。例 `set so=2` `set ft=python` `setl ic` `set hls!`）、同じ文字の候補が列に無ければ先頭の候補になる（`set fontsize=` と同じ「打った値を候補にする」形）。設定の族かは答えの欄で決め（文書の保存と終了・タブ・`:s`・map・枠・面・印刷・行の移動・プレビューの要求と、途中で止まった複数の引数を持たない答え）、`CommandChoice.cpp` の `settings_only` が `ExResult` の欄を構造化束縛で全部名指しするので、欄が増えるとコンパイルが落ちてどちらの側かを決め直す（命令の名前の表は持たない・CPP-002 と同じ形）。ファイルを書く・閉じる・本文を変える・map・タブ・開く行（`w! <file>` `saveas! <file>` `wq <file>` `%s/a b/x/g` `map x y` `tabnew <file>` `e <file>`）と受けない行（`set so=abc`）は打った行を候補にせず、面は今までどおり候補の列だけ（当たる候補が無ければ Enter は何もしない・当たる候補があればそちら・`set display=g` は `set display=grid`）。引数の無い行（`q!` `tabclose`）も今までどおり表の候補だけ（独立レビュー V-1663 の M1・設計席の決め）。確定は Ex の入力行と同じ `evaluate_command` → `persist_settings` の 1 本で、今の設定で評価し直す（候補の評価が既定の設定なのは、答えの種類が設定の値に依らないため）。値の無い候補（`set scrolloff=` `set ft=` `set filetype=`）は、空の値を同じ判定が受けなければ `set fontsize=` と同じ fill（入力欄をその文字に置き換えて面を開いたまま）で、空の値に意味がある `set clipboard=` `set root=` は実行する。決め方は評価の 1 本で、形ごとの表を持たない。#1561 の `:set` の形の表（Vim 9.1 の実測）のうち Nib が受ける形は、面から打っても Ex の入力行と同じ設定・知らせ・ステータスバーになり、上の設定の族でない行は面から打っても main と同じく何も変えない（ファイル・本文・タブ・map・設定の書き込み・閉じる要求・契約 `--ex-settings`）。

## 検証

候補/照合/順位/循環、共通入力session、本文/選択/undo/元モード保持、既存Exとの排他を対象unitで確認。nativeで通常とVimの入口、テーマ/フォント変更、取消、狭窓、入力配送とIMEの復元を確認する。変更した共通入力経路だけを再検証し、不変のcodec/保存競合/無関係なVim操作は成功結果を再利用する。全件検証を追加しない。
