# 品質向上の日（2 日目）— 2026-10-29（設計席 Fable・hide 不在）

hide の指示（朝）「これから 1 日、外回りでデスクにいないので他のセッションは Folio サナ以外は立ち上げない。自由に試験や計測を繰り返して、品質が上がるように調査対応をして欲しい。必要なら web 検索やマルチエージェント・オーケストレーションを自由に使っていい。止めるまで続けて最善を尽くして。試したことや変更したこと等は全てドキュメントとして記録を残して」。前日の同じ日は [quality-day-2026-10-28.md](quality-day-2026-10-28.md)。

設計・判断は Fable の設計席、実装・調査・レビューは Opus の背景の席（ADR 0038 / 0039・依頼書は `D:\NeNeNib\briefs\20261029\`・席の共通の約束は同じ場所の `_common.md`）。merge の裁量（hide の指示 2026-10-15）: 試験・道具・文書・製品の破れの直し・hide が「よし」と言った機能は、独立レビューと CI の後に main に入れる。速さに関わるものは D41。基準値（2026-10-18 採用）は変えない。

## 0. 朝の状態と計画

- origin/main `0703602d`（#1059 まで）。本体の checkout は 33 本遅れていて、前夜の文書 PR と同じ内容の未 commit の文書（29 ファイル・origin/main と同一）が残っていたので `git stash push -u`（捨てない）で退かせて fast-forward した（stash `20261029-duplicate-docs-identical-to-origin-main`・中身は全部 main にあるので後で drop してよい）。open の PR は #593（hold）だけ。作業木 27 本（前日までの実験のまま）。
- `D:\NeNeNib\worktrees\main-replayer` に前夜の試験の残骸 `nib-adapter-files/`（15 KiB・adapters の試験の一時ファイル）が未追跡で残り、`build-release.ps1` が dirty と見るので `D:\NeNeNib\outputs\20261029\main-replayer-nib-adapter-files-moved\` へ移した（捨てていない）。adapters の試験が作業木の直下に一時ファイルを残す件は後で確かめる（§5）。

計画（レーン）:

| レーン | 内容 | 根拠 |
|---|---|---|
| D 速さ | 分割・開く道の port・設定 v4 の後の main を **朝一番の静かな機械で** 8 本のゲート（beta.8 の前に必須・引き継ぎ） | 前日は未計測 |
| A 再現度 | Vim の差分 fuzz の続き seed 21〜30（3,000 件・`--exclude` で `''` と `` ` ` ``・`--known` 781 / 985 / 980）を調査席で回し、新規の種類を仕分ける | 前日 6,900 件で 12 種 → 全部直した。例に基づく fixture の外に差が残る |
| F 機能（hide「よし」の列） | 引き継ぎの順: L0c（LSP の配線）・E3（エクスプローラーの application）・L2b（worker で読む adapter）・Vim の定番 #1037〜#1040 を並列 8 席 | 前日の引き継ぎの再開手順 1 |
| G 設計 | D71 / D72 の ADR（検索・置換・行へ移動 / `ClockPort` / `map` / 印刷）と追補（Ctrl+ホイール・文字コードと改行・右クリック・全面プレビュー）を CI 待ちに書く | 前日の引き継ぎ「先に ADR」 |
| T 言語 | T1a（`ui_texts` の表と移し替え・CNF-016）は F の群が merge した後に 1 席（全ファイルの日本語のリテラルを触るので、同時に走らせると衝突が全部に出る） | 設計席の判断 |

## 1. 速さのゲート（朝・main `0703602d`・Release `D:/NeNeNib/worktrees/main-replayer/build/release-0703602/NeNeNib.exe`・sha256 `9111070C…`）

負荷 14 / 8 / 2%（Folio の codex は軽い）。`python eng/measure-speed.py --check --repetitions 5` → **8 benches checked, 0 regression(s), 0 unmeasurable**（ログ `D:/NeNeNib/outputs/20261029/gate-main-0703602d.log`）。

| ベンチ | 中央値 ms | 前日 `0070859c` |
|---|---|---|
| startup-first-frame | 48.669 | 41.261 |
| startup-window-shown | 36.131 | 31.813 |
| key-to-frame-single | 0.461 | 0.580 |
| key-to-frame-single-long-line | 1.017 | 1.132 |

（残り 4 本はログに。起動の 2 本は許容の内で、朝の 1 回目の計測は OS の暖まりで数 ms 揺れる。1 打鍵は分割の後も速い。）分割（#1034 #1053）・開く道の port（#1054）・設定 v4（#1055）は速さに退行なし → beta.8 の前提の 1 つが済んだ。

## 2. 席と結果（随時）

| # | レーン | 席 | 状態 |
|---|---|---|---|
| #994 L0c | F | L0c | PR #1071（`:set lsp`・寿命 start → initialize → initialized → shutdown → exit → stop・再起動 3 回・tick 30 秒・**pump が `GetMessageW` → `MsgWaitForMultipleObjectsEx`**・`--language-server` 34）。レビュー: pump は handle 0 本で同じ振る舞い・費用は数 µs → 直し 2 件（bind の前に溜まった出来事の合図・failures の戻し）→ **8 本のゲート 0 regression**（負荷 5/4/3%・単打 0.602・16 MiB open 49.7・長い行 1.066）→ 待ち列 |
| #997 E3 | F | E3 | PR #1070（子 #1063・`--explorer` 55・新規 13 ファイル・`EditorControllerExplorer.cpp` 549 行）。席の判断 2 つを設計席が受けた: `FolderPort` は新しい要求で前をやめる（ADR 0062 決定 6）ので application は 1 つずつ頼む（面の券が優先）・5 つ目の意図 `ExplorerRootChosen`。→ 独立レビュー |
| #993 L2b | F | L2b | 着手前に設計の穴で正しく止まった（`SealedBytes::filled` は 1 回の同期の fill で封印）→ 設計席が **A**（`BytesReservation`・ADR 0141 追補 8〜11）→ PR #1075（子 #1074・`Win32DocumentReader` `DocumentReadShelf` `DocumentChunkRead`・追補 1・2・4・`--open-async` 31 → 44・adapters 65 MiB の見本 18 checks）。レビュー: 直し 2 件（取消と失敗で仕事の中でハンドルと予約を手放す・大きさは開いたハンドルの `GetFileSizeEx`）・後続 2 件は ADR 0159 追補 5〜7（IME の変換中と Vim の待ちも留める・Ctrl+Tab は読みの途中のタブを飛ばす）に決めて L2c へ → ゲート → 待ち列 |
| #1037 | F | V1 | PR #1072（fixture 37・`VimJumpList` 固定長 100・実測で Issue と違った点 6 つ・Ctrl+O は **D71 で Vim 優先**（通常モードだけ開く・Vim は Ctrl+Shift+O / F1 / Ctrl+P）・Ctrl+I は届かず Tab で足りる）。レビュー → 直し 3 件（回数の回り込み・再生の中は最初の編集だけ・上限 100 の契約・fuzz から `g;` `g,` を外す）→ 待ち列 |
| #1038 | F | V2 | PR #1076（fixture 72・`VimNumber` の純関数・`nrformats=bin,hex` を oracle と fuzz に・窓は前後 4 KiB を倍に・VISUAL は 1 回の置き換え）。レビュー: `do_addsub` と 1 段ずつ一致・UB 無し → 直し 2 件（`switch` の網羅・記録）→ 待ち列 |
| #1039 | F | V3 | **main `506ad600`**（PR #1068・レビューの直し 3 件の後・CI 緑）。元: PR #1068（fixture 32・`VimLastVisual` と `'^` は `VimMarks`・置くのは `visual_remembered` の 1 か所・実測で Issue と違った点 3 つ: `gi` の後の `.` は `i`・`.` の VISUAL の再生は前の選択を置き換えない（`redoing_visual`）・回数つき INSERT の `'^` は `VimStep.insert_left`）→ 独立レビュー |
| #1040 | F | V4 | PR #1069 → レビューの直し 3 件（`Ctrl-r` は `+` `*` だけクリップボード・矢印で始めより左へ動いてから打つと始めが移る（実測・`stop_arrow`）・`Ctrl-w` は `floor` から）→ 待ち列。元: PR #1069（fixture 43・`VimState.insert_start`（Vim の Insstart_orig）・`Ctrl-r` は pending・実測で Issue と違った点: `Ctrl-u` の 2 段目は行頭で字下げで止まるのは autoindent のときだけ・oracle の DEFAULT_SETTINGS が `backspace=indent,eol,start` を置く）→ 独立レビュー |
| #954 fuzz 21〜30 | A | FZ | 済み（§4）。新規 4 種 → #1064〜#1067 → 席 I1〜I4 |
| #1064 | A → 直し | I1 | PR #1079（fixture 36・`case_span` の 1 か所・実測で Issue と違った点: 2 行目以降の 1 桁目は行の残り全体（`op_tilde` の `dec` が前の行の末尾へ）・1 桁目以外の空は 0 文字で成功）。レビュー: 直し 0 → 待ち列 |
| #1065 | A → 直し | I2 | PR #1077（1 か所・fixture 15・`_` は外す（`nv_lineop` は y で動かさない））。レビュー: 直し 0（記録だけ）→ 待ち列。副産物: fuzz seed 21 で `dip1.ge` の新しい不一致 1 件（未調査・次の fuzz で） |
| #1066 | A → 直し | I3 | PR #1080（fixture 30・`current_block` を Vim のソースで確かめた: 後ろ向きの選択でも探す（Issue の見込みは誤り）・内側が選択より広くないとき old_start の 1 つ前から 1 回だけ）。レビュー → 直し 1 件（閉じの前が空白だけの形で比べる位置を Vim の cursor に）→ 待ち列 |
| #1067 | A → 直し | I4 | PR #1078（fixture 24・`replaced_block_fill` の 1 本・奇数の空白は右端に届いた行だけ（実測）・`VimBlockRange.wish`・左上が切れた矩形の `r` のキャレットも Vim に）。レビュー → 直し 1 件 → 待ち列 |
| #1041 V5a | F | V5a | PR #1084（`ignorecase` / `smartcase`・`VimPattern::parse` の旗 1 つ・`\c` `\C`・`*` `#` は `smartcase` を見ない・fixture 30・設定 v4 の鍵 2 つ）。設計席の判断: **v4 に後から足す鍵は無ければ既定**（ADR 0170 追補・最初の 11 鍵は必須のまま）。レビュー: 直し 0（記録だけ）→ 待ち列 |
| #1041 V5b | F | V5b（#1084 の上に重ねる） | PR #1098（子 #1097・`relativenumber` / `scrolloff`・追従の余白は `first_visible_with_margin` の 1 本・gutter の文字は `gutter_label` の 1 本・`--vim-view` 17・`verify-window --gutter` の節）。設計席が `--gutter` を 2 回（1 回目は試験の側の不良: 面が閉じきる前に撮っていた → 席が待ちを足して pass）。PNG で左寄せの「1」が窓の縁に張り付いていた → 右と同じ余白（20 px）を左にも。レビュー: 両モードで効かせる（設計席の判断）・折り返しの試験 → 直した。H M L と Ctrl-f/b の余白は #1099・前からの中央寄せの切れは #1100 |
| #1058 M1a | F | M1a | PR #1082（子 #1081・記法の表の生成物を `src/core/VimKeyNames.hpp` へ・`KeyMap` `key_map_step` `key_map_flush`・設定 v4 の `map.*` `mapleader`・`--key-map` 673）。席が決定の字面から踏み出した 6 点は Vim と同じ動きで受けて ADR 0176 追補 10〜17 に。レビュー → 直し（保存で同じ鍵になる 2 行の拒否・空白の leader は `<Space>`）→ 待ち列 |
| #1058 M1b | F | M1b（#1082 の上に重ねる） | PR #1096（`press_vim_key` の頭で `typed_vim_key`・待ちは `key_map_pending` と `MapTimeout`・`:nmap` 系の Ex と Ctrl+P の候補 19 行・`--key-map` 733）。設計席の判断 2 つ: **`WorkCompleted` は待ちを保つ**（ADR 0176 決定 4 の設計席の書き誤りを直した）・引数なしの `:nmap` の一覧は面の記号が hide の判断（D29 の系）なので planned のまま。判定は「使う人の操作ではない意図」の一覧で（`keeps_tab_walk` ではない） |
| #993 L2c | F | L2c（#1075 の上に重ねる） | PR #1095（上限 1 GiB / 64 MiB の定数 1 か所・BOM は最初の piece の開始を 3 に・窓の文言・退避の `oversized`・追補 5〜7・任意のベンチ 512 MiB / 1 GiB）。**1 GiB の open 762 ms・private bytes 1.09 倍・512 MiB 486 ms・1.15 倍**（参考値・ADR 0159 の見込み 1.05〜1.3 倍の内）。レビュー → 直し 2 件（退避の読み戻しの上限を符号化後の最大に・歩きで飛ばしたタブが揃っても勝手に切り替えない）|
| #1089 | A → 直し | I5 | PR #1092（Issue の見込みは外れ: 真因は VISUAL の欲しい列（Tab の上で錨より手前なら最初の桁）・fixture 16 + `visual-wanted-` 5）。レビュー → 直し（VISUAL の検索とテキストオブジェクトも同じ 1 本・`curswant` の遅延は実測で差なし）→ 待ち列 |
| #1090 | A → 直し | I6 | PR #1091（`block_column_swapped` を `v_swap_corners` の規則に・fixture 24）。レビュー: 直し 0 → 待ち列 |
| #1093 | A → 直し | I7（依頼書は `brief-I5-1093-…`） | 夕方の fuzz 31〜40 で `Ctrl-u` と `.` を含む 6 件が同じ系と分かり（§4c）、席を出した → PR #1106（回数の記録を本文から打鍵の列に替え Esc で残りの周を写しの上で流し直す `replayed_insert`・`VimReplaceRange.boundary`・`EditHistory::absorbed` を「頭より前から入れた本文の中までの置き換え」に広げる（設計席が受けた・ADR 0015 決定 5 の帰結）・fixture 27・5 seed で新規 0）。レビュー: 止める 0・直してから 3（`keys` は `VimRecordedKeys` に・何も消せない鍵の述語を 1 か所に・`replayed_key` の既定分岐を `std::visit` に）・後続 7 → 同じ席が直した（HEAD `0bf089de`・既定実行は同じ 2,328,508 pass・`--vim-open-lines` が 704 → 709 と席が報告したが総数は同じなので選択の差とみて CI に任せた）→ main の #1088 に乗せると矩形の INSERT の回数の記録（本文）と意味が衝突してビルドが落ちた → 同じ席が `VimRecordedKeys::from({})` に合わせ、矩形の回数の周に消す鍵が混ざるときの写し `first_line_growth`（Vim の `op_insert`）を足し、設計席の求めで Vim の値を fixture `insert-count-control-block-` 5 件に（HEAD `8b3ff35e`・既定実行 2,329,476 pass）→ 差分の 2 回目のレビュー: 止める 0・直してから 0・後続 6（#1107）→ 待ち列（#1084 の後）→ #1072 の `control_o` を消す鍵の `switch` が扱わずビルドが落ちた（CPP-002 が働いた）→ 同じ席が足した（HEAD `9c9caf70`・既定実行 2,330,186 pass）。列は手元ビルドが落ちると push しないので origin は乗せ替え前のままで、席の通常の push が拒まれて止まった（正しい動き）→ 設計席が `--force-with-lease` で上げ、列 23 の後ろへ。fuzz の 5 seed を設計席が最終の HEAD で回し直した: #1093 の系は 0・新規 1（seed 37・矩形の `C` で左端が Tab の途中・#1042 の `C` の端の扱い・**#1108**・#1106 の merge 後に席 I9 を出した（20:45）→ PR #1110（各行の消す範囲と埋め方は前から `block_prep` と一致・違ったのは 1 行目で打ち始める位置だけ（Vim の `check_cursor_col` と `op_change` の `inc_cursor`）・`change_entry` の 1 か所・fixture 20・修正前に落ちたのは 1 件・seed 37 の残り 1 件 `Vgg~c$` は既存の差で席が **#1109** を起票）→ レビュー: 止める 0・直してから 1（ADR の端の表の括弧書き・設計席が `I` `A` の古い行も直した・HEAD `0cb40b15`）・後続 3（最後の編集がビルドより後 → 手元ビルド付きの列で受ける）→ 待ち列（#1098 の後・chain2）） |
| #1104 | A → 直し | I8（依頼書は `brief-I6-1104-…`） | 矩形の `u` `U` `~` で右端にまたがる全角（fuzz seed 33・§4c）を起票 → PR #1105（`cased_block_edits` は `VimBlockLine.inside` だけを変える 1 か所・fixture 12・`block-` 214 件・seed 33 の残りは #1093 の系 1 件）。Issue 本文の最小例の空白の転記ずれは席が見つけ、Issue にコメント。レビュー: 止める 0・直してから 1（ADR の表の決定の番号・設計席が直した）・後続 4 → 待ち列（#1072 の後） |
| #1109 | A → 直し | I10（依頼書 `brief-I10-1109-…`） | 席 I9 が seed 37 で見つけた既存の差（`Vgg~c$`・行単位 VISUAL の `~` の後のキャレットの疑い）。席を出した（21:05）→ PR #1111（推測は外れ: 規則は「`V` を打った端が先頭行なら行頭・キャレットだけが先頭行ならその位置・`.` の再生は `.` を打った位置」・fixture 19・`visual_restore` の 1 本に寄せて `visual_case_changed` を消した・seed 37 不一致 0）。`.` の再生の後の `u` の戻り先も錨になる点は Vim の redo 分岐に沿うとして設計席が受けた（oracle では測れない）。副産物: main で `--vim-visual-wanted` の件数の表明が 17 のまま（22 件）→ **#1112**（設計席が直した: PR #1114 を作ったが、#1111 の乗せ替えで列の数え直しが同じ 1 行を直して #1111 に入り、#1114 は空で閉じた・`--vim-visual-wanted` 248 pass・resolver に数え直しを足した）→ レビュー: 止める 0・直してから 1（ADR の追補の効く先の列挙）・後続 3 のうち `.` の後の `u` の戻り先の契約と ADR 0052 の参照も同じ席が直した（HEAD `18ed7760`・契約 `verify_undo_caret_after_visual_line_dot`・分岐を外すと 6 件落ちることを確認）→ 待ち列（#1110 の後） |
| #1073 | 門 | S3 | `EditorWindow.cpp` 2,967 行（CNF-014 まで 33 行）→ ADR 0158 追補 7〜9（6 翻訳単位）と Issue・依頼書 S3（E3 / L0c の merge の後）。#1102 が入って `EditorWindow.cpp` を触る open の PR が無くなった 21:10 に S3 の席を出した（main の `EditorWindow.cpp` は 2,980 行・門まで 20 行）→ PR #1113（中核 309・Lifecycle 694・Pointer 543・Keys 681・Operations 528・Ime 324・Detail 19/30・member 127（表の 118 に無い 12 本は呼び手で: Lifecycle 9・Operations 2・中核 1）・本文不変 bad 0・移動以外 +62/−2・`protected-diff` 0・PNG 54 枚一致・checks 前後同じ）→ レビュー: 止める 0・直してから 0・後続 2（未使用の include 3 つは次に同じファイルを触る PR で・CLAUDE.md の `work_message` の所在は設計席が直した）→ 待ち列（#1114 の後）。ADR 0158 の追補に結果の項 10 を足した |

## 3. 設計（CI 待ちに書いたもの）

- ADR 0174（通常モードの検索・置換・行へ移動は Ex の入力行と Vim の照合器を共用・#1046）・ADR 0175（`ClockPort`・#1050）・ADR 0176（`map` は設定 v4 の行・入口の表 1 つ・#1058）・ADR 0177（印刷は port 1 本・XPS の print control・#1047）。全部 D71 / D72 の施主決定の範囲の中で、仕様の D 番号と FR を読み直してから書いた。
- ADR 0141 の追補 8〜11（`BytesReservation`・L2b の穴）。

## 4. Vim の差分 fuzz（seed 21〜30・3,000 件・main `0703602d` の再生器）

調査席 FZ（07:24〜07:47・23 分・負荷 6 / 13 / 20%・速さの計測の終わりを待ってから）。`--exclude` で `''` と `` ` ` `` を外し、`--known 781:~,g~,gu,gU,V --known 985:w,b,e,W,B,E`（`--known 980:dd,c$` は `c$` が語の名前でないので道具が受けず外した・#980 に当たる件は手で見て 0）。

| 生成 | 拒否 | 一致 | 不一致 | 新規 |
|---:|---:|---:|---:|---:|
| 3000 | 209（E353 186・E348 16・E486 4・ほか 3） | 2784 | 7 | **7（4 種）** |

道具が #781 に数えた 4 件は既知ではなかった（ASCII の形・#781 は閉じている）→ 種類 A。4 種を Issue にした（原稿は席・`D:/NeNeNib/outputs/20261029/vim-fuzz-seeds21-30/issue-*.md`・報告 `report.md`）:

| Issue | 種類 | 例 |
|---|---|---|
| #1064 | 空の範囲の `g~` `gu` `gU` は Vim は実行（`cpo-E` 無し・カーソルの下の 1 文字・`.` に残る・`exclusive-linewise` の空の行はキャレットを先頭へ） | `x1` に `gUh` → Vim `X1`・Nib 不変 |
| #1065 | 行単位の yank の移動が同じ行に着くときのキャレットは (行, 桁) の早い方（#965 の広げ） | `foofoo? a` に `$ygg` → Vim (1,1)・Nib (1,9) |
| #1066 | 複数行の VISUAL の `i[` `i(` `ib` `iB` `i<` で Nib は上の行の先のブロックを選ぶ（Vim は失敗・#967 の内側の版・条件は `current_block` で確かめる） | `e [sq]⏎b` に `Vji[<Esc>` → Vim (2,1)・Nib (1,5) |
| #1067 | 矩形 VISUAL の `r` に全角は矩形の幅を全角で埋める（`w / 2` 個と奇数なら空白） | `abcd` に `<C-v>lr語` → Vim `語cd`・Nib `語語cd` |

直しの席（1 Issue 1 席・先に oracle で測る）: I2（#1065）と I4（#1067）は昼前に出した。I1（#1064）と I3（#1066）は Vim の席が空いてから。前日の 12 種に続き、fixture 2,300 件超が全部通る状態で見つかった差。不一致の率は 7 / 2791 = 0.25%（前日の seed 13〜20 は 4 / 2220）。

残り: I2 の席が seed 21 で見た `dip1.ge`（`fuzz-21-0168`）は、1 段落の本文を `dip` で空にした後の `.` で、Vim は空のバッファでレジスタを書かず Nib は `⏎` を書く差。#980（`ML_EMPTY`・`dd` `c$`）と同じ根なので #980 にコメントで足した（新しい Issue は起こさない）。seed 14 の `fuzz-14-0103` は #1093。

## 4a-0. 午後のゲート

| 対象 | 負荷 | 結果 | 主な中央値 |
|---|---|---|---|
| #1071（pump の変更） | 5/4/3% | 0 regression | 単打 0.602・16 MiB open 49.7・長い行 1.066 |
| #1075（worker の読み・`BytesReservation`） | 4/4/1% | 0 regression | 単打 0.608・16 MiB open 49.2 |
| #1095（上限 1 GiB・BOM のずらし） | 14/9/11% | 0 regression | 単打 0.576・16 MiB open 49.3・長い行 1.086 |
| #1096（`map` の入口・表が空の既定） | 2/5/8%（計測の途中で席の Debug ビルドが始まった可能性） | 0 regression | 単打 0.555・連打 2.26・16 MiB open 61.2（min 48.9・揺れ）・長い行 0.997 |

| main `e7628a5c`（夜・#1073 の分割まで入った後・22:55） | 1/0% | **0 regression** | 起動 41.8・窓 32.6・単打 0.605・連打 2.69・16 MiB open 49.8・16 MiB 連打 2.88・面 1.03・長い行 1.10（ログ `D:/NeNeNib/outputs/20261029/gate-main-e7628a5c.log`・Release `main-replayer/build/release-e7628a5`・試用の exe はこれ） |

## 4a. 昼の merge と窓の検査

- merge の列（1 本ずつ・乗せ替え → 衝突を自動で解いたときだけ手元の Debug のビルドと既定実行 → Ready → CI → squash）: #1068 → #1077 → #1069（`VimState.hpp` で #1068 と衝突・設計席が解いた）→ #1079 → #1070（CI が 25 分かかったので古い列を止めて同じ run で引き継ぎ）→ #1076（`VimKeyNames.hpp` の生成物・`KEY_TABLE`・鍵の表・`VimAction`・`VimStepInsert` / `Recording` / `Visual`・fuzz の 9 ファイルで衝突 → 設計席が解いた。解いた後の手元のビルドで 2 回落ちた: 行の閉じ括弧の欠け（`}}};` を 4 文字消した）と `NibTests.cpp` の 2 表の数（git が行は自動で併合するが大きさは数え直しが要る）→ 直して通した）。
- 教訓: 乗せ替えの衝突を手で解いたときは、必ず手元で Debug のビルドと既定実行を回す（`merge-chain2.sh`）。`NibTests.cpp` の 2 表の大きさは「行を数えて書き直す」を鎖の道具に入れるとよい（次回）。
- 窓の検査: main `86ab08f7`（E3・#1064・#1077・#1069・#1068 の後）の Release で `eng/verify-window.py` の全節 **pass**（exit 0・ログ `D:/NeNeNib/outputs/20261029/verify-window-main-noon.log`）。


夕方の列（#1092 の後）: #1092 `3cfb0d8f` → #1088 `6a028d0f`（`VimBlockRange.cpp` が #1089 の `landed_column` と当たり席が解いた・`block_caret` の引数は #1067 の形に）→ #1072（`CMakeLists.txt` のソースの行の和・`<C-i>` の件は §5）→ #1105 `54e90076` → #1084 `15858ac4` → #1095 `02a04da8` → #1096 `8606a589` → #1106 `d7cd0d1b` → #1102 `fcc2db46`（`EditorIntent.hpp` の意図の和型に両側が 1 型ずつ足した衝突・設計席が両方を残して解き、手元の Debug ビルドと既定実行）→ #1098 `7518c4ff`（親 #1041 を閉じた・`rebase --onto origin/main 5222eb39` で `SettingsCodec.cpp` `EditorSettings.cpp` `ExResult.cpp` が #1084 の設定の鍵と `:set` の分岐で当たり、席に戻した。`CMakeLists.txt` と `NibTests.cpp` は道具）→ #1110 `0785dea1`（chain2・手元ビルド付き）→ #1111 `2f6f032f`（この乗せ替えで列の新しい数え直しが `visual-wanted-` の 22 を commit し、#1112 の直しが #1111 の squash に入った。PR #1114 は空になって閉じ、#1112 は閉じた）→ #1113。席の解き方: v4 の鍵は `…ignorecase, smartcase, relativenumber, scrolloff` の順で 16 個・`set_option` が分岐 11 で tidy の複雑度に掛かったので `rnu` `so` を `view_option` の 1 本に（閾値は変えない）・main の `verify_settings_maps` の期待に 2 行を足した（HEAD `9fc45811`・既定実行 2,329,954・窓の試験 4,201・adapters 2 本 pass）。設計席が `verify-window.py --gutter` を 1 回: exit 0（本文の画素 0・余白 20。朝の失敗は席の「面が閉じて描き直されるまで待つ」で消えた）。席は「道具の結果で書式文字列の `
` が改行に化けた」と報告したが、化けたのは `SettingsCodec.cpp` で、道具はそのファイルに触れていない（席に残した 3 本の 1 つ）。席自身の編集の跡とみる（席の報告は検証した数字以外は裏を取る）。 その後 main が #1096 まで進んで `EditorControllerVim.cpp` の include の並びで再び衝突 → 同じ席が乗せ替え直した（HEAD `f86cecaf`・既定実行 2,330,390・窓の試験 4,203）→ #1102 の後の列。

## 4b. hide との外からのやり取り（昼）

- 外部の講評（別のアカウントの Claude）「`gj` `gk`・`s S R`・大文字マーク・autoindent は該当 Issue なし」→ 照合: `gj` `gk` は main（誤り）・`s S R` は #768（Issue なしは誤り）・大文字マーク（#1085 起票）・autoindent（#1086 起票）は本当に無かった。#768 は `R` `s` `S` だけに狭めた。
- hide「ドキュメントも現状に合わせて棚卸して整理する必要あるかも」→ 計画 `docs/plans/2026-10-29-docs-inventory.md`（表から生成する一覧・経緯と現状の分離・SPEC の棚卸・着手は hide の合図）。
- 講評「右クリックで編集に対応していない点がメモ帳の方が優位」→ Windows のエクスプローラーへの登録 #1087 → hide「入れて、Microsoft Store リリース段階くらいのタイミング」= **D73 / FR-033**（段 1 は実装予定・段 2 は Store の段階）。
- hide「CSV 表示モードについてはどう思う？」→ 設計席の案は 3 段（色分け・表のプレビュー・その場で揃えて編集）で 1・2 は合う → hide「**保留**。CSV エディタに任せる方がいい」。Issue は立てない。

## 4c. Vim の差分 fuzz の続き（seed 31〜40・3,000 件・main `4040e1c5` の再生器・夕方）

設計席が回した（merge の列の CI 待ちの間・速さの計測ではないので負荷は見ない）。生成器は `generator=3`（INSERT の `Ctrl-w` `Ctrl-u` を含む）。`--exclude` で `''` と `` ` ` ``・`--known` は 781 / 985 / 980（`c$` は語の表から消えていたので `dd` だけ）。出力 `D:/NeNeNib/outputs/20261029/vim-fuzz-seeds31-40`。

| 生成 | 拒否 | 一致 | 不一致 | 既知 | 新規 |
|---:|---:|---:|---:|---:|---:|
| 3000 | 210 | 2782 | 8 | 1（#980） | **7（2 種）** |

| 種類 | 件 | 行き先 |
|---|---:|---|
| 回数つき INSERT と `.` の中の `Ctrl-w` `Ctrl-u` の反復（Vim は打鍵そのものを流し直すので 2 回目の `Ctrl-u` が 1 回目の本文を消し、行頭なら `bs=eol` で前の行に結合する） | 6 | **#1093** にコメントで 6 件の最小の再現を足した（fixture の入力に） |
| 矩形 VISUAL の `u` `U` `~` で右端にまたがる全角（Vim の `block_prep` は削除でない operator では `textlen` に入れない） | 1 | **#1104**（新規・低） |

残りの不一致率 0.27%（前回 0.25%）。新しい語（INSERT の `Ctrl-w` `Ctrl-u`）が入った分が #1093 に集まっていて、それを除けば 1 件。再生器の自己検査 `--from-fixtures 50` は ok 50。seed 21 の `dip1.ge` は #980 の系（上の §4）。

## 5. 今日分かったこと・次に効くこと

- 本体の checkout に前夜の文書の写しが未 commit で残ると `build-release.ps1` が断る。`git stash push -u` は捨てないので auto mode の分類器を通る（`checkout --` と `clean` は止められる）。
- adapters の試験（`nib_adapter_tests`）が作業木の直下に `nib-adapter-files/` を残す（確かめる: どの試験か・一時フォルダに書く形に直すか）。

- 乗せ替えの道具 `resolve-vim-conflicts.py` の穴 3 つ（夕方・#1092 と #1088 で）: (1) 接頭辞が 2 つ以上のとき `--regenerate --only` を接頭辞ごとに呼ぶと、2 回目が 1 回目の fixture を「reuse 元の main に無い」と断る → 1 回の呼び出しに `--only` を並べる。(2) 記法の表の生成物が #1082 で `src/core/VimKeyNames.hpp` に移ったのに `tests/vim/` を `git add` していた → 存在する方だけ。(3) `fixtures.json` の読みが空で落ちた（原因は未特定）→ 数え直しだけ諦めて残りを続ける。コードの衝突（#1088 の `VimBlockRange.cpp` が #1089 の `landed_column` と当たった）は席に戻した。

- #1072（ジャンプの列）の乗せ替えで、手元の既定実行が 1 件落ちた: 枝が記法の表に `<C-i>` と `<Tab>` を同じ鍵 0x09 で 2 行足していて、main の M1a（#1081）の契約「表の名前は鍵と 1 対 1 に往復する」（`:nmap` の一覧の表示名が 1 つに決まるため）に当たった。設計席が `<Tab>` の 1 名に決めて直した（Vim の表示名も `<Tab>`・fixture 5 件の鍵を書き換えて `jumplist-` 28 件を作り直し・fuzz の語も `<Tab>`・ADR 0152 の追補に 1 文）。**記法の表に同じ鍵の別名を足さない**（CLAUDE.md §2 の記法の項に足す）。
- `CMakeLists.txt` のソースの列は 1 行に複数並ぶので、両側が足した衝突は token の和で解ける（resolver に足した・枝側だけの token は枝側での直前の token の後ろに差す）。

- 待ち付きの列 `wait-then-queue.sh` が自分の最初の行（「前の列が終わった: queue done」）を終わりの印と誤って読み、#1072 の CI 中に #1106 を並行で流した（17:52）。別の作業木なので #1072 には影響なし。#1106 は乗せ替えに衝突が無いのにビルドが落ちた（main の #1088 が矩形の INSERT の回数の記録を本文で作る・本件は打鍵の列に替えた・意味の衝突）→ 同じ席に戻した。印は行頭だけを見る形に直した。

- #1102 の乗せ替えで `NibTests.cpp` の 2 表は衝突せず git が行を自動で合わせたが、`std::array` の大きさは古いままでビルドが落ちた（衝突が無いと列は手元ビルドを省くので CI まで気づかない形）。`merge-chain4.sh` に「衝突が無くても表の大きさを数え直し、差があれば commit して手元ビルドへ」を足した。

- **走っている bash のスクリプトを編集しない**（20:20）。bash はスクリプトを実行しながら読むので、`merge-chain4.sh` に行を足した瞬間に走行中の #1106 の列が途中の行で構文エラーになり、手元ビルドを飛ばして push と merge-pr まで進んだ（merge-pr は生き残って CI 緑で squash した・main `d7cd0d1b`。乗せ替え後の手元ビルドは無いが CI が同じ構成で建てて試験している）。道具を直すときは列が止まっている間に、または写しを別名で作って次の列から使う。#1106 は `merge-chain2.sh`（手元ビルド付き）で流し直した。

- 設計席の小さな道具（heredoc の Python）は backslash を文字列に書かず `chr(92)` で作り、`python -X utf8 -` で回す。Windows の `python -` は stdin の script の読みで日本語と `\N` を含む文字列が「unicodeescape」や空振りの一致で落ちる（夕方に 4 回当たった・`D:\NeNeNib` の `\N` も同じ）。

## 6. 置き場

`D:/NeNeNib/outputs/20261029/`（計測のログ・fuzz の結果・レビュー）、`D:/NeNeNib/briefs/20261029/`（依頼書）。
