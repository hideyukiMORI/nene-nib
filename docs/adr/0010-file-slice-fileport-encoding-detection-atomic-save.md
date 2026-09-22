# ADR 0010 — ファイルの縦切り: `FilePort` と `CodePagePort`・文字コードと改行の判別・一時ファイルからの置換・未保存の印

- 状態: 受理
- 日付: 2026-09-15
- Issue: #11
- 影響する規則: ARC-001 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-009 / ARC-010 / ARC-011 / CPP-001 / CPP-002 / CPP-005 / CPP-007 / CPP-009 / CPP-014 / CPP-016 / CPP-017 / QLT-009 / QLT-013

## 文脈

Issue #7 で本文（piece table）と編集は入ったが、ファイルを開けず保存もできない。未保存の本文は終了で消える。
SPECIFICATION の FR-008（D8: UTF-8 既定・Shift_JIS 自動判別・改行は読んだ形を保つ）と、ARC-003 / ARC-007（ファイルを触れるのは `src/adapters/win32` だけ）を同時に満たす
経路を 1 本決める必要がある。見た目の判断は無い（タブの題名と未保存の印は採用案の「無題」の枠に載る）。

## 決定

**ファイルは `FilePort`（バイト列の読み書き）と `CodePagePort`（CP932 ↔ UTF-8）の 2 つのポートで触り、文字コードと改行の判別は core の純関数、
書き込みは同じフォルダの一時ファイルからの置換、未保存の判定は履歴の位置で行う。**

1. **`FilePort` はバイト列だけを扱う**（application が宣言、`Win32FileAdapter` が `CreateFileW` / `ReadFile` / `WriteFile` で実装）。
   `read(FilePath, maximum_bytes) → expected<std::string, FileFailure>`、`write(FilePath, bytes) → expected<void, FileFailure>`。文字コードを知らない。
   読む上限は application が引数で渡し（定数は `EditorController` の 1 か所）、adapters は `GetFileSizeEx` の後・`ReadFile` の前に `too_large` を返す。書くほうに上限は無い（開ける上限を超えて貼り付けた本文でも保存できる。`WriteFile` の DWORD の上限を越えないよう 32 MiB ずつ分けて書く）
2. **CP932 ↔ UTF-8 の変換は `CodePagePort`**（application が宣言、`Win32CodePageAdapter` が `MultiByteToWideChar(932)` / `WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS)` で実装）。
   表せない文字は `unencodable` で返し、勝手に `?` へ置き換えない。CP932 の表を core に持ち込まない（数千行の表になる。将来 SIMD や自前表にするなら別 ADR）
3. **文字コードの判別は core の純関数 `detect_encoding(bytes)`**: `EF BB BF` で始まり残りが正しい UTF-8 → `utf8_bom`。正しい UTF-8（空を含む）→ `utf8`。
   それ以外で CP932 のバイト列として正しい（先行 0x81–0x9F / 0xE0–0xFC ＋ 後続 0x40–0x7E / 0x80–0xFC、単独 0x00–0x7F / 0xA1–0xDF）→ `shift_jis`。
   どれでもない → `undecodable`（開かず理由を 1 行で出す）。**UTF-8 が常に勝つ**のが D8 の「UTF-8 既定」の意味。
   BOM は明示的なラベルなので、BOM で始まる列は `utf8_bom` か `undecodable` のどちらかで、CP932 へは落ちない
4. **改行の判別は core の純関数 `detect_line_ending(utf8)`**: 最初の `\n` の直前が `\r` なら `crlf`、そうでなければ `lf`、`\n` が無ければ `crlf`。
   本文のバイト列は変えない（混在は混在のまま保つ。ARC-009）。Enter は判別した形を挿入する。単独の `\r` は改行ではなく文字として残す。
   **判別の結果は `TextBuffer` が持つ**（`from_utf8` が 1 度だけ判別し、`insert` / `erase` が引き継ぐ）。`Document` も `EditorState` も自分では判別せず、行の切り方と同じ所から読む（ADR 0036 の決定 1）
5. **保存は読んだ形を保つ。** BOM の有無・文字コード・（本文に残っている）改行をそのまま書く。Shift_JIS で表せない文字があれば UI が「UTF-8 で保存しますか」と聞き、
   同意なら `utf8` を載せて意図を出し直す。文字コードは意図（`SaveDocument{path, encoding}`）が必ず運び、controller は状態から勝手に変えない
6. **書き込みは同じフォルダの一時ファイル `<経路>.nib-tmp` に書いて `FlushFileBuffers` → 既存なら `ReplaceFileW`、無ければ `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`。**
   失敗したら一時ファイルを消し、元のファイルは触らない。`ReplaceFileW` が属性と ACL を保つ。フォルダを開こうとしたら `CreateFileW` に任せず `GetFileAttributesW` で先に見て `unreadable`
   （`CreateFileW` だと `ERROR_ACCESS_DENIED` になり理由が違う）。`ERROR_INVALID_NAME` は `not_found` に寄せる
7. **未保存の判定は `EditHistory::position()` と保存時点の位置の一致**（`Document::saved_position`）。保存と開くで位置を記録し、末尾の入力単位を閉じる（`EditHistory::sealed()`）ので、
   保存後の連続入力が同じ単位に混ざって位置が動かない穴を塞ぐ。undo で保存時点に戻れば印は消える。保存時点より前へ undo してから新しい編集をすると redo の列が切れて
   保存時点へ戻れないので、`saved_position` を空にして「保存するまで未保存」にする
8. **文書の状態 `Document`（経路・文字コード・保存時点の位置）は `EditorState` が所有する**（ARC-004）。意図は `OpenDocument{path}` / `SaveDocument{path, encoding}` の 2 つを
   `EditorIntent` に足す（`std::visit` の写し先が増える。CPP-002）。開くと履歴は空・キャレットは先頭・スクロールは先頭・モードは保たれる。保存はキャレットとスクロールを動かさない
9. **ファイルの失敗は表示値に載せる**（ARC-010）。`EditorFrame::document` に `DocumentView{title, path, encoding, save_state, last_failure}` を置き、`last_failure` は開く／保存の意図で
   設定され、次の意図で消える（`apply` の冒頭で毎回消す）。UI は `last_failure` があれば `MessageBoxW` で 1 行出すだけで、本文は変わらない。
   `path` は決定 10 の「経路があれば `SaveDocument`、無ければダイアログ」を窓が判断するために載せる（窓が経路を覚えると起動引数で開いた文書を取りこぼす）。
   起動引数で開けなかった理由は、最初の描画（`VisibleLines` の意図）で消える前に窓が表示値を控え、窓を見せた後に 1 行出す。
   `EditorFrame` の `tab_title` は `document.title` に一本化する（題名の経路を 2 本にしない・ARC-001）
10. **ダイアログと確認は `src/ui/win32`。** `IFileOpenDialog` / `IFileSaveDialog`（COM・`ComPtr`・CPP-017）は窓の一部で、ファイルには触らない。「保存しますか」は
    `MessageBoxW(MB_YESNOCANCEL)`。Ctrl+S は経路があれば `SaveDocument`、無ければ保存ダイアログ。Ctrl+Shift+S は必ずダイアログ。Ctrl+O と閉じる（`WM_CLOSE`）は
    未保存なら先に確認する。Esc は閉じない（ADR 0009 の決定 10 を保つ）。`CoInitializeEx(COINIT_APARTMENTTHREADED)` は合成ルート（`wWinMain`）で 1 回
11. **起動引数 `NeNeNib.exe <path>` で開く**（`CommandLineToArgvW`・合成ルート）。Explorer の「プログラムから開く」と、実機検証（`eng/verify-window.py`）の入口になる。
    経路は adapters が `GetFullPathNameW` で絶対経路にし、UTF-8 の `core::FilePath`（空でない・制御文字なし）で application へ渡す（CPP-014）
12. **読み書きは UI スレッドで同期。** 64 MiB を超えるファイルは `too_large` で開かない。1 GB（FR-013）はメモリマップとワーカー（ADR 0004）の縦切りで別に決める
13. **題名**: タブはファイル名（無ければ「無題」）、未保存なら「● 」を前に付ける。窓の題名（タスクバー）は「<題名> - NeNe Nib」。`DisplayText` の 256 バイトを超える名前は
    末尾を落として「…」を付ける（core の純関数 `tab_title_for`）。色のトークンは足さない
14. **ステータスバーの文字コードの項目**は `UTF-8` / `UTF-8 BOM` / `Shift_JIS` を出す。項目の幅は 44 → 72 DIP（採用案の「実装で凍結」の表を更新）

## 強制

- ARC-003 / ARC-007: `src/core` / `src/application` からファイルのシンボル（`CreateFileW` 等・`__std_fs_*`）が出ないことを `eng/symbols.py` が見る — **active**（既存）
- ARC-002: adapters_win32 は kernel32 だけで足りる（`platformLibraries` を増やさない）。ui_win32 に `ole32`、app に `ole32` / `shell32` を足す — **active**（`eng/architecture.json` と `nenenib_system_link`）
- QLT-009: 判別・改行・`Document` の遷移・保存の失敗系は単体テスト（偽の `FilePort` / `CodePagePort`）で分岐 90% を保つ — **active**
- QLT-013: adapters/win32 のファイル読み書きは、ビルドディレクトリ配下の一時ファイルで往復する test target（`adapter_tests` モジュール）を持つ。実機は `eng/verify-window.py` が起動引数で開いて描画を見る — **active**（この PR で結線）
- CPP-002: `EditorIntent` に 2 つ足し、`std::visit` の写し先が足りなければコンパイルが落ちる — **active**

## 結果

- 得られるもの: ファイルの開閉と保存。未保存で終了しても消えない。日本語 Windows の古い Shift_JIS ファイルがそのまま開いて、そのまま保存される。実機検証がファイルを渡せる
- 失うもの: 1 GB はまだ開けない（64 MiB の上限・同期読み込み）。文字コードと改行の手動切り替え・外部からの変更の検知・バックアップ・自動保存は無い。複数タブは無いので Ctrl+O は今のタブを置き換える
- 正直に記録しておくこと: `EditorController` のコンストラクタはポート 4 本で CPP-012 の引数上限ちょうど。次にポートが増えるときは束ねる型が要る。
  UTF-8 ↔ UTF-16 の変換関数がこの縦切りで 7 ファイルに散った（adapters 4・ui 3）。OS を使わない純関数として core に 1 本置く refactor を別 Issue にする。
  adapters/win32 の実ファイルの往復は新しい test target `nib_adapter_tests`（モジュール `adapter_tests`・`tests/adapters`。ビルドディレクトリ配下の固定名フォルダを冒頭で消して作り直す。時刻も乱数も使わない）で測り、
  QLT-009 の分岐カバレッジの対象（core / application）には入れない。`eng/prove-gates.py` の証明用ツリーへ `tests/adapters` も複写する（閾値・除外・重大度は触っていない）。
  Shift_JIS のバイト列が偶然正しい UTF-8 になる稀な場合は UTF-8 として開く（D8 の「UTF-8 既定」の帰結。手動切り替えの縦切りで逃げ道を作る）。
  判別は「正しいバイト列か」だけで、CP932 の未割り当て領域は見ない（`MultiByteToWideChar` の失敗が最後の砦で、その場合は `undecodable`）。
  `ReplaceFileW` は別ボリュームへの移動や一部の同期フォルダで失敗することがある（その場合は `unwritable` で、元のファイルは残る）。
  Ctrl+S / Ctrl+O は `PostMessageW` で駆動できない（日報）。実機では `SendInput` を試し、駆動できなければ手動確認と記録に留める。
  「保存しますか」は `MessageBoxW` で、Win11 の `TaskDialog` 風ではない（見た目の縦切りで置き換える）

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `FilePort` が判別と変換も行い、UTF-8 の本文を返す | 判別が adapters に沈んで単体テストできない。バイト列のポートと純関数に分けると判別は core で厚く測れる |
| CP932 の表を core に持つ | 数千行の表を今書く価値が無い。OS の変換で足り、変換は adapters が担う区画（CPP-014） |
| 判別できないバイト列を U+FFFD に置き換えて開く | 保存で元のバイト列が壊れる。開かないほうが損害が小さい |
| 保存時に Shift_JIS で表せない文字を `?` に置き換える | 黙って内容が変わる。聞いてから UTF-8 |
| 上書きを直接 `CreateFileW(TRUNCATE_EXISTING)` で | 途中で落ちると空ファイルが残る。一時ファイルからの置換が施主の指定 |
| 未保存の判定を内容のハッシュで | 1 打鍵ごとに全文を読む。履歴の位置で足りる（保存時に単位を閉じる） |
| ダイアログを `FileDialogPort` として adapters に置く | ダイアログは窓の一部でファイルに触れない。controller がダイアログを呼ぶと純粋でなくなる |
| 読み込みをワーカーで（ADR 0004） | 64 MiB までは同期で人が待てる。ワーカーの最初の利用はメモリマップの縦切りで版番号と一緒に入れる |
| `GetOpenFileNameW`（旧ダイアログ） | Win11 の見た目と Mica の窓に合わない。`IFileOpenDialog` が現行 |

## 関連

- [ADR 0004](0004-ui-thread-plus-one-worker.md)・[ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)
- SPECIFICATION D8 / D9 / FR-008 / FR-013・第 6 節「改行コードの既定」
