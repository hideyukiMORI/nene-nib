# 品質向上の日 — 2026-10-28（設計席 Fable・hide 不在）

hide の指示（朝）「これから 1 日、外回りでデスクにいないので他のセッションのサナやリナを立ち上げない。自由に試験や計測を繰り返して、品質が上がるように調査対応をして欲しい。必要なら web 検索やマルチエージェント・オーケストレーションを自由に使っていい。止めるまで続けて最善を尽くして。試したことや変更したこと等は全てドキュメントとして記録を残して」。

設計・判断は Fable の設計席、実装・調査・レビューは Opus の背景の席（ADR 0038 / 0039・依頼書は `D:\NeNeNib\briefs\20261028\`・席の共通の約束は同じ場所の `_common.md`）。製品の性能改善は D41（独立レビュー・正式ゲート・CI・hide の試用）で merge は hide の試用の後。試験・道具・文書・製品の破れの直し・hide が頼んだ小さな機能は、独立レビューと CI の後に merge の裁量（hide の指示 2026-10-15）で main に入れる。機械は hide の実機（i9-10850K / RTX 3090 / 120 dpi）。基準値（2026-10-18 採用）は変えない。

## 0. 朝の状態と計画

- main `b51684f2`（origin と一致）。open の PR は #593（draft・hold）だけ。作業木 27 本。役目を終えた `perf-stack-20261027` と `947-command-match`（前の席の途中の状態・未 push 0）は hide が権限を変えた後に外した（auto mode の分類器は `worktree remove` を「元に戻せない破壊」として止める）。前日までの実験の作業木は残したまま。
- 前日の引き継ぎの「hide に決めてもらうこと」（#895 `SW_SHOWNOACTIVATE`・長い行の基準値の採り直し・beta.8 の時期）は今日は触らない。

計画（レーン）:

| レーン | 内容 | 根拠 |
|---|---|---|
| A 再現度 | **Vim の差分 fuzz**: ランダムな本文と鍵列を本物の Vim 9.1 に流して Nib と突き合わせる道具（#954・`eng/vim-fuzz.py` と再生器 `nib_vim_replay`）。見つけた違いを Issue にして直す | fixture 2315 件は人が書いた例。#847 #793 #656 は全部 hide の試用で見つかった |
| B 安定性 | 性質の試験の続き: #947 Ctrl+P の絞り込み（15 本目・前日の依頼書）・#955 通常モードの編集の意図のランダムな列（16 本目） | 前日の 14 本が製品の破れを 4 件見つけた |
| C 試用で見つかった小さな直し | #849 Ctrl+O の絞り込み・#847 Ex の `:N`・#737 面のホイールの累積・#848 `clipboard=unnamed`（hide「足しておいて」） | 前日の引き継ぎ「Opus で進めてよいもの」 |
| D 速さ | 静かな窓で main の 8 本のゲートを 1 回（今日の参照値）。EndDraw 0.19 ms の命令の数への比例の実験・タイトルバーの bitmap は席の空きで | 前日の残り |
| E 道具の確認 | `eng/verify-window.py` の既定実行を main で 1 回（前日 #937 #940 の後の全節 pass の再確認） | — |

## 1. 席と結果（随時）

| # | レーン | 席 | 状態 |
|---|---|---|---|
| #947 | B | F19（2 回目の席・前の席は hide が止めた） | **main** `d2702b0e`・PR #964（scope `--command-match-property`・7865 checks・50 倍 373,187 checks で破れ 0・製品不変） |
| #954 | A | A1 | **main** `90ac3f60`・PR #958（`eng/vim-fuzz.py`・再生器 `nib_vim_replay`・既存 fixture 50 件で ok 50）。seed 1〜3 の 900 件: 拒否 125（`-es` の Vim は `:normal!` の中のエラーで終了コード 1）・一致 736・不一致 39 → 原因 9 種（下の §2） |
| #962 | A → 直し | I1 | PR #969（`VimMarks.previous_context` の既定値を 1 行目の桁 0 に・ADR 0152 追補・`--vim-marks` 969 checks・既定実行 2,315,347 checks）。レビュー OK → **main** `b5e20896` |
| #960 | A → 直し | I3 | PR #973（Vim の実測: 空で書くのは `C` `c$` `cg_` だけ・`D` `d$` `c0` は書かない・空の本文の `p` は失敗しない。`VimEmptyChange` の印は行末への移動だけが立て `register_after` の 1 か所が読む・fixture 11 件・ADR 0050 追補）。限界: 全く空のバッファの `c$` は Vim は書かない（Nib は空の 1 行と区別できない・ADR に記載 → #980）。**main** `b4a04f9f` |
| #965 | A → 直し | I5 | PR #975（上へ動いた行単位の y は着地へ、の規則を #411 の 6 移動から行単位の全移動に広げた・fixture 12 件・ADR 0015 追補）。副産物: 移動 `_` が Nib に無い → **#976**（未起票だった機能）。**main** `adab76eb` |
| #966 | A → 直し | I6 | PR #974（`line_end_landing` を `landing_of` の 1 本に・最終行で N ≥ 2 は `not_moved`・fixture 11 件・ADR 0015 決定 12）。**main** `3c001f7c` |
| #959 | A → 直し | I2 | PR #981（`cased_block`・各行の範囲は `r` と同じ・undo 1 回・キャレットは左上・fixture 10 件（短い行は飛ばす・`$`・Tab・`.`）・ADR 0035 追補）。席の気づき: 1 行目が Tab で始まる本文の `j` は Vim が欲しい列を Tab の終わりから数える（Nib が合うかは未確認・fuzz 1800 件では出ていない）。**main** `07860364` |
| #967 | A → 直し | I7 | PR #982（`block_search_start`: 選択が 2 文字以上なら前の端から探し、そこにある開き括弧は数えない・始点を囲むブロックが選択より広くなければ 1 つ外・囲むものが無ければ**前方の次のブロック**（依頼書の見込み「外なら失敗」は誤り）・fixture 23 件・直す前は 28 checks 落ちた・ADR 0031 追補）。副産物: fuzz-2-0056 の真因は `.` の `c%` の再生（Vim は `%` が括弧を見つけず失敗）→ **#983**。**main** `1da755bf` |
| #976 | A → 機能 | I11 | PR #986（移動 `_`: `VimMotion` と `VimAction` に 1 値・表に 1 行・着地は `g_` と共用の `counted_line_landing`・範囲は `+` と同じ `lines_below`・fixture 16 件・fuzz の語に `_`・ADR 0015 追補・乗せ替えで決定 13 に）。**main** `199da714` |
| #983 | A → 直し | I12 | PR #987（真因は `%` でなく `.`: 再生の中の失敗で残りの再生は捨てていたが `.` の鍵そのものが成功を返し、後ろの鍵が流れていた → controller の `perform(VimReplay)` が打ち切りの失敗を `replay_failure_` に置き `step_failure` が返す・`@` も同じ・fixture 10 件（直す前は 5 checks 落ちた）・ADR 0030 / 0152 追補）。**main** `0e6a4814` |
| #781 | A → 機能 | I13 | PR #989（`~` `g~` `gu` `gU` と VISUAL の `~ u U` を非 ASCII に。表は `eng/case-mapping.py` が Python の `unicodedata`（Unicode 15.0）から生成した `CaseMappingTable.hpp`（CNF-010 で全文一致・Vim のソースの表は写さない）・`VimCase` の純関数 3 本・fixture 15 件・scope `--vim-case` 126 checks）。Vim との差は全数比較で 28 字（`İ`・ypogegrammeni のギリシャ）と `ß → SS` → **#990**（後続・低）。**main** `7a72cc19`（乗せ替えの表の壊れを CI が見つけ、直して再 CI） |
| #988 | A → 直し | I14 | PR #991（`"_p` `"_P` は何もせず成功・Vim の実測で `.` の記録には載り undo の単位は作らない → 既存の `VimInsertBlock` の空の列で表す（新しい効果を足さない席の判断を採る）・fixture 9 件・ADR 0050 追補）。**main** `896e15b8` |
| #979 | A → 直し | I15 | PR #996（語の範囲 `iw` `iW` `aw` `aW` にだけ `writes_empty` の印・括弧の空の範囲 `ci(` は Vim も書かない・fixture 8 件・ADR 0050 追補）。**main** `ba67ff9c` |
| #994 → #1010 | LSP L0a | L0a | PR #1009（core の JSON（parse / write・性質 6 checks・50 倍 2.7 s）・`lsp_frames`・JSON-RPC の対応・位置の写し utf-8 / utf-16・新規 24 ファイル・allowlist に `__std_search_1` と Ryu の表 4 つ（`to_chars` の double・純粋なデータ）・既定実行 2,323,918 checks）。独立レビュー R 7（ADR 0160 が main に無い → 文書の PR で先に入れる・2^53 の境・`-0`・`1e-400`・位置の境目・末尾の改行・試験の境目）→ 直し済み（`--json` 171・`--lsp-messages` 118）→ **main** `7bb06c56` |
| #994 → #1020 | LSP L0b | L0b | PR #1021（`LanguageServerPort`（4 つ・`HANDLE` は出さない）・`Win32LanguageServer`（子プロセス・Job Object・OVERLAPPED のパイプ 2 本・継承は 3 つ・ADR 0126 の取消と完了の分離）・替え玉のサーバー exe `nib_fake_language_server`・`nib_language_server_tests`・新規 27 本）。独立レビュー R 10（`stop` が UI スレッドで 2 秒待つ → 退く子を pump の配列で待ち `expire_retired()` を tick で・絶対パスだけ・孫の継承は `TerminateJobObject`・書きの取消の試験・event の列を捨てる・fail-fast・`cannot_start`）→ ADR 0160 追補 → 直し済み（52 checks × 3 回・0.2 s）→ **main**（#1021） |
| #1023 #1024 | 門 | F20 | PR #1029（`VimStepSource = variant<VimKey, ExLineRequest>` で `std::function` を外す・symbols は Debug / Release とも 1 → 0・CI の `build-and-test` に symbols.py の step（File API の問いを Release の configure の前に置く）・`test_verification_policy` に門の試験・ADR 0136 追補）。**main** `8175b0ee`（本文の `Closes` 2 行が GIT-001 で拒まれ `Refs` に直して merge・#1024 は手で閉じた） |
| #1027 | A → 直し | I16 | PR #1031（`insert_line_start_landing`・空白だけの行の `I` は行末・fixture 9 件・ADR 0028 追補）。副産物: `gI` が無い → #1032。**main** `0070859c` |
| #1028 | A → 調査 | I17 | PR #1030（Nib のバグではない・oracle の起点の差・fixture 17 件を `m'` の前置きで・ADR 0152 の追補に 1 行）。**main** `79ad43c1` |
| #961 | A → 直し | I4 | PR #984（真因は語の分類の表: 全角の数字 U+FF10〜FF19 が記号に分類されていた → Vim の `utf_class_tab` と同じ 4 行に・語が無いときの非空白の文字列 `vim_string_at`・特殊文字の escape・後ろ向きの `\?` は文字・fixture 17 件・ADR 0032 追補）。表の直しは `w` `b` `e` `iw` `\<` にも効く（fixture 2315 件は不変）。副産物: 表に無い記号（U+2190 など）が語に落ちる → **#985**。**main** `b39fe040` |
| #963 | A → 道具 | A2 | PR #978（生成器 2: `''` `n` `"Np` `'a` は前の語の後だけ・`--exclude` `--known`・拒否の内訳（`-V1` でもう 1 回走らせて E 行を取る）・`--seeds`）。seed 4〜6 の 900 件: 拒否 72（8%・69 件は空のレジスタの `p` の E353）・一致 813・既知 10・**新規 5** → #979（空行の `ciW` も空で書く・#960 の広げ）・#980（全く空のバッファの `dd`・ML_EMPTY・後続）・`V…~` の全角は #781。**main** `3a115458` |
| #849 | C | C1 | PR #957（core の `listed_language_extensions()` / `file_dialog_pattern()`・重複と `txt` の在りは `static_assert`・ui は「対応するファイル」「すべてのファイル」で既定は後者・ADR 0154 追補・`--language` 186 checks・symbols 0）。設計席のレビュー OK → **main** `004c97e9` |
| #847 | C | C2 | PR #972（`ExLineRequest`・engine の `vim_line_jumped` は `{N}G` の着地 `landed_step` だけを通り `''` は置かない・fixture 5 件）。席は「Vim の `:N` は `''` を置かない」で正しく止まり、設計席が Vim に合わせる判断（Issue の見込みが誤り）。独立レビュー R 3（コメントの位置・字下げの行への fixture・折り返しのコメント）→ 直し済み → **main** `9b7c1b81` |
| #737 | C | C3 | PR #956（`wheel_turned` を面でも通す・余りは面の欄 1 つ・ADR 0150 追補・`--wheel` 20 checks）。設計席のレビュー OK → **main** `a871eec4`（CI run 40612049526 success） |
| #955 | B | B2 | **main** `ae19c623`・PR #970（scope `--editing-property`・48 checks / 0.9 s・50 倍 1,000 組で破れ 2 件 → 製品の守りの穴 **#971**（`PlaceCaret` の範囲の外の位置で文字の途中・CRLF の間に置く）。模型の決め: 通常モードの 1 文字は ADR 0065・単独の CR は貼り付けで残る・redo のキャレットは末尾） |
| #971 | B → 直し | I8 | PR #977（寄せの 1 本 `placed_caret`（行の切り詰め・CR の前・通常モードは 1 文字の始め）・`line_end` の行 0 を行 1 として読む・`PointText` も同じ口・回帰の組を反転し除外を外した・50 倍 1,028 checks で破れ 0）。`offset_of` の中でなく `PlaceCaret` の入口に置いた席の判断を採る（呼び出し元 40 か所の意味を変えない）。**main** `b053c6b3` |
| #992 → #1035 | 分割 1 本目 | S1 | PR #1034（`EditorController.cpp` 5,273 行 → 10 ファイル＋ `EditorControllerDetail.hpp/.cpp`・最大は入口 1,085 行・member 245 と補助 89 を照合・`EditorView.cpp` → `EditorControllerView.cpp`・checks 数は前後同じ 2,325,108 / 24,045 / 48 / 11 / 4,191 / 708・protected-diff 0・symbols 0）。独立レビュー R0〜R4 merge 可（旧 334 項目のうち 333 が文字どおり・残る 1 件は `clipped_mark` の `inline constexpr`・`D:/NeNeNib/outputs/20261028/review-1034/`）。設計席の判断: CNF-014（3,000 行の門）は `VimStep.cpp` 6,153 行が残るので 2 本目で active・子 Issue #1035 を立てて `Closes` を替える（親 #992 は 2 本目が閉じる）。merge の道具が題名の `(#992)` と本文の `Closes #1035` の不一致（GIT-001）で 1 回止めた → 題名を `(#1035)` に。**main** `fd742f92` |
| #1033 | 門 | 設計席 | PR #1036（ARC-002 の target 依存の検査で同じ module の別の target を許す・include の検査と同じ規則・`test_arc002_dependency_on_same_module_target_passes`・109 件 OK）。S1 が見つけた main の 1 violation（`nib_language_server_tests` → `nib_fake_language_server`）が 0 に。**main** `6646d40c` |
| #992 → 2 本目 | 分割 2 本目 | S2 | PR #1053（`VimStep.cpp` 6,153 行 → 12 ファイル＋ `VimStepDetail.hpp/.cpp`・最大は `VimStep.cpp` 1,775 行・関数 367・翻訳単位をまたぐ 120 本を名前を変えず `detail` へ・checks 数は既定 2,325,108 / `--vim-random` 11 / `--vim-search` 2,251 / `--vim-marks` 969 / `--vim-dot` 1,689 / `--application` 24,045 / 窓 4,191 / adapter 708 の 8 本とも前後同じ・protected-diff 0・symbols 0・CNF-014 を active（`translation_unit_size_checks`・試験 5 本・最大のファイルは `EditorWindow.cpp` 2,906 行）。独立レビュー（`review-1053/`）: 383 項目中 379 が文字どおり・4 件は `inline constexpr`・置き場は意味で決めた所（検索と `.` の節の後ろ半分 → 入口・VISUAL の `insert_*` → Insert・待ち / g z Ctrl-w / マーク → 入口）も妥当・直してから merge は「窓と adapter の checks の記録」の 1 件 → 設計席が取って本文に足し、ADR 0158 に表の追補を commit。**main** `fa0f2fde`（#992 閉）・CNF-014 active |
| #993 → #1056 | 巨大 段 2（L2a） | L2a | PR #1054（`DocumentReadPort` と和型・`DocumentDecoding` の `read_opened_document` / `decoded_text` の 1 本（controller の `read_document` を消し自動退避も通す）・券と cancel・合成ルートは同期の `SynchronousDocumentReader`（L2b が置き換える）・`insufficient_memory`・契約 `--open-async` 31 checks・既定 2,325,139・quiet paths の欄）。席が見つけた設計の穴 2 つ（`reach_tab` の裏の読み・歩き / 入力行 / 面の最中に結果を採る）は ADR 0159 の追補で L2b の形を決めた（暫定は製品の振る舞いを変えない）。`Closes` は子 Issue #1056 に。main の `EditorWindow.cpp` の clang-format の差分 → #1057。独立レビュー（`review-1054/`）: 開く道は 1 本・券と取消に穴なし・写しは本文 0・直してから merge は `adopt_read` の `else` の `static_assert`（CPP-002）1 件 → 設計席が直して再試験（`--open-async` 31・既定 2,325,139）。後続 5 点は親 #993 のコメント。**main** `ccdc15d6`（#1056 閉） |
| #997 → #1044 | エクスプローラー（E2） | E2 | PR #1055（core の `ExplorerTree` / `ExplorerLayout` ほか・Ex の `set explorer` / `set root=`・操作 2 つ（E4 で動く）・**設定 v4**（ADR 0170 決定 3 の鍵の集合・`ui_language` 6 値・`lsp` `explorer` `root` `features`・旧版は v4 不在時だけ・保存は v4 だけ）・`--explorer-tree` 36 / `--explorer-layout` 15・50 倍 2 seed × 10,000 列で破れ 0・既定 2,325,263）。差し戻し 1 回: `lsp.<language>.command` の行を受けることと上限 16 KiB（ADR 0170 決定 3・#1044 のコメント）が入っていなかった → 同じ席に続きを送り `cdc1ce2d` までで直った。独立レビュー（`review-1055/`）: v4 は ADR 0170 決定 3 と一致・移行と lock は ADR 0123 改訂のとおり・直してから merge 2 件（知らせの文の 4096・ADR 0114 の追補が実装と逆）→ 設計席が直した。v3 の名を正本として書く文書 9 か所は文書 PR で v4 に。乗せ替えで `NibTests.cpp` の表の宣言行が衝突（#1054 と両方が行を足した）→ 1 本残して行を数え直し（contracts 68・scopes 91）。**main** `1c828873`（#1044 閉） |
| #848 | C | C4 | PR #968（`ExClipboardRequest` enable / disable / show・`VimState.clipboard_unnamed`・書きは `registers_written` の `clipboard_written`・読みは `p` `P` の直前だけ `vim_reads_unnamed_clipboard`・ADR 0051 追補・`--vim-clipboard` 275 checks）。席は `:set clipboard?` の表し方で止まり、設計席が request 型（前例どおり）に決めて再開。独立レビュー R 6（鍵は表から引く・自分の書いた本文を読み戻すと矩形が文字単位に化ける → 決定 G を「本文が自分の書いたものと同じなら種類を保つ」に改めた・VISUAL では読まない・コメント・否定の試験）→ 直し済み `3c6cf47a`（`--vim-clipboard` 282 checks）→ main の #972（`stepped_vim`・`ExResult.line`）と controller / `ExResult.hpp` で衝突 → 乗せ替えの席（R1）が解いた（副産物: #972 が application に `std::function` を持ち込み `eng/symbols.py` が 1 violation・CI は symbols を回していない → **#1023 #1024**）→ **main** `cb612530` |

## 2. 分かったこと

### Vim の差分 fuzz（#954）の最初の 900 件（seed 1〜3・各 300）

| 原因 | 件数 | Issue | 根拠 |
|---|---:|---|---|
| 前の跳びが無い `''` `` ` ` `` を Nib は失敗にする。本物の Vim（非 Ex）は開いた時点で前の位置の印を (1, 1) に置いて 1 行目へ跳ぶ（oracle の `-es` は最後の行に置くので fixture では測れない） | 19 | #962 | `D:/NeNeNib/outputs/20261028/probe-pcmark/` |
| `~` `g~` `gu` `gU` の全角の英字（Vim は Unicode の簡単な写像） | 5 | #781（既知） | 設計どおりの差 |
| 矩形 VISUAL の `~` `u` `U` が効かない | 3 | #959 | — |
| `C` `D` が消す本文の無いとき Vim は無名レジスタを空で書く（種類 v）・Nib は前のまま | 2 | #960 | — |
| `*` `#` の語の判定が ASCII だけ（全角の語を飛ばす・keyword が無いときの非空白の文字列の fallback も無い） | 4 | #961 | — |
| 後ろ向きの行単位の yank（`3y-` `yk`）の後のキャレットは motion の着地・Nib は欲しい列を保つ | 1 | #965 | probe（`jy-` → (1,1)・`j$yk` → (1,3)） |
| 最後の行の `{N}$` は失敗して動かない・Nib は行末へ | 1 | #966 | probe（`2$` → (1,1)・2 行で `3$` → (2,3)） |
| VISUAL の選択が複数の文字のときの `aB` の起点とブロックの外の失敗 | 2 | #967 | — |
| 拒否（比べられない）: `-es` の Vim は `:normal!` の中のエラー（E35 の `n`・E353 の `"8p`）で終了コード 1 | 125 / 900 | #963（道具） | — |

道具の最初の 1 回で製品の違いを 7 種（うち 6 種が新規）見つけた。

続き（生成器 2・#963・seed 4〜12 の 2700 件・既知を Issue ごとに分けて数える）: 拒否 208（7.7%）・一致 2446・既知 38・**新規 8** → #979（空行の `ciW`）・#980（全く空のバッファ）・#988（`"_p` は黙って成功）・ほかは #781 と #961 の別の形（`V…~`・`b` の全角の数字）。生成器 2 で `''` の拒否と oracle の起点の差が消え、拒否は空のレジスタの `p`（E353）が 9 割。

夜（今日の直しを全部含む main `7bb06c56` の再生器・seed 13〜20 の 2400 件）: 拒否 180・一致 2216・不一致 **4**（既知 #985 1・新規 3 → 2 種: 空白だけの行の `I` は Vim は空白の後ろに入れる（#1027）・`4*''` の `''` の行き先（#1028 → **Nib のバグではない**: 自分に着く検索の後は Vim が前の位置を跳ぶ前の値に戻し（checkpcmark）、oracle の -es の起点（最終行）が出ていた。fixture 17 件を m' を先に置く形で足し Nib は全部一致・PR #1030）。朝の 12 種の違いは全部消えた。fuzz の次の走りは `--exclude` で `''` と `` ` ` `` を外す（契約と fixture で守られている・oracle の起点の差だけが出る）。fixture 2315 件は全部通っている状態での発見なので、例に基づく fixture の外に Vim との差が残っていることが分かる。

### 設計（CI 待ちの間に・hide の提案）

hide の判断（夕方・外から）: **ADR 0158〜0161 は全部受理・LSP は初版の範囲に入れる（FR-021・D65）・細部は計画の推しのまま・着手は 巨大 段 2 → LSP L0 の順で、どちらも分割（#992）の後**。仕様に D65 と FR-021 を足し、FR-013 の文言を目標の形（1 GB の UTF-8・読み込み中も打てる）に改めた。

- **ADR 0158（提案）** D48 の分割の切り方: `EditorController` は意図の種類の単位で 10 ファイル（1 class のまま・`EditorView.cpp` が先例）・`VimStep` は鍵の表の節で 12 ファイル・群をまたぐ補助は `*Detail.hpp/.cpp`・振る舞いを変えない証明は `protected-diff` 0 と checks 数の一致・CNF-014（3,000 行の門）。Issue #992・席の依頼書 `brief-S1-992-split-controller.md`（鎖が終わってから出す）。
- **ADR 0159（提案）** 巨大ファイル 段 2・3: `DocumentReadPort{request, cancel, collect}`・adapter が worker で読み → 判定 → 流れる検証 → `TextBuffer::from_utf8` を揃え application は move で採用・64 MiB 以下は同期（起動と 16 MiB の基準値は不変・worker の最初の仕事 device の後ろに並ばない）・揃うまでタブを足さず「読み込み中: 名前 37%」・券で古い完了を捨てる・`insufficient_memory`・上限 1 GiB（UTF-8）/ 64 MiB（Shift_JIS / UTF-16）・BOM は piece の開始をずらす。Issue #993。前提は #992。
- **ADR 0160（提案）** LSP 段 L0: `LanguageServerPort{start, send, collect, stop}`・adapter は子プロセス（Job Object）と OVERLAPPED の名前つきパイプ 2 本で待ちは窓の pump の配列（`Win32Worker` に同期の読みを載せない・2 本目のスレッドも作らない）・core の純関数で `lsp_frames`（`Content-Length`）と最小の JSON（外部ライブラリなし・性質の試験）と JSON-RPC の対応（`positionEncodings`・UTF-16 は `Utf16` の 1 本）・寿命と失敗は値・再起動 3 回・`LanguageServerTick`・設定 v4（`lsp` 既定 off・設定に書いたパスだけ）・契約は替え玉のサーバー exe で CI に clangd を要らなくする。Issue #994。hide の確認: L-1（初版の範囲・FR と D 番号）・再起動の回数・4 MiB・clangd。
- **計画（案）** サイドのファイルエクスプローラー（hide の追加の指示・`docs/plans/2026-10-28-file-explorer.md`・#997）: 左の面 240 DIP・`ExplorerTree` の純粋な値・`FolderPort` を entries に広げて裏で読む・Ctrl+Shift+E と `:set explorer`・ルートは `:set root=` と F1 の操作・設定 v4 は LSP と 1 回で・4〜5 席日。見た目 3 案と鍵の案に推しを付けて hide の判断 → **全部推し（D66・FR-022・ADR 0162 受理・採用案 `docs/design/2026-10-28-explorer-look.md`）**。
- **計画（案）** イージーモーション（hide の追加の指示・`docs/plans/2026-10-28-easymotion.md`・#998）: `\\` の後に `w b e j k f F s l`・見えている範囲の跳び先に印（vim-easymotion の既定の文字・27 超は 2 文字の組）・跳び先は Vim の語の分類で日本語の中にも・`\\f` の文字は IME の 1 文字の入力行・oracle では測れないので契約と性質の試験・3.5〜4 席日。E-1〜E-7 は hide の判断 → **推しのまま（D67・FR-023・ADR 0163 受理）**。
- **計画（受理）** 編集の定番の機能の追加計画（`docs/plans/2026-10-28-editing-features-roadmap.md`・D67・FR-024〜027・#999〜#1008）: matchparen（ADR 0164 受理）・commentary・surround・変更の印・全文検索・autopairs・indent guides・yank の光り・`:s` の族・補完。速さの条件 §0（分岐を足さない・既定オンは +1% まで・全文を走らない・frame を増やさない）。2 段目は hide の判断で 5 本を取り入れ（#1011〜#1015）・4 本を検討リスト・3 本は要らない。
- **計画（案）** 設定モーダルと「速さへの影響」の可視化（`docs/plans/2026-10-28-settings-modal-speed-costs.md`・#1016・hide の要望）: core の表 `feature_costs`（段は測った数字から機械で決める）・設定 v4 の `features` の欄に全部のオン・オフを畳む・`:set` は表から生成・モーダルは #454 の土台に「機能」の節・要約の 1 行・3 つの組。G-1〜G-5 は hide の判断 → **ほぼ推し（D68・FR-028）**。
- **計画** 表示言語 6 言語（`docs/plans/2026-10-28-display-language.md`・D69・FR-029・#1019）: core の表 `ui_texts`（`UiMessage` × `DisplayLanguage`・網羅性）・日英が先・残り 4 言語は機械の訳とレビューの一覧・幅の試験・Vim の文は英語のまま・ADR 0123 を改訂へ。
- hide の追加の判断（D70・FR-030）: ミニマップ（bitmap の保持・既定オフ・#1017）と起動画面（既定オン・#1018・IDE の「ようこそ」・フォルダを開くも）を実装予定に。
- **ADR（受理・hide の「CI 待ちに設計を進めて」）**: 0123 改訂（6 言語と文の表 `ui_texts`・CNF-016）・0165（commentary / surround / autopairs・記号と対の表 1 つずつ・全角の括弧）・0166（変更の印は保存からの `Edit` の増分・indent guides・yank の光りはタイマーの意図）・0167（全文検索は worker が塊ごとに・面に流し込む・1,000 件）・0168（`:s` の族は 1 回の `Edit`・照合器に群を足す・見えている行の preview・`c` の問い）・0169（INSERT の補完は文書ごとの語の表・面は Ctrl+P の部品・LSP L2 も同じ面）・0170（設定モーダル・`feature_costs` の表・段は機械で・v4 の鍵の集合を 1 回で・組 3 つ）・0171（ミニマップは bitmap の窓・編集した帯だけ）・0172（起動画面・履歴は最初の frame の後の 1 回・見た目の案 `docs/design/2026-10-28-start-screen-look.md`）・0173（末尾の空白・TODO・色の見本・ファイル操作・行のファジー検索）。設計席の 1 日の設計: ADR 16 本（0158〜0173）・計画 5 本・採用案と見た目の案 2 本・Issue 32 本。
- **ADR 0161（提案）** LSP 段 L1: 診断は文書の値（1,000 件まで・position の写しは core の 1 本・編集では `EditAdjust` でずらす）・増分の `didChange` を 1 意図 1 通に束ねる・見せ方は `LineView.diagnostics` の波線（IME の `underline_runs` の道・色は `SyntaxPalette` の error / warning）と右端の 5 つ目の項目「E2 W1」・`lsp-` のベンチは替え玉のサーバー exe（D60）。Issue #995。

### 鎖の道具の穴（CI が見つけた）

#989 の乗せ替えで `NibTests.cpp` の表の宣言行の衝突を「両方を残す」で解いたため宣言行が 2 重になり、ローカルの「ビルドして既定実行」が通ったのに CI のビルドで落ちた。原因は `merge-chain2.sh` が pwsh に Git Bash の `/c/...` のパスで `toolchain.ps1` を渡していて dot-source が失敗 → cmake が見つからず → ログに `error:` が無いので成功に見え → 古い exe で試験が通っていた。それまでの 8 本の乗せ替えも同じ穴を通っていたが、CI が毎回ビルドと ctest を回したので main は壊れていない（ADR 0136 の形が効いた）。道具は exe の時刻と終了コードと `not recognized` で確かめる形に直し、`NibTests.cpp` の衝突の解決は宣言行が 1 本であることを数える。

### 速さのゲート（夜・main `0070859c` の Release・負荷 2%・席を止めた窓）

| ベンチ | 中央値 ms | 基準 |
|---|---:|---:|
| startup-first-frame | 41.3 | 41.9 |
| startup-window-shown | 31.8 | 32.4 |
| key-to-frame-single | 0.580 | 0.64 |
| key-to-frame-burst-200 | 2.486 | 2.16 の許容内 |
| open-large-file-16mib | 48.6 | 49.8 |
| key-to-frame-burst-200-16mib | 2.693 | 2.34 の許容内 |
| key-to-frame-palette-5000 | 1.014 | 1.73 |
| key-to-frame-single-long-line | 1.132 | 1.574 |

**8 本 0 regression**（`D:/NeNeNib/outputs/20261028/gate-main-0070859.log`）。今日の Vim の直し 12 本と LSP の core / adapter（打鍵の道には載らない）で退行なし。

## 3. 効かなかったこと・決めなかったこと

- CNF-014（3,000 行の門）は 1 本目の分割 PR では active にしなかった。main の `VimStep.cpp` が 6,153 行で落ちるため。2 本目（S2）で足す。期限つきの waiver は使わない。
- ARC-002 の target 依存の検査は、同じ module の別の target を違反にしていた（include の検査は同じ module を通していた）。規則を揃えた（#1033）。門を弱めたのではなく include と同じ境界の読み。
- 1 日の終わりに hide と「Vim / Neovim の人が欲しがる機能は揃ったか」を点検した。仕様 T2 に書いてあって未実装: ジャンプリスト `Ctrl-o/i`・INSERT の `Ctrl-w/u/r`・矩形の `I A c C > <`・`:g` と一般 Ex・簡単な `map`・`:e :b :ls`。T2 に無いが定番で小さい: `gv` `gi`・`Ctrl-a/x`・`R`・`&`・`:set ignorecase smartcase relativenumber scrolloff`・`gd`・`=` `gq`。hide「よし」→ Issue #1037（ジャンプリスト）#1038（Ctrl-a/x）#1039（gv gi）#1040（INSERT の Ctrl-w/u/r）#1041（set の 4 つ）#1042（矩形の I A c C > <）。`:g` の #1043 は #1007（FR-027）と重なるので閉じて #1007 に足した。`map` は hide「全部推し」で D72（設定 v4 の行・入口の置き換え・engine は知らない・待ちは 1 秒のタイマー）→ #1058。
- 印刷は無い（仕様・計画・ソースのどこにも無い）。hide の指示で計画 §3.5 の検討リストに入れた。hide「メモ帳の代わりという場合もある」→ 通常モード（メモ帳の役）の抜けを点検: **Ctrl+F / F3 / Ctrl+H の検索と置換・Ctrl+G の行へ移動が無い**（検索は Vim の `/` だけ）・Ctrl+ホイールのズーム・保存時の文字コードと改行の選択・F5 の日時（`ClockPort` が要る）・右クリックのメニュー。設計席のおすすめは「検索と置換と Ctrl+G を先に・印刷を取り入れるへ・小物 3 つ」。→ hide「メモ帳系で足りないものは全部入れよう。ショートカットが Vim と被るものは Vim 優先で代替を用意」「全面 md プレビューも欲しい。編集はできないけど選択は出来る感じで」= **D71**（FR-031）。Issue #1046（検索・置換・行へ移動）#1047（印刷）#1048（Ctrl+ホイール）#1049（文字コードと改行の選択）#1050（F5 の日時・`ClockPort`）#1051（右クリックのメニュー）#1052（全面の Markdown プレビュー）。

## 4. 記録の所在

`D:/NeNeNib/outputs/20261028/`（計測のログ・レビュー `review-1034` `review-1053` `review-1054` `review-1055`・鎖のログ `chain-*.log`・fuzz の結果）、`D:/NeNeNib/briefs/20261028/`（依頼書と Issue の原稿・PR 本文の写し）、`D:/NeNeNib/scripts/20261028/`（鎖の道具）と `D:/NeNeNib/scripts/992/`（分割の道具 `split.py` `vverify.py`）。1 日の終わりの main は `1c828873`（30 本）。
