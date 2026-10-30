# ADR 0116 — ファイルのドロップは既存の開く意図へ渡す

- 状態: 受理（実装前の設計判断。検証結果は #416 に記録する）
- 日付: 2026-10-12
- Issue: #416
- 影響: ARC-001/002/003、CPP-004/005/006、QLT-001/002/012/013、GIT-001〜004

## 文脈

hideはExplorer等からファイルを落として開く操作と、対応済みbeta.2のZIP公開を求めた。
ファイルの読込み、既存タブの再選択、未保存タブの保持は既にOpenDocumentが所有する。
ADR 0010のファイル選択と同じく、ドロップもUIが受け取るパス入力である。

## 決定

1. 主窓をWS_EX_ACCEPTFILESで作り、WM_DROPFILESを受け取る。ui/win32にShell32のDragQueryFileW / DragFinishを宣言する。既存appもShell32を使っており、外部ランタイムは増えない。OLEのIDropTargetや第2の読込み処理は設けない。
2. UIのFileDropがHDROPを引き取り、RAIIで必ずDragFinishする。個々の名前はOSへ必要長を問い合わせて確保し、MAX_PATHへ切り詰めない。UTF-16から既存to_utf8 / FilePath::parseだけを通して所有値へ移す。パス取得・変換・構文検査の失敗は既存FileFailure::unreadableとして保持する。
3. 全パスを取り出してHDROPを解放してから、OSが渡した順に既存OpenDocumentを送る。読込み・文字コード・容量制限・同じファイルの照合は従来のFilePort/Applicationだけが担う。空の無題は既存規則で置換し、編集済みタブは保持する。エラーは既存の通知を使い、後続のファイルを続ける。通知中に窓が終了したら残りを開かない。
4. 通常/Vim共通の窓入力にし、選択ドラッグを終了してから既存sendを呼ぶ。本文テキストのドロップ、フォルダをまとめて開く操作、昇格プロセスへのOS制限回避は今回含めない。
5. tests/uiの資源制約（ADR 0077）へ、DROPFILESを表すメモリだけのHGLOBALを追加する。窓・実ファイル・時計・スレッドを作らない新しい--file-drop選択で、単一/複数/日本語/長い名前/不正文字/空入力/解放を確かめる。既存のタブ契約は関連入力が不変なら結果を再利用し、実窓の入力接続とエラー後の継続は調整済み時間帯に短く確認する。
6. 受理後、ADR 0115の正規経路でclean mainからbeta.2を作成し、起動確認・公開・再取得照合を行う。

## 根拠

- [WM_DROPFILES](https://learn.microsoft.com/en-us/windows/win32/shell/wm-dropfiles): wParamはHDROP、処理後の戻り値は0。
- [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles): WS_EX_ACCEPTFILESでファイルを受け取る。
- [DragQueryFileW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilew): 件数と終端を含まない必要長を取得する。
- [DragFinish](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragfinish): OSが渡した名前のメモリを解放する。

## 追補（2026-10-30・#1047・ADR 0177 の決定 6）

ui の許可の表（`eng/architecture.json` の `platformLibraries.ui_win32`）に、印刷のダイアログ `PrintDlgExW` の `comdlg32` と、その DEVMODE を print ticket に写す `prntvpt` を足す。どちらも製品の `NeNeNib` では遅延読み込み（ADR 0130 の形）で、起動の道に載せない。
