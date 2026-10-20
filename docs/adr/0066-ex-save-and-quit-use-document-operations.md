# ADR 0066 — Ex の保存と終了は既存の文書操作を通る

- 状態: 受理
- 日付: 2026-10-03
- Issue: #284
- 影響する規則: FR-003 / FR-008 / D22 / ARC-001 / ARC-003 / ARC-004 / ARC-010 / ARC-011 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / QLT-001 / QLT-012 / QLT-013

## 文脈

一般 Ex は ADR 0022 からの残件で、FR-003 の `:w` と `:q` が未実装。文書の保存は ADR 0010 の `SaveDocument`、タブを閉じるのは ADR 0056 の `CloseTab`、`:tabclose` の確認は ADR 0057 の `close_request` に既にある。命令の入口のために書き込みや終了を複製しない。

固定 Vim 9.1 を `-u NONE -i NONE -N -n -es` で非対話実行し、名前あり/無題と保存済み/未保存の4条件・26命令、計104ケースを観測した。`out/probes/ex-save-quit-2026-10-03.json`。専用文書以外のファイル・クリップボード・前面入力は使わない。

## 決定

1. **基本命令を一段階として実装**。引数なしの `:write`（`w`から）、`:quit`（`q`から）、`:quit!`、`:wq`、`:xit`（`x`から）、`:exit`（`exi`から）を受ける。core の `ExDocumentName` の表が省略と補完を決め、`ExResult.document` に閉じた `ExDocumentVerb` を返す。既存の256 bytes・単一行UTF-8・パイプ拒否を共用する。
2. **未対応構文は実行しない**。ファイル名・範囲・追記・パイプ・外部コマンド・保存側のbang（`:w!` / `:wq!` / `:x!`）・全タブ命令を本件に含めない。`!`を受けるのはquitの名前に直結した一つだけ。`:q !`は拒否する。`:ex`はVimの別命令であり、exitの省略として受けない。
3. **保存の正典は一つ**。既存の `accept(SaveDocument)` の実処理を `save_document`（`expected<void, FileFailure>`）へ移す。GUIもExもこの関数だけを呼ぶ。現在のパスと文字コードを使い、本文・改行・BOMを保つ。成功時だけ履歴をsealし保存位置を更新、失敗時は状態を変えない。adapterの一時ファイルによる原子的保存は不変。
4. **`:w`は常に保存**。パスがなければ `E32: No file name` として書かない。成功は一行で知らせる。Exからの変換/書き込み失敗も一行の結果にし、GUIの保存失敗ダイアログやUTF-8への変換提案は既存のGUI経路に保つ。Exが文字コードを勝手に変えない。
5. **`:q`は未保存なら止まる**。保存済みなら現在のタブだけを閉じる。未保存なら `E37: No write since last change (add ! to override)` を表示して本文・タブを保持する。`:q!`は現在のタブの変更を破棄して閉じる。ほかのタブは閉じない。Vimの分割窓は対象外なので、閉じる対象は製品の文書タブである。
6. **保存してから閉じる**。`:wq`は常に保存し、`:x`系は未保存のときだけ保存する。保存に失敗したら閉じない。変更のない無題の`:x`は書かずに閉じる。無題の保存はこの段階では既存のCtrl+Sで名前を付ける。
7. **終了の経路も一つ**。Exの条件を満たした後は既存の `CloseTab` に渡す。最後のタブが立てる `EditorFrame.closing` はuiの `deliver` の最後で一回だけ受け、既存の `leave(last_tab_closed)` でセッションを終了する。×・Ctrl+F4・`:tabclose` は従来どおりuiで確認してから同じ `CloseTab` を送る。`close_tab` 自体からの窓終了処理は移動して残さない。フレームを作り直す前に終了の印を保持し、窓を壊した後は触れない。
8. **変えない境界**。未保存判定は既存の履歴の保存位置、隣への切替は `reach_tab`、閉じたファイルの履歴とセッションも既存の経路。Exで別のdirty判定・undo・終了確認・ファイルI/Oを作らない。

## 強制と検証

新規 `--ex-document` 204 checksで省略・拒否・無題・未保存・保存成功/失敗・文字コード・undo・ほかのタブの保持と既存GUI保存の契約を確認した。共有評価と補完は `--ex-settings` 195 / `--ex-files` 82 / `--command-palette` 356、閉じる直接の依存は `--tabs` 291 / `--session` 199 / `--history` 38。計1365 checks成功。Debugの静的解析・sanitizer、conformanceとsymbolsも成功。Releaseはd078f9aで生成。hideの今回分の了承後、専用文書を使った実機7場面・保存内容/更新時刻・3回の正常終了を確認し、全画像を目視して受理した。コマンドと初回失敗の修正は[gate-proofs 5-cg](../quality/history/5-cg-ex-save-quit.md)。全件Vim・性能は実行しない。

## 結果

Vimの基本的な保存・終了が使え、保存失敗で文書を失わない。ファイル名引数や範囲・強制上書き・全タブ終了は後続で扱う。保存形式・port・adapterの契約は増えない。

## 却下

- Ex専用の保存や窓の破棄: 原子的保存・dirty・履歴・セッションの意味が分裂する。
- `:q`でGUIの保存確認を出す: Vimの未保存拒否と明示的な`:q!`の意味を失う。既存の`:tabclose`の確認はそのまま保つ。
- `:wq`で書く前や失敗後に閉じる: 保存できていない本文を失うため採らない。

## 追補（2026-10-20・#501）: 保存側の `!` と `:noh` の省略

決定 2 で外した保存側の bang を受ける。`ExDocumentName` の表の各行に `forced`（`!` を受けたときの条件・空なら受けない）を足し、`:w!` は `write`、`:wq!` は `write_quit`、`:x!` `:xit!` `:exit!` は `update_quit` に写す。Nib には読み取り専用の拒否が無いので、`!` は「今と同じ書き込み」で、`ExDocumentVerb` と controller は変えない。`:q!` だけが別の条件（`quit_force`）。
`!` の付いた形は引数を取らない。`:w! file` の上書きは書き込みの条件（同じでないパスは `create_new`）に無いので、`!` を黙って捨てず「Not supported」にする。`:saveas!` も同じ。`:tabs!` など表の外の `!` は今までどおり拒否する。
補完は `!` で条件が変わる `quit!` だけを並べ、`write!` `wq!` は並べない。
`:nohlsearch` は Vim 9.1 のとおり `noh` から完全な形までの前方一致を受け（`:nohl` `:nohls` …）、`:nohx` `:nohlsearchx` は unknown_command。タブ・一覧・保存の表と同じ省略の判定 `abbreviates` の 1 つを使う。補完の候補は完全な形の `nohlsearch` 1 つで、省略の形を並べない。

## 追補（2026-10-20・#738）: `:w! <file>` と `:saveas! <file>` の上書き

#501 で「Not supported」にした `:w! <file>` と `:saveas! <file>`（`:sav! <file>` などの省略を含む）を受ける。`ExDocumentRequest` に `overwrite` の印を足し、`!` と引数の両方があるときだけ立てる。controller の `write_ex_path` は、別のパスへの書きでも `overwrite` なら `FileWriteMode::create_new` でなく保存と同じ `FileWriteMode::replace` を選ぶ（一時ファイル → 既存なら `ReplaceFileW`・無ければ `MoveFileExW`・ADR 0010 の決定 6 / ADR 0135）。`replace` が既に「既存を置き換える」書きの道なので、`FileWriteMode` に同じ意味の第 2 の値（`overwrite`）は足さない（ARC-001）。
文書のパスの規則は `!` の無い形と同じで、`:w! <file>` は今のパスと保存位置を保ち（写しを書く）、`:saveas! <file>` は成功したらそのパスを文書のものにする。`!` の無い `:w <file>` `:saveas <file>` は今までどおり既存があれば `E13: File exists` で拒否する。
Vim の `!` は読み取り専用も越えるが、Nib は越えない。読み取り専用・ロック中など置き換えられないときは今までどおり失敗の値（`Could not write file`）で、文書の状態を変えない。`:wq! <file>` `:x! <file>` は本件に含めず「Not supported」のまま。`:saveas!` を引数なしで打ったときも今までどおり拒否する。補完は変えない（`saveas!` は条件を変えないので並べない）。
