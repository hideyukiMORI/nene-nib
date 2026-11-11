# ADR 0009 — 編集の縦切り: piece table の本文・UTF-8 の内部表現・undo の単位・編集中の状態の見た目

- 状態: 受理
- 日付: 2026-09-15
- Issue: #7
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / ARC-007 / ARC-010 / ARC-011 / CPP-001 / CPP-005 / CPP-007 / CPP-012 / CPP-014 / CPP-016 / CPP-017 / QLT-009 / QLT-013 / QLT-014

## 文脈

Issue #5 までの窓は 1 行の固定文字列を描く。エディタの正本＝本文がまだ無い。SPECIFICATION 第 3 節は piece table とメモリマップを土台に挙げ、
CPP-014 は「内部表現は Phase 3 の piece table の ADR で決める」としていた。編集中の状態の見た目は施主が
[キャンバス](https://claude.ai/code/artifact/f42a77d4-30f3-4867-8303-77686e2cf53c)で選んだ（[採用案](../design/2026-09-15-editing-look.md)）。

## 決定

1. **本文の正本は `core::TextBuffer`（piece table）ただ 1 つ**（ARC-001）。original バッファ（読んだファイル。今は空）と add バッファ（入力）の 2 本と、
   piece の列（どのバッファの、どこから、何バイト）で本文を表す。行索引は piece ごとの改行の位置を持ち、行の取得は行番号 → piece の走査で行う（1 万行で十分。
   1 GB の遅延構築とワーカーはメモリマップの縦切りで ADR 0004 に載せる）
2. **内部表現は UTF-8**（CPP-014 の確定）。位置は `Offset`（バイト）・`LineNumber`（1 始まり）・`Column`（code point・1 始まり）の専用型（CPP-001）で、
   境界でだけ相互に変換する。DirectWrite への UTF-16 変換は描く行だけ、描くときに行う。`WM_CHAR` の UTF-16（サロゲートペアを含む）は窓が UTF-8 へ変換して意図にする
3. **undo の単位は「連続した文字入力」。** 移動・削除・貼り付け・改行で区切る。undo の履歴は編集の逆操作（挿入 ↔ 削除）の列で、`EditorState` が所有する（ARC-004）。
   Vim の undo の区切り（挿入モードの出入り）は Vim エンジンの縦切りで同じ履歴に載せる
4. **選択は anchor と caret の 2 つの位置**で、Shift+移動が anchor を固定する。選択中の入力は削除 → 挿入を 1 つの undo 単位にする
5. **クリップボードはポート**（`ClipboardPort`。application が宣言し、adapters/win32 が Unicode テキストで実装。Loupe と同じ）。中核はクリップボードを知らない
6. **描画は見えている行だけ。** 縦スクロールの量は application が所有し（`ScrollState`。上限は core の純関数）、UI はホイール 3 行とキャレット追従の意図を出す。
   折り返しと横スクロールは持たない（長い行は切る）。1 行ごとに DirectWrite の layout を作って描き、キャッシュは QLT-014 の計測で必要が出てから。#291の測定を根拠に、表示中の本文layoutの再利用を[ADR 0069](0069-reuse-visible-body-text-layouts.md)で追加した
7. **編集中の状態の見た目は採用案のとおり。** キャレットは通常モードとINSERT がバー、NORMAL がブロック。選択・検索の当たり・IME の変換中・`:` の行は
   採用案の表（`search` / `ime` のトークンを `Palette` に足して 16 にする）。`:` の行は**案 A**（ステータスバーの左側が置き換わる）
8. 改行は読んだ形を保つ（ARC-009）。新規の本文は CRLF。文字コードはこの縦切りでは UTF-8 だけ（Shift_JIS の判別はファイルの縦切り・D8）
9. **意図は `std::variant` の閉じた和型**（`EditorIntent`。11 の選択肢。方向や操作の種類は閉じた `enum` を持つ型に畳む）で、`EditorController::apply` は `std::visit`。
   写し先が足りなければコンパイルが落ちる。`std::variant` は意味を持つ閉じた和型であり、CPP-006 が禁じる汎用データバッグではない
10. **Esc は窓を閉じない。** 本文が入るようになったので、Issue #5 までの「Esc で終了」は未保存の内容を失う道になる。通常モードでは選択の解除だけ、Vim では NORMAL への遷移（エンジンの縦切りで）
11. **クリックでキャレットを置く**（`PlaceCaret{TextPosition}`。Shift 併用で anchor を固定）。ドラッグ選択はまだ
12. **追補（2026-11-11・#1393・施主決定 D82）: 通常モードで選択があるときの Shift なしの移動は選択の端から動く**（Windows の標準。メモ帳の engine＝RichEdit の実測 `D:\NeNeNib\outputs\20261111\windows-standard-probe.md` §3 を正にする）。
   `←` は選択の先頭、`→` は末尾へ畳むだけで動かない。`↑` `Home` `PgUp` は先頭、`↓` `End` `PgDn` は末尾の画面行から動き、上下の横位置はキャレット（動く端）のもの（狙う横位置があればそれ）。
   折り返しの境目では、キャレットでない側の端は選択を描く側の画面行を起点にする（先頭は後ろの画面行、末尾は前の画面行）。キャレットの側の端はキャレットが描かれている画面行のまま（キャレットの affinity）。
   `←` `→` で畳んだキャレットも同じ側の画面行に留まり（末尾へ畳めば前の画面行の行末・先頭へ畳めば後ろの画面行の行頭）、その後の `↑` `↓` もそこから動く。折り返しの境目の Windows の実測は無く、選択を描いていた側に留まる形は設計席の決め。側を決めるのは `origin_affinity` の 1 つで、畳みと上下の起点が共用する。
   端の行の着地（ADR 0144 決定 11・#1386）はそのまま重なる（末尾が最後の画面行なら `↓` は本文の末尾、先頭が最初の画面行なら `↑` は本文の先頭）。RichEdit は端の行の `↑` `↓` で選択の端へ畳むだけだが、Nib は #1386 に従い本文の先頭 / 末尾へ着く（`↑` も `↓` も・設計席が受けた違い）。
   `Ctrl+Home` `Ctrl+End` は文書の端へ畳む（前からのまま・契約で固定）。`Ctrl+←` `Ctrl+→` はまだキャレットから歩く（端が語の止まりなら留まる形は語の境 #1394 の後）。
   `PgUp` `PgDn` は 1 画面を超える本文の Windows の実測が無く（見えない窓では画面が送られない）、`↑` `↓` と同じ起点にした。
   規則が効くか（通常モード・Shift なし・選択あり）は `moves_from_selection` の 1 つ、起点を決めるのは application の `motion_origin` の 1 か所（`EditorControllerEditing.cpp`）で、上下は `move_visual` が起点の画面行とキャレットの x を読む。Vim のモードと Shift つきは変えない（契約 `--selection-arrows`）

## 強制

- QLT-009: piece table・行索引・undo・位置変換・選択は単体テストで境界（空・先頭・末尾・piece の境界・サロゲートペア・CRLF の途中）を厚く測り、分岐 90% を保つ
- QLT-013: キー入力の実機は `eng/verify-window.py` が `WM_CHAR` / `WM_KEYDOWN` を `PostMessageW` で送って本文と表示値を見る。CI は WARP で描くだけ
- QLT-014: **planned のまま**。1 MB の挿入と行索引が単体テストで 100 ms 未満であることだけを上限として確かめる（ベンチではない）
- ARC-011 / CPP-017: 窓はキーを意図に変えるだけで、本文に触らない（レビュー事項）

## 結果

- 得られるもの: エディタの正本、undo、選択、クリップボード、複数行の描画とスクロール。Vim エンジンと IME とファイルが載る土台
- 失うもの: 1 GB のファイルはまだ開けない（行索引を同期で作る）。折り返し無し。横スクロール無し。1 行ごとの layout 生成は速さの計測前
- 正直に記録しておくこと: piece table の行索引は piece 内の改行の配列で、巨大ファイルの遅延構築は設計に入れていない（ADR 0004 のワーカーの縦切りで）。
  `WM_CHAR` は IME を通らない入力だけで、変換中の表示（採用案の IME）は FR-012 の縦切り。語の区切りは空白だけ（記号の扱いは Vim の語の定義と一緒に）。
  1 打鍵ごとに `Present(1, 0)` で vsync を待つので、まとめて post した入力は 1 フレーム 1 打鍵で流れる（人の速度では問題にならない。描画のまとめ方は速さの縦切り・QLT-014 の検討事項）。
  `std::string_view::find` は MSVC STL の `__std_find_trivial_*`（純関数・ベクトル化された検索）を core の外へ出す。許可シンボルに足した（時刻・OS・スレッドに触れない）。
  手元の計測（`/O2`・使い捨て）: 1 MB の挿入 0.62 ms、1 万行 CRLF の読み込み 1.93 ms、全行の走査 0.77 ms、末尾への 1 万打鍵 3.25 ms、1.22 MB の中央への 1000 挿入 47.5 ms（piece 1002 本）。QLT-014 は planned のまま

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| gap buffer / rope | piece table は original を不変に保ち、メモリマップ（1 GB）と undo と相性がよい。施主の土台の指定でもある |
| 内部表現を UTF-16（DirectWrite に合わせる） | ファイルとクリップボードの往復で変換が要り、`std::string_view` の道具が使えない。描く行だけ UTF-16 にする方が変換量が少ない |
| undo を 1 文字ずつ | 使い勝手が悪く、Vim の区切りとも合わない |
| `WM_KEYDOWN` だけで文字を扱う | 配列とシフト状態の変換を自分で書くことになる。`WM_CHAR` が正典 |
| 折り返しをこの縦切りで | 行索引と桁の意味が変わる。描画の縦切りを分ける |
| `:` の行を案 B（ステータスバーの上） | 施主が案 A を選んだ |

## 関連

- [ADR 0004](0004-ui-thread-plus-one-worker.md)・[ADR 0005](0005-own-vim-engine-verified-against-real-vim.md)・[ADR 0008](0008-adopted-look-tabs-titlebar-statusbar-mica.md)
- SPECIFICATION 第 3 節・FR-002 / FR-008 / FR-013

## 追補（#1395・2026-11-11）: 通常モードの Windows の標準の鍵（施主決定 D82）

通常モード（Vim でないモード）の振る舞いは Windows の標準（メモ帳・ブラウザの入力欄）に合わせる（D82）。既存の意図に鍵を写すだけで済む 5 つを足す。Vim のモードの鍵は変えない（D71: 同じ鍵が Vim で別の意味を持つ所は Vim 優先）。

1. **Ctrl+N は新しいタブ**（メモ帳）。操作の表 `operation_bindings`（ADR 0078）に通常モードだけの行を 1 つ足し、`OperationKey` に `n` を足す。Vim の Ctrl-n には譲らないので Vim では行を置かない。一覧（F1）に見せる鍵は表の最初の行の Ctrl+T のまま。
2. **Ctrl+Shift+Z はやり直す**（ブラウザの入力欄と VS Code）。表の「Shift を見ていなかった今の振る舞いを保つ行」（元に戻す）を、やり直すの行に替えて Ctrl+Y の 2 行の後ろへ置く。見せる鍵は Ctrl+Y のまま。Vim では今までどおり値なし（`u` と Ctrl-r）。
3. **Ctrl+Insert はコピー・Shift+Insert は貼り付け・Shift+Delete は切り取り**。クリップボードの鍵は操作の表の外（Ctrl+C / V / X の写しの道）なので、ui の写し `clipboard_key_for`（`src/ui/win32/ClipboardKeys.hpp`）の 1 本で意図 `ClipboardAction` に写す。規則は Ctrl+C / V / X と同じ: コピーはどのモードでも、貼り付けと切り取りは通常モードと Vim の INSERT だけ（Vim の NORMAL / VISUAL では OS の貼り付けをしない・ADR 0035 の決定 9）。Vim の NORMAL / VISUAL の Shift+Delete は今までどおり何も送らない（ADR 0053 の Delete の節・#1325 のレビュー L2）。修飾なしの Insert と Delete・Ctrl+Delete・Ctrl+Shift の組みは今までの道のまま。この規則は本文の鍵の話で、入力行（`:`・検索・Ctrl+P の面）の Shift+Insert は後続。
   - Vim のモードでも Ctrl+Insert のコピーが効き、Vim の INSERT で Shift+Insert / Shift+Delete が効くのは、Ctrl+C / V / X と同じ規則にそろえる（設計席の決め・2026-11-11）。
   - 選択の無い Shift+Delete は切り取る本文が無いので何もしない。メモ帳の engine（RichEdit・RICHEDIT50W とメモ帳の package の riched20.dll）の実測と同じ（EDIT は右の 1 文字を消し、割れる。実機のクリップボードを別スレッドが握って書けないようにして測った・通し番号は前後で不変・2026-11-11）。
   - 「OS の貼り付けは通常モードと Vim の INSERT だけ」は ui では `takes_os_clipboard_keys` の 1 本で、Ctrl+V / Ctrl+Shift+V を Vim の矩形の鍵として送るかも同じ関数で決める。
4. 窓は鍵を意図に写すだけ（ARC-011）。契約は `nib_window_tests --standard-keys`（写しの関数と表）と `nib_tests` の操作の表の検査。実機のクリップボードには触れない。
5. 順の前提: `press_key` は入力行（`:`・検索・Ctrl+P の面）→ タブの鍵 → 操作の表 → エクスプローラーの焦点 → `clipboard_key_for` → Ctrl の表 → Vim / 通常の表の順に引く。面が開いている間の Ctrl+N は次の候補のまま、入力行の Shift+Delete は 1 文字の削除のまま。古いクリップボードの 3 つの鍵がタブの鍵と操作の表に当たらないことは `--standard-keys` が守る（入力行が先であることは窓の中の順で、試験は無い）。

## 追補（2026-11-11・#1394・施主決定 D82）: 通常モードの語の境と Ctrl+Backspace / Ctrl+Delete

「語の区切りは空白だけ」（結果の節）を通常モードについて改める。Vim のモードは変えない。

1. **語の境は core の `ordinary_word_boundary` の 1 本**。通常モードの Ctrl+← →（Shift つきも・`moved_caret` の `CaretUnit::ordinary`）と、Ctrl+Backspace / Ctrl+Delete が消す範囲の端はこれだけで決める。`CaretUnit::code_point`（Vim のモードの Ctrl+← →）は空白だけの区切りのまま。
2. **規則は Windows のメモ帳の engine の実測に合わせる**。見えない `EDIT`・`RICHEDIT50W`（msftedit）・メモ帳の package の Office RichEdit（`RICHEDIT60W`）を probe のプロセスの中に作って測った（記録 `D:\NeNeNib\outputs\20261111\windows-standard-probe.md`・EDIT は語の区切りが空白で Ctrl+Backspace が無いので、メモ帳と同じ RichEdit を正にした）。語は同じ種類の文字の連なりで、後ろの空白は前の語に付く。Ctrl+→ は語と後ろの空白を越えて次の語の頭へ、行末では改行の前で 1 回止まる。Ctrl+← はその鏡で、行頭からは改行 1 つ、字下げの行頭（前の空白を越えて行頭に着いたとき）は止まらずに前の行末（空の行が続けば全部越える）へ戻る。
3. **文字の種類は Vim の語の分類の表（`vim_character_class` の word）を引き、表を 2 つ作らない**。メモ帳と違う 4 か所だけを読み替える: `_` は記号、Tab は 1 つずつ別の語（空白に数えない）、半角カタカナ（U+FF66〜U+FF9F）は英数字と別の種類、英数字に挟まれた `'` と U+2019 は語の中。幅 0 の文字（結合文字・ZWJ）は前の文字に付く。この 1 文字の単位（`OrdinaryWordBoundary.cpp` の `atom_end` / `atom_begin`）は、既にある `vim_character_end` / `vim_character_start`（Vim の 1 文字・ADR 0053）と `ordinary_character_boundary`（ADR 0065）を使わない: メモ帳は ZWJ（表の幅は unprintable）を前の語に付け（記録 §5 の `a<ZWJ>b`）、Tab と制御文字には何も付けないので、どちらとも違う。語の境はどれも通常の文字境界の上にあり、キャレットを文字の途中に置かない（#1410 のレビュー L2）。
4. **漢字の連なりは 1 語**。メモ帳は漢字を辞書（文脈）で切る（`東|京|都` `日本語　|全角|空白` `飲む`）ので表では写せない。Issue #1394 の「文字の種類の切れ目で止まる」を正にした。照合（`model.py`）は試験の 64 文字列のうち 52 が全位置で一致し、残りは辞書の切れ方・サロゲートの途中・記号の後ろの結合文字・`あ'い` だけ。これは 64 文字列の中の話で、別の probe（記録の probe4）では次の 4 つもメモ帳と違う（Nib は規則のまま・#1410 のレビュー L1）: `々〆〜`（メモ帳は 1 文字ずつ・Nib は 1 語）、ZWSP（メモ帳は 1 つの語・Nib は空白）、`👍🏽 🇯🇵`（メモ帳は 1 語・Nib は別々）、`＄１００`（メモ帳は 1 語・Nib は `＄|１００`）。ハングルと英字の境（`한국어abc`）は測っていない（Nib は `한국어|abc` で切る）。
5. **Ctrl+Backspace / Ctrl+Delete は意図 `DeleteText` の `unit = word`**（`core::DeleteUnit`）。選択が空なら Ctrl+← / Ctrl+→ の着地までを `replace` で消す（undo 1 単位・行頭 / 行末では改行を消す）。選択があれば向きに関わらず選択を消す（Issue の決めた形・Backspace と同じ）。メモ帳の engine は選択を残して選択の始まりから語を消すが、Issue と標準のブラウザの入力欄に合わせた。窓は通常モードでだけ送り（`send_word_deletion` が `word_deletion_for` を引く）、Vim のモードの Ctrl+Backspace / Ctrl+Delete は今までどおり何もしない。Ctrl+Shift+Backspace / Ctrl+Shift+Delete はメモ帳の実測が無いので何もしない（鍵を増やさない・nib_window_tests の `--word-delete` が固定する・#1410 のレビュー L4）。字下げの行頭からの Ctrl+Backspace は Ctrl+← の着地（前の行末）までを消すので、字下げ・改行・その上の空の行を 1 打でまとめて消す（`ab⏎⏎  |cd` → `ab|cd`・undo の 1 単位で戻る・`--ordinary-words` の契約）。ブラウザの入力欄はまず字下げだけを消すかもしれないが測っていない（#1410 のレビュー L5）。
6. ダブルクリックの語の選択は Vim の `iw` の範囲（#1250）のままで、この境を使わない。例: `foo_bar` のダブルクリックは全体を選ぶが、Ctrl+→ は `_` の前で止まる（`foo|_|bar|`）。境を共用したくなったら、この違いを先に決める（#1410 のレビュー L7）。
7. **鍵は操作の表（ADR 0078 の `operation_bindings`）に足さない**。Ctrl+Backspace / Ctrl+Delete は Ctrl+← →・Ctrl+A / C / X / V と同じ文字の編集の鍵で、ui の `press_control_key` が引く（F1 の一覧には出ない）。表の外の鍵は一覧に載せるときにまとめて表へ移す（CLAUDE.md の「コピー・貼り付け・Vim の鍵はまだ表の外」と同じ扱い・#1410 のレビュー L3）。
8. **1 打鍵ごとに行の全体を写さない**（CLAUDE.md の「1 打鍵ごとに呼ばれる所は行の全体を `text_range` で写さない」・#1410 のレビュー M1）。`ordinary_word_boundary` はキャレットの前後の窓（4 KiB）だけを読み、窓の端で code point を割らない（`whole_code_points` と先頭の続きのバイトを落とす）。判断が窓の端に掛かったとき（着いた所の 1 文字が窓の終わりで切れる・前の 1 文字や `'` の隣が窓の外）だけ窓を倍にして読み直すので、答えは行の全体で歩いた答えと同じ。後ろ向きはキャレットの後の 1 code point（`'` の判定）だけを後ろに読む。費用の probe は `ordinary-word-boundary-giant-1line`（16 MiB の 1 行の真ん中で後ろ向きと前向きを 1 回ずつ・窓を出さない）。

検証は `--ordinary-words`（英文・記号・Tab・空白・日本語・混在・結合文字・行をまたぐ・行頭行末・CRLF・Vim のモードの不変・controller の移動と削除と undo・選択があるときの Ctrl+Delete・字下げの行頭の Ctrl+Backspace・固定 seed の本文で 1・2・3・7・64 バイトと既定の窓が行の全体の答えと同じ・窓より長い語）と nib_window_tests の `--word-delete`。fixture は変えない。
