# ADR 0067 — Ex のファイル名は共通保存へ渡し、新規作成を原子的に守る

- 状態: 受理
- 日付: 2026-10-03
- Issue: #286
- 影響する規則: FR-003 / FR-008 / D8 / D22 / D39 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-010 / CPP-002 / CPP-004 / CPP-011 / QLT-001 / QLT-012 / QLT-013

## 文脈

ADR 0066で引数なしの保存・終了を既存経路へ接続した。次は無題の命名と別名への保存。GUIの`SaveDocument`は常に名前を変更するため、Vimの`:w 別名`をそのまま渡すと元文書の名前と未保存状態を失う。

固定Vim 9.1を非対話で48ケース観測した（`out/probes/ex-write-path-2026-10-03.json`）。名前ありの`:w 別名`はコピーで元の名前と未保存状態を保つ。`:saveas`は名前を変える。未保存の名前あり文書の`:wq 別名`/`:x 別名`はコピーを書いてもE37で閉じない。無題への保存や`:saveas`では書き込み失敗後も名前が変わるケースがあり、この点はhideがNib既存の「成功時だけ変更」を選んだ（D39）。

## 設計

1. **名前のある保存要求**。`ExDocumentRequest`が既存の`ExDocumentVerb`と任意の`FilePath`を持つ。`:write` / `:wq` / `:xit` / `:exit`は任意の引数、`:saveas`（最短`sav`）は必須の引数を受ける。`:quit`系には名前を渡さない。省略と補完は同じ`ExDocumentName`の表。
2. **ファイル名の範囲**。絶対/相対Windowsパス、日本語と空白、空白の前のバックスラッシュによるescapeを受ける。普通のバックスラッシュはパス区切りのまま。範囲・bang保存・追記・外部命令・`++opt`、`%` / `#` / 環境変数 / ホーム / wildcard / backtickの展開は拒否する。引用符で囲む構文とパス補完も後続。既存256 bytes・単一行UTF-8・パイプ拒否を維持する。
3. **パス解決はOS境界**。`FilePort::resolve`が`absolute_file_path`を共用して絶対パスを返す。相対指定の基準はプロセスの作業フォルダでVim既定と揃える。core/applicationにOS・ファイル・環境参照を入れない。別の開いているタブが同じファイルを持つ場合はEx保存を拒否してその文書を守る。
4. **保存の意味を型で区別**。`SaveDocument`へ`FileWriteMode`（replace/create_new）と`SaveIdentity`（update_document/retain_document）を持たせ、既存GUIは既定のreplace/update_document。名前ありの別名`:w`はcreate_new/retain_document、無題の命名と`:saveas`はcreate_new/update_document。同じファイルへはreplace。文字コードの変換・書き込みは既存`save_document`の一つで行い、コピーでは元文書の保存位置や名前を更新しない。
5. **新規作成を最終配置でも守る**。`FilePort::write`の2引数の既存呼び出しは一つの非virtual転送からreplaceを渡し、実体はmode付きのvirtualだけ。adapterは共通の一時ファイル/Flush経路を使い、create_newでは最終配置に`MOVEFILE_REPLACE_EXISTING`を付けない。存在する別ファイルは`FileFailure::already_exists`（ExでE13）。存在確認だけに頼らず、書いている間に別プロセスが作っても置き換えない。[MicrosoftのAPI定義](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)を参照。
6. **一時ファイルの所有**。create_newでは一時ファイルも`CREATE_NEW`で取得し、既存の同名一時ファイルを壊さない。失敗時に削除してよいのは自分が作成に成功した一時ファイルだけ。既存replaceの保存挙動を維持し、書く処理と最終配置の経路は共用する。
7. **終了条件**。`:wq`は書き、`:x`は未保存時だけ書く。その後、現在の文書がなお未保存（別名コピー）ならE37で止まる。現在の文書を保存できたときだけ既存`CloseTab`へ渡す。変更のない`:x ファイル`は書きもパス解決もせず閉じる。
8. **名前の更新は成功時だけ（D39）**。hideが2026-10-03に「良い。進めて。」と推奨案を了承した。無題への保存と`:saveas`は、書き込み成功後にだけ名前と保存位置を更新する。解決・変換・存在時拒否・書き込みのどの失敗でも元の文書名と保存位置を保つ。Vimの失敗時にも名前が変わる挙動とは意図的に異なり、NibのGUI保存と同じ意味にする。

## 検証

新規のExパス90、共有Ex文書保存204、補完候補195、Exファイル82、コマンド候補356、実adapterのファイル64、計991 checksが成功。解析・copy/adopt・失敗時のD39・保存位置/undo・終了条件・他タブの保護・文字コード/BOM/改行と、実ファイルの新規作成・既存ファイル/一時ファイルの保護・失敗した置換の後始末を確認した。

Debug/tidy/ASan/UBSan、symbolsとconformanceも成功。途中の失敗と修正・各検査の必要性・再利用は[gate-proofs 5-ch](../quality/gate-proofs.md#5-ch--名前付きex保存issue-286adr-0067)に集約する。全件Vim/性能は実行しない。

## 却下

- Ex専用の書き込み: 文字コード・原子的保存・失敗と保存位置が分裂する。
- 存在確認してから無条件に置換: 確認と書き込みの間に作られた他のファイルを失う。
- コピーを通常のSaveDocumentと同じ名前更新として扱う: Vimの`:w 別名`の意味を失い、元文書の未保存印まで消す。
- coreでパスを絶対化: 作業フォルダというOS状態への依存を純粋層へ持ち込む。

## 残る範囲

強制上書き・ファイル名展開・`:cd`・範囲/追記・全タブ保存/終了は後続。同じファイルの比較は既存のOS序数比較で、短い名前とリンクの解決はADR 0056の既知の範囲外。保存schemaは変えない。
