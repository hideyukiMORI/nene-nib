# ADR 0022 — Ex入力と設定コマンドの評価

- 状態: 受理
- 日付: 2026-09-20
- Issue: #64
- 影響する規則: ARC-001 / ARC-004 / ARC-008 / ARC-009 / ARC-011 / CPP-002 / CPP-004 / CPP-007 / CPP-011 / CPP-012 / CPP-014 / QLT-008 / QLT-009 / QLT-012 / QLT-013

## 文脈

C2の設定保存に対話的な入口を足す。画面はhideが採用したEx案A（docs/design/2026-09-15-editing-look.md）。C3をEx設定コマンド（本Issue）とCtrl+Pの共通一覧（後続）に分ける。

## 決定

1. `EditorState` が任意の `CommandLine` とコマンドの表示メッセージを所有する。本文・選択・undo・Vimレジスタを変更しない。UTF-8の走査と編集位置は既存Utf8/Offsetを使い、1行256 bytesに制限する。不正文字/制御文字/上限超過は型付きの失敗。
2. NORMALの `:` は既存Vimキー表から `VimOpenCommandLine` 効果を返す。UIやcontrollerに別のNORMALキー表を置かない。入力中は文字と編集キーの意図をコマンド行へ送り、本文には送らない。Enterで評価、Escまたは空入力でBackspaceは取消。左右/Home/End/Delete/Backspace、Tab/Shift+Tab、Ctrl+Vによる1行の貼付を扱う。Ctrl+Cは取消。コマンド履歴は本Issueに含めない。
3. coreの純関数 `evaluate_ex` は文字列と現在の設定/OS外観を受け、保存する設定（問い合わせなら無し）と表示文字列、または閉じた失敗を返す。`:colorscheme` は現在の解決済みテーマ（system追従も明記）、`:colorscheme <name>` / `system` は切替。`:set fontsize=<pt>` と `:set guifont=<name>:h<pt>` を扱う。先頭のcolonは入力表示だけが持つ。未知コマンド・オプション・テーマ・不正値は明示して拒否する。
4. ptの文字列解析もFontSizeに集約し、既存settings codecから同じ経路を使う。guifontの名前は最後の`:h`より前を取り、空白入りの名前を受ける。フォント名は既存DisplayTextで検証する。保存形式と8〜40ptの範囲は変更しない。
5. `colorscheme` / `set fontsize=` / `set guifont=` の補完と、テーマ補完は純関数で生成する。テーマ一覧はBuiltinThemesの表を共用する。Tab巡回は最初のprefixと選択indexをCommandLine内に持ち、手入力/カーソル移動で解除する。候補は入力の上のpanelに表示する。
6. 設定の保存はcontrollerの一つの `persist_settings` に集め、C2の相対サイズ変更とExが共用する。同値は保存しない。成功してから状態を変え、失敗時は元設定を保持する。表示高とOS背景の更新も設定変更後に同じUI経路へ送る。コマンド結果は次の利用者入力まで表示し、描画/レイアウト通知では消さない。
7. ステータス左側を入力または結果メッセージに置き換え、右の行/桁・encoding・line endingは維持する。コマンド領域と候補panelをcoreの配置から導き、狭い窓では幅を0以上に制限する。文字とcaretは左領域内にclipし、長い入力はcaretに追従する。本文のcaretは入力中に描かない。UI文字サイズは本文のptと独立。
8. この設定用ExはNORMALから始める。Vim範囲/回数付きEx、VISUAL範囲、パイプ、VimScript、`:w` / `:q`、履歴、Ctrl+Pは後続。範囲や未対応構文を黙って別コマンドとして実行しない。Ex中のIMEはNORMALと同じ閉状態を維持し、Unicodeフォント名はUTF-8入力/貼付で受ける。本文のIME経路へ流さない。

## 検証

### 固定済みSTLの参照追加

`FontSize::parse` の `std::from_chars` は、固定toolset 14.44.35207の `charconv:691` にある `const uint32_t _Large_power_data[578]`（十進べきの不変表）を参照する。`string_view::rfind` と候補数の `std::min(initializer_list)` は同toolsetの `xutility:106/132` の範囲走査 `__std_find_end_1` / `__std_min_8u` を参照する。この3シンボルだけをcoreの許可表へ完全一致で加える。C++ランタイムは既存の固定済み依存であり、新しいライブラリ・runtimeDependencies・platformLibrariesはない。

`std::format` を導入すると（文字列だけを渡しても）ロケール参照までcoreへ入ることを実ライブラリの検査で検出したため採用しない。Exの成功メッセージは固定文字列と検証済み入力を連結して表示し、保存時の数値表記は既存adapterが担う。locale・OS・threading・自由な外部呼び出しの拒否は維持する。許可した正例とロケール/未知関数の反例、実ライブラリのシンボルを確認する。

純関数の解析/補完/UTF-8編集、controllerの本文/履歴/レジスタ保持と保存失敗、NORMALからの入口、nativeの入力/取消/テーマ/サイズ切替/再起動を確認する。C2で成功した保存競合・codecの不変部分・無関係なVim oracle/ファイルI/Oは再利用する。対象のコードが変わった場合だけ関連する検証を更新する（ADR 0021）。

## 却下

- UIにコマンド解析と保存を持つ: 状態の正本と検証が分裂する。
- コマンド文字列を本文へ一度入力して戻す: undoと選択を壊す。
- 設定変更ごとに保存処理を複製する: C2と競合保護が分裂する。
