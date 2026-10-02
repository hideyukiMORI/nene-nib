# ADR 0061 — Ctrl+P の面は日本語入力を受け、IME はオフで開いて使う人に任せる

- 状態: 受理（設計席 2026-09-30・Issue #264・**施主決定 D31・D32**）
- 日付: 2026-09-30
- Issue: #264
- 影響する規則: FR-006 / FR-012 / ARC-001 / ARC-003 / ARC-004 / CPP-002 / CPP-004 / CPP-011 / QLT-001 / QLT-012 / QLT-013
- 前提（仕様の決定）: **FR-012「日本語入力（IMM32 / TSF）。変換中は Vim の鍵を奪わない」**・D5・FR-006（Ctrl+P の統合）・D28（作る順）・D29（出どころの記号は半角の `*` `@` `/` `#` `:`）。本 ADR は D28 の順の「同じフォルダ」の前に 1 本足す（D31）。
- 前提（ADR）: [ADR 0014](0014-ime-imm32-composition-outside-the-buffer.md)（IMM32・変換中の文字列は本文の外の `core::Composition`・Vim の NORMAL では IME を切る）・[ADR 0023](0023-command-palette-and-shared-input-session.md)（決定 5 の「開いている間は IME を閉じ」は、面について本 ADR が置き換える）・[ADR 0060](0060-ctrl-p-lists-files-and-marks-select-the-source.md)（決定 4 の「照合はバイト単位のまま」は本 ADR が直す）

## 文脈

Ctrl+P の面が開いている間、ui は意図を送るたびに IME を閉じ、controller は変換中の文字列も確定した文字も捨てている。履歴と同じフォルダは日本語の名前のファイルが多いほど効くので、施主は「同じフォルダ」の前にこれを入れると決めた。

| 決定 | 内容 |
| --- | --- |
| D31 | Ctrl+P の面の日本語入力を、同じフォルダ（D28 の 2 番目）の前に入れる |
| D32 | Ctrl+P の面は IME がオフの状態で開く。日本語を打つときだけ「半角/全角」を押す。面を閉じたら開く前の状態に戻る |

現物の調査（`out/probes/probe-palette-ime-2026-09-30.md`）で分かったこと:

- IME を閉じるか戻すかは、ui の `follow_ime` が意図を送るたびに決めている（入力行があるか、Vim の NORMAL / VISUAL なら閉じる）。面が開いている間に使う人が IME を開いても、次の意図でまた閉じる。
- `composition_ignored()` は、Ex の行・検索の行・面のどれが開いていても変換を捨てる。確定を入力行へ入れる道は無い。
- 変換中の文字列は `frame.composition` に載り、renderer は本文のキャレットの行にだけ差し込む。入力行の描画 `draw_command` は見ない。
- 候補窓の位置は、renderer が本文のキャレットを描いたときの矩形。入力行が開いている間は更新されない。
- 変換中の Enter・Esc・矢印は `VK_PROCESSKEY` で来て、面の鍵の分岐はどれにも当たらず無視する（面の中では未実測）。
- 照合 `match_score` は UTF-8 のバイト列の部分列で、日本語の文字の途中のバイトにも当たる。コードポイントを歩く関数は core にある（`next_code_point`）。

## 決定

**IME をどうするか（構え）は application の 1 つの値が決め、ui はその値の変わり目を実行するだけ。面の構えは「開くときに 1 度閉じて、あとは使う人に任せる」。面が開いている間の変換は面の入力欄のもので、確定は入力欄に入る。照合はコードポイントの境目で行う。**

1. **IME の構え（application・閉じた enum）**: `ImeStance { as_left, closed, closed_once }`。決めるのは純関数 `ime_stance_of(...)` の 1 本で、`EditorFrame` に載せる。
   - `as_left`（使う人が残した状態に戻す）: 通常モードと Vim の INSERT で、入力行が無いとき。
   - `closed`（閉じたままにする）: Vim の NORMAL / VISUAL と、Ex の行・検索の行が開いているとき（今までどおり）。
   - `closed_once`（入るときに 1 度閉じて、あとは使う人に任せる）: 面が開いているとき（D32）。
   - 今 ui にある「どのモードで閉じるか」の判断（`ime_blocked`）は、この関数へ移す。
2. **構えの実行（ui）**: `follow_ime` は frame の構えを読み、変わり目を実行する。
   - `as_left`: 控えた状態に戻して、控えを消す（今の `restore_ime`）。
   - `closed`: 控えが無ければ今の状態を控え、IME を閉じる。意図を送るたびに閉じ直す（今の `close_ime`。面の中で使う人が開いた後に Vim の NORMAL へ戻るときも、必ず閉じる）。
   - `closed_once`: 前の構えが `closed_once` でないときだけ、控えが無ければ控えて閉じる。続く意図では何もしない。
   - 面を閉じると、構えは元のモードの値（`as_left` か `closed`）になり、IME は面を開く前の状態に戻る。
3. **変換の行き先（application）**: 面が開いている間の `ComposeText` は今と同じ状態の欄（`composition`）に置き、`CommitText` は変換を消してから、打った文字と同じ道（`CommandText` の道・`inserted_command`）で面の入力に入れる。選択は先頭に戻る（今の入力と同じ）。Ex の行と検索の行では、今までどおり捨てる。
   - `composition_ignored()` の「入力行が開いていれば捨てる」を、「Ex の行か検索の行が開いていれば捨てる」に狭める。
   - 確定した文字が 256 バイトの上限を越えるときは、今の入力と同じく入れずに 1 行知らせる。変換中の文字列は上限に数えない。
   - 絞り込むのは確定してから。変換中の文字列では絞り込まない。
   - 変換中は面を開かない（今のまま）。面が閉じるとき（確定・取消・面の外のクリック）に変換が残っていたら、状態の変換を消し、ui が IME の変換を取り消す（`ImmNotifyIME` の `CPS_CANCEL`。変換を捨てるほかの道と同じ口を使う）。
4. **見せる値（application）**: `EditorFrame` の変換は、どこに描くかを型で分ける。面が開いていれば入力行の変換（`command_composition`）、そうでなければ今までどおり本文の変換（`composition`）。両方が同時に値を持つことは無い。ui が「変換中か」を見る所は、どちらかに値があること（1 つの関数）。
5. **描画と候補窓（ui）**: 入力行の描画 `draw_command` は、入力行の変換があれば、キャレットの位置に変換中の文字列を差し込み、本文と同じ下線（`draw_clauses`）を引き、キャレットを変換の中の位置に置く。キャレットを描いたら、その矩形を候補窓の位置の欄（`caret_rectangle_`）に書く。候補窓は面の入力欄の下に出る。色は今のトークンだけ。
6. **照合はコードポイントの境目（core）**: `match_score` は、query をコードポイントごとに歩き、候補の中を**コードポイントの境目から**探す（`next_code_point`）。ASCII の大文字と小文字を区別しないこと・空白を飛ばすこと・点の決め方（候補の長さと飛ばした量の和・最初の文字までは 4 倍・単位はバイト）は今のまま。ASCII だけの query と候補では、結果は今と 1 つも変わらない。
7. **範囲の外（後続）**: Ex の行と検索の行の日本語入力・変換中の文字列での絞り込み・全角の `＃` `＠` `：` を出どころの記号として扱うこと・全角と半角やひらがなとカタカナを同じとみなす照合・再変換と TSF 固有の機能・面の IME の検査を `eng/verify-window.py` に足すこと。

## 強制

- 契約（照合）: **active**（#264）。`tests/unit/CommandPaletteTests.cpp` の `verify_listed_code_points` が、カタカナ・ひらがなの query がそれぞれの名前にだけ当たること・「め」が「アあ」の継続バイトにまたがって当たらないこと・空白で区切った query も文字の境目で当たること・ASCII と日本語の混在（ASCII は大文字と小文字を区別しない）・ASCII の点は今までどおりバイトで数えること・Ex の候補も同じ照合を使うことを守る。入口は scope `nib_tests --command-palette` と引数なしの既定の実行（CTest `nib_unit`）。
- 契約（構えと変換の行き先）: **active**（#264）。`tests/unit/ApplicationTests.cpp` の `verify_ime_stance_table`（通常 / Vim × 5 つのモード × 入力なし・Ex の行・検索の行・面の全組で `ime_stance_of` の値）と `verify_ime_stance_frame`（controller の frame の構え: 通常で Ctrl+P → `closed_once`・閉じる → `as_left`・Vim の NORMAL → `closed`・Ctrl+P → `closed_once`・もう一度 Ctrl+P → `closed`・`:` → `closed`・`i` → `as_left`）。入口は既定の実行（CTest `nib_unit`）。`tests/unit/CommandPaletteTests.cpp` の `verify_palette_composition`（通常と Vim の NORMAL の両方で、面の変換は `command_composition` にだけ載り本文と一覧を変えないこと・確定は面の入力に入り変換が消えること・面を閉じる取消と確定で変換が消えること・確定は undo の単位を作らないこと・上限を越える確定は入らず知らせが出ること・Ex の行と検索の行では変換も確定も捨てること・変換中は記号の案内を出さないこと `verify_palette_hint_composing`）と `verify_palette_input_isolation`（面に打った文字が本文に入らず、確定が面の入力に入ること）。入口は scope `nib_tests --command-palette` と既定の実行。
- 閉じた和型の写し漏れ: `ImeStance` は **active**（#264）。ui の `EditorWindow::follow_ime`（`src/ui/win32/EditorWindow.cpp`）の `switch (frame.ime)` は `as_left` `closed` `closed_once` を写し、`default` は無い（後ろは `std::unreachable()`）。application の `ime_stance_of`（`src/application/ImeStance.cpp`）は、入力の種類を `std::visit` で型ごとの `stance_of` へ写し（`CommandLine` `SearchLine` `CommandPalette`）、入力が無いときは `default` の無い `switch` で `EditMode` と `VimMode` を写す（CPP-002）。
- 実機の確認: **planned**（本物のキー入力が要る。設計席が施主に確かめてから回す。機械の必須 check ではない）。#264 は設計席が 1 回限りのスクリプトで確かめた（gate-proofs の #264 の節）。
- fixture: **不能**（oracle の対象ではない）。既存の fixture は不変（`eng/protected-diff.py`）。

## 結果

得られるもの: Ctrl+P の面で日本語の名前を打って絞り込める。IME をどうするかの判断が application の 1 つの関数になり、契約で守れる。日本語の名前と query が、文字の途中のバイトで誤って当たらない。
失うもの・残る穴: 日本語の名前を探すたびに「半角/全角」を 1 回押す（D32）。変換中は一覧が動かない（確定してから絞り込む）。全角で打った `＃` `＠` `：` は記号にならない。Ex の行と検索の行では今も日本語を打てない。変換中の鍵（Enter・Esc・矢印）が面に届かないことは、IME が `VK_PROCESSKEY` にすることに頼っている（実機で確かめる）。

- Vim の NORMAL で使う人が IME を開いても、次の 1 打鍵で閉じ直す（決定 2 の `closed`）。前は控えがあれば何もしなかったので、開いたままだった。
- 面の記号の案内（`# タブ　@ 履歴　: 設定`）は、入力が空で、入力行が変換中でないときだけ出す。変換中の文字列と重なるため（実機の画で見つけた。判断は `command_palette_view()` の 1 か所・契約 `verify_palette_hint_composing`）。
- 入力行の変換の面と下線は、文字の 1 行の上下（キャレットの棒と同じ画素）に引く。入力欄の領域は 1 行より高いので、領域をそのまま `draw_clauses` に渡さない（`draw_command_clauses`）。
- 本文の変換を状態から消す既存の道（ステータスバーのモードの切り替え・タブの切り替え・ファイルを開く）は、今も IME の側の変換を取り消さない。IME の側を取り消すのは、面の入力行の変換が消えたときだけ（残る穴）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 面を開く前の IME の状態のまま開く | 施主がオフで開くと決めた（D32）。ファイル名と設定は半角が多く、`:` が全角になると設定が出ない |
| 面が開いている間も、意図のたびに IME を閉じ直す（今の形） | 使う人が「半角/全角」で開いても、次の 1 打鍵で閉じる。日本語を打てない |
| 構えの判断を ui に置いたまま、面の枝を足す | 状態の意味の判断が ui に増える（#238 と #248 で直した形）。ui の分岐は単体の契約で守れない |
| Ex の行と検索の行でも変換を受ける | Vim の `:` と `/` は NORMAL から入り、IME は切ってある（ADR 0014 の決定 5）。検索の行で IME を開く規則は別に決めることが多い。面だけに限る |
| 変換中の文字列を入力の一部として絞り込む | ひらがなの途中の文字列で一覧が動き、確定の漢字で動き直す。入力の型（256 バイト・UTF-8 の 1 行）と変換の型を混ぜることになる |
| 変換中の文字列を `InputLineView`（core）の欄にする | 変換の見せる値（下線・キャレット）は application の `CompositionView`。core の入力行の型を変えずに、frame の欄で「どこに描くか」を分ける |
| 全角の記号も出どころの記号にする | 面は IME がオフで開くので、記号は半角で打てる（D32）。表に行を足すだけで後から入れられる |
