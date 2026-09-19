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

## 検証

候補/照合/順位/循環、共通入力session、本文/選択/undo/元モード保持、既存Exとの排他を対象unitで確認。nativeで通常とVimの入口、テーマ/フォント変更、取消、狭窓、入力配送とIMEの復元を確認する。変更した共通入力経路だけを再検証し、不変のcodec/保存競合/無関係なVim操作は成功結果を再利用する。全件検証を追加しない。
