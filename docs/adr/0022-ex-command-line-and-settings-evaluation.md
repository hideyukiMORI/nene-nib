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
   - 追補（2026-10-20・#725・施主決定 D59 の続き）: `:` の行と検索（`/` `?`）の行は Up / Down で入力の履歴を歩く。履歴は core の `InputHistory` で、窓に `:` 用と検索用の 2 本を `EditorState` の窓の欄に持つ（各 100 件・古いものから捨てる・セッションの中だけでファイルに書かない・タブの切り替えで変わらない・`verify_quiet_paths` は不変）。足すのは確定の意図（Enter・`SubmitCommand`）が通る controller の `confirmed_command` だけで、空と直前と同じ入力・Esc / 取消・再生（`.`・マクロ）の中の `<Esc>` の確定（Vim の c_<Esc>）・Ctrl+P の面の確定は足さない。歩きは既存の意図 `EditCommand` の `CommandEdit::history_previous` / `history_next`（新しい意図は作らない・ui は `press_command_key` の表の Up / Down）で、`CommandLine` / `SearchLine` の `recalled` が `InputHistory::stepped` の 1 本を呼ぶ。Up は歩きを始めたときの入力（全文）を接頭辞とする 1 つ前の履歴、Down は 1 つ次で、末尾の次は歩きを始めたときの入力とキャレットに戻る（回り込まない・前が無ければ動かない）。歩きの状態（始めたときの入力と位置 `HistoryWalk`）は入力行が持ち、文字を打つ・Backspace・Tab の補完（候補が無いときも）・カーソル移動は歩きを終える（その内容が次の接頭辞）。面の Up / Down は候補の選択のまま（ADR 0150 の決定 2）で、ADR 0150 の決定 3 の「`:` の行の Up / Down は何もしない」はこの追補で置き換える。マクロと `.` の再生の中の上下は今までどおり捨てる（ADR 0048 の決定 8）。永続化・面の履歴・Ctrl+R 系の検索は後続。
3. coreの純関数 `evaluate_ex` は文字列と現在の設定/OS外観を受け、保存する設定（問い合わせなら無し）と表示文字列、または閉じた失敗を返す。`:colorscheme` は現在の解決済みテーマ（system追従も明記）、`:colorscheme <name>` / `system` は切替。`:set fontsize=<pt>` と `:set guifont=<name>:h<pt>` を扱う。先頭のcolonは入力表示だけが持つ。未知コマンド・オプション・テーマ・不正値は明示して拒否する。
   - 追補（2026-10-28・#847）: 行番号だけの命令 `:N`（10 進の数字だけ・先頭の 0 を許し・size_t を超えれば最大に飽和）・`:0`・`:$` は `evaluate_ex` が `ExResult::line`（`ExLineRequest`・`$` は空）に置き、controller の `jump_to_line` が engine の `vim_line_jumped` を鍵と同じ測り・写しの道（`stepped_vim`）で写す。着地は `{N}G` と同じ 1 本（最後の行に切り詰め・`:0` は 1 行目・最初の非空白・欲しい列）で、知らせも警告音も無い。`:N` は jump ではなく `''` を置かない（Vim 9.1 の `:help jump-motions` に `:N` は無い・probe と oracle の `ex-line-context-mark` で確認）。範囲 `:3,5`・相対 `:+3`・マーク `:'a`・`:3d` は今までどおりの失敗で、補完の候補に数字は出さない。
4. ptの文字列解析もFontSizeに集約し、既存settings codecから同じ経路を使う。guifontの名前は最後の`:h`より前を取り、空白入りの名前を受ける。フォント名は既存DisplayTextで検証する。保存形式と8〜40ptの範囲は変更しない。
5. `colorscheme` / `set fontsize=` / `set guifont=` の補完と、テーマ補完は純関数で生成する。テーマ一覧はBuiltinThemesの表を共用する。Tab巡回は最初のprefixと選択indexをCommandLine内に持ち、手入力/カーソル移動で解除する。候補は入力の上のpanelに表示する。
   - 追記（2026-10-17・#597・施主決定 D58）: `set guifont=` の補完は OS の字体の family 名を `set guifont=<名前>` の候補として後ろに足す（全件の一覧には出さない・`:h` は付けない）。一覧は application の型のある port `FontFamilyPort::list()`（失敗は閉じた `FontFamilyFailure`）で、実装は ui/win32 の `SystemFontFamilies`（描画と同じ DirectWrite・ADR 0114 の `TextLayoutPort` と同じ置き場）。controller は入力行の挿入・編集の後に core の `completes_font_family` が真のときだけ、プロセスで 1 回だけ読む。読めなければ候補は打った値だけ（知らせなし）。名前は各 family の全ロケールの名前（`BIZ UDゴシック` と `BIZ UDGothic` の両方が候補に出る・既定ロケールは読まない・ARC-007）で、重複は core の `FontFamilies::from` が除く。
   - 追記（2026-10-20・#696）: 候補の並びと引数の照合を Vim 9.1 に揃える。`colorscheme` の候補は組み込みと利用者のテーマを混ぜた名前順（core の `ascii_folded_before`＝ASCII の大文字小文字を畳んだ順・同じならバイト順）で `ThemeCatalog::names()` が返し（`system` は最後のまま・表の順は表示に使わない）、`set guifont=` の候補は adapters の `Win32FontCollationAdapter` が OS の照合順（既定ロケールの `CompareStringEx`・ARC-007）で並べた順を core の `FontFamilies::from` が変えずに重複だけ除き、引数（テーマ名・字体名）の前方一致は core の `ascii_prefix_ignoring_case` で ASCII の大文字小文字を無視する（命令名は区別のまま・Ctrl+P の `:` は点数順のまま同点だけ同じ名前順）。
6. 設定の保存はcontrollerの一つの `persist_settings` に集め、C2の相対サイズ変更とExが共用する。同値は保存しない。成功してから状態を変え、失敗時は元設定を保持する。表示高とOS背景の更新も設定変更後に同じUI経路へ送る。コマンド結果は次の利用者入力まで表示し、描画/レイアウト通知では消さない。
7. ステータス左側を入力または結果メッセージに置き換え、右の行/桁・encoding・line endingは維持する。コマンド領域と候補panelをcoreの配置から導き、狭い窓では幅を0以上に制限する。文字とcaretは左領域内にclipし、長い入力はcaretに追従する。本文のcaretは入力中に描かない。UI文字サイズは本文のptと独立。
8. この設定用ExはNORMALから始める。Vim範囲/回数付きEx、VISUAL範囲、パイプ、VimScript、`:w` / `:q`、履歴、Ctrl+Pは後続。範囲や未対応構文を黙って別コマンドとして実行しない。Ex中のIMEはNORMALと同じ閉状態を維持し、Unicodeフォント名はUTF-8入力/貼付で受ける。本文のIME経路へ流さない。

## 追補（2026-11-04・#1199）: Ex の解析は解析関数の表

1. 操作の表（ADR 0078 の追補 #1188）と同じく、core の Ex も表で引く。`colorscheme` と `set` のほかの命令は、`ExResult.cpp` の無名名前空間の `constexpr std::array<ExCommandParser, N> command_parsers` を `remaining_command(text)` が上から回し、最初に値を返した行の結果を返す。どの行にも当たらなければ `unknown_command`（開いた入力なので CPP-002 の `default` には当たらない）。
2. 行の型は `using ExCommandParser = std::optional<std::expected<ExResult, ExEvaluationFailure>> (*)(std::string_view);`（`using` なので CPP-011 の型を増やさない）。値なしは「この行の命令ではない」、`expected` の失敗は「この行の命令だが受けない」。
3. 行の順は `line_command`（数字だけ・`$` だけ）が先頭、`print_command`（`:ha[rdcopy]`）は `document_command` の前。`:noh[lsearch]` も 1 行 `nohlsearch_command` にし、`evaluate_ex` の名前と引数の切り出しを中に写す。束ねの関数（`document_command_or_print`）は置かない。
4. 網羅性の `static_assert` は置かない。無名名前空間の解析関数を表に書き忘れると `-Wunused-function`（`/W4` が含む・`/WX` で error）でコンパイルが落ち、単体試験 `verify_ex_command_table` が候補の全件が表のどれかの行に当たることと各行の境目の入力を守る。
5. 命令を足すときの差分は、解析関数 1 つと表に 1 行（と補完の候補）。候補の名前の表 `fixed_candidates` と解析関数の名前の 2 か所の統合は別 Issue。

## 追補（2026-11-12・#1526）: VISUAL の `:` と回数の `:` は範囲つきで開く

Vim 9.1 の実測（`-u NONE`・`:normal!` と `feedkeys(…, 'tx')`・窓のある形 `--not-a-term`）と source（`normal.c` の `nv_colon`・`ops.c` の `op_colon` / `do_pending_operator`）に揃える。それまで VISUAL の `:` は何もしない鍵で、続けて打った `set so=2` の `s` が VISUAL の置き換えになって本文を変えた（#1526）。NORMAL の `3:` は取消で、続く文字が NORMAL の鍵として流れた。

1. VISUAL・行単位・矩形の `:` は VISUAL を抜けて入力行を開き、行の先に `'<,'>` を置く（Vim の `op_colon`）。キャレットは Vim の `oap->start`（文字単位は選択の先の端・行単位は始めの端がその行の先なら行頭で、キャレットが上なら今の桁・矩形は左上）で、`d y` の戻り先と同じ `visual_restore` と矩形の `block_caret` の 1 本ずつ。前の選択（`gv`）と `'<` `'>` は VISUAL を抜ける鍵の出口（`visual_remembered`）が覚える。VISUAL の中の回数は使わない（Vim も `'<,'>` だけ）。`.` の記録は変えない（Vim の `op_colon` は redo に入らない）。
2. NORMAL の回数つき `N:` は行の先に `.,.+(N-1)`（`1:` は `.`）を置く（Vim の `nv_colon`）。
3. 効果は `VimOpenCommandLine` の 1 つで、`range`（置く文字・空なら何も置かない）と `caret`（VISUAL を抜けて置くキャレット・NORMAL では値なし）を持つ。controller の `perform` がキャレットを置いてから同じ `CommandLine` を開き、`inserted` で範囲を入れる。IME の構え（D57）と入力行の写しは `:` と同じ道のまま。
4. 範囲の解析は `evaluate_ex` の入口の 1 か所（`ranged_command`）。範囲は Vim の番地の部分集合（`.` `$` `%` 10 進・`'` と ASCII の 1 文字・`+N` `-N` と数字の足し・`,` `;` の区切り）で、数字だけ・`$` だけの行は今までどおり `line_command`（`:N`）。範囲の後ろが空の行は次のとおり。
   - `'<,'>` だけ: 何もしない（成功・知らせなし）。Vim は VISUAL の `:` で開いた行の `'<,'>` の確定でキャレットを選択の先の端に残す（桁も保つ・実測。NORMAL で打った `:'<,'>` は `'>` の行へ飛ぶが、その形は残す違い）。
   - `.` と `.,.+N`: キャレットの行から N 行下へ `{N}G` と同じ着地（`ExRelativeLineRequest`・controller が `:N` の `jump_to_line` へ渡す・本文の外は最後の行）。
   - ほかの範囲だけの行（`'<,'>3` `1,5` `%` `'a` など）: `unsupported_argument`（「Not supported: <入力>」）。Vim は `line2` へ飛ぶ（残す違い）。
5. 範囲の後ろに命令がある行は、後ろを今の評価に通して、失敗ならその失敗（未知の命令は `Unknown command`）。成功した命令が Vim で範囲を取らない種類（設定・強調・incsearch・表示形式・言語・構文・クリップボード・ルート・LSP・`map`・保存の形＝ `:set` `:colorscheme` `:noh` `:nmap` 系・`:syntax`）なら新しい `ExFailure::no_range_allowed`（「E481: No range allowed」・Vim と同じ番号）、ほか（`:w` `:q` `:e` `:tabnext` `:only` `:hardcopy` `:tabs` など）は範囲つきの形を Nib がまだ持たないので `unsupported_argument`（Vim は E140 / E16 / E481 / 範囲の実行・残す違い）。`:s` `:d` など範囲を取る Vim の命令は Nib に無いので `Unknown command`（#1007 が範囲の命令を足すまで）。どの失敗も本文・選択・レジスタを変えない。
6. Ex の評価の失敗（`evaluate_ex` の失敗と設定の保存の失敗）は、知らせと警告音に加えて再生（`:normal!` の写しの fixture・`.`・マクロ）を打ち切る（`submit(CommandLine)` が `VimRepeatFailure::refused` を返す）。Vim の `emsg` が typeahead を捨てるのと同じで、入力行に打った後ろの鍵が本文の鍵として流れない。命令の要求の後の失敗（`:tabnext 9` の範囲の外など）は今までどおり知らせだけ（後続）。
7. 窓で打った Esc / Ctrl+C は入力行を閉じるだけで、モードは NORMAL（VISUAL へ戻らない・Vim の実測）。キャレットは決定 1 の位置。再生の中の `<Esc>` は今までどおり確定（Vim の `c_<Esc>`）。マクロの録画の道（`:` の行を録らない・#1528）は変えない。
8. 限界（Vim と違いを残した形・どれも本文・選択・レジスタを変えない失敗か無動作）:

   | 形 | Vim 9.1 | Nib |
   |---|---|---|
   | 範囲つきで実行する命令（`:'<,'>s` `:'<,'>d` `:3d`） | 範囲に実行 | `Unknown command`（#1007） |
   | 範囲だけの行の跳び（`:'<,'>3` `:1,5` `:%` `:'a`） | `line2` へ飛ぶ | `Not supported: <入力>`（#1540） |
   | NORMAL で打った `:'<,'>` | `'>` の行へ飛ぶ | 動かない（VISUAL の `:` の確定と同じ扱い） |
   | `:'<,'>w` `:'<,'>q` `:'<,'>e` `:'<,'>tabnext` | E140 / E16 / E481 / E16 | `Not supported`（番号が違う） |

9. fixture は接頭辞 `visual-colon-`（20 件）。fixture にしない形が 3 つある。マクロの中の VISUAL の `:`（fixture の再生がマクロを `q` で録って置くので、録画が `:` の行を録らない間・#1528）、`` `< `` `` `> `` `'<` `'>` の移動（ADR 0152 の限界のまま・置く値は前の選択 `gv` で確かめる）、再生の中の入力行の `<Esc>`（Vim の `:normal!` は行を実行するが、fixture の道は打った鍵の取消・ADR 0048 の決定 8）。Ex が失敗する鍵列は `-es` の Vim が終了コード 1 で終わり oracle が測れないので、窓のある形（`viewport`）で測る。窓のある形にしたのは、`-es` の Ex モードでは範囲だけの行が `'<,'>+`（`'>` の次の行）になる別の規則（`ex_docmd.c` の `use_plus_cmd`）だから。

## 追補（2026-11-12・#1549）: オペレータ待ちの `:` は Ex の命令を移動として使う

Vim 9.1 の実測（窓のある形 `viewport`・`set scrolloff=0`）と source（`normal.c` の `nv_colon`・`ops.c` の `do_pending_operator`・`ex_docmd.c` の `do_cmdline` の `DOCMD_KEEPLINE`）に揃える。それまでオペレータ待ちの `:`（`d:` `c:` `y:`）は取消と失敗で入力行が開かず、窓で続けて打った `set so=2<CR>` が NORMAL の鍵として流れて `o` から本文に文字が入った（#1549・V-1547 の L1）。

1. オペレータ待ちの `:`（全部のオペレータ・回数つき・`"x` の前置きつき）は、NORMAL の `:` と同じ効果 `VimOpenCommandLine`（範囲もキャレットも無し）の 1 本で空の入力行を開く。保留のオペレータ・回数・レジスタの名指しは engine の状態に残す。回数は範囲に写さない（Vim の `nv_colon` は `op_type == OP_NOP` のときだけ `.,.+(N-1)` を置く・`j2d:5<CR>` と `jd3:5<CR>` は `jd:5<CR>` と同じ・実測）。engine の入口は `acted` の 1 か所（保留中の `:` だけを `operator_command_line` へ）。
2. 確定（Enter）は controller の `submit(CommandLine)` が engine の待ちを見て `operator_command` へ分ける。Ex の答えが行へ跳ぶ形（`:N` `:$` と範囲だけの `:.` `:.,.+N`・`landing_line`）なら、跳ぶ道（`jump_to_line`）を通さずに行き先を `VimCommandMotion{typed, line}` で engine の `vim_command_operated` へ渡す。行き先は `:N` と同じ着地（`line_counted` → `document_last`・行の最初の非空白）で、範囲は Vim の `nv_colon` の `MCHAR`・`inclusive = FALSE` のとおり文字単位の exclusive を、検索の後ろのオペレータと同じ 1 本（`exclusive_range(characters_between)`・`:help exclusive-linewise`）に通す。`:.,.+N` の行は跳ぶ道と同じ `line_below_caret` が引く。移動の種類（実測）:

   | 起点 → 行き先 | 範囲 | 例（10 行 `lineNN abc`） |
   |---|---|---|
   | 行頭（字下げの中）→ 別の行の行頭 | 行単位（行き先の行を含まない） | `jd:5<CR>` は 2〜4 行目・`jjjd:2<CR>` は 2〜3 行目・`jd:$<CR>` `jd:9999<CR>` は 2〜9 行目・`jc:4<CR>` は 2〜3 行目を替える |
   | 行の途中 → 別の行の行頭 | 文字単位・前の行の末尾まで | `jlld:5<CR>` は `ne02 abc⏎…line04 abc`・`jlg~:3<CR>` は 2 行目の `ine02 abc` |
   | どこか → 字下げのある行（行き先が最初の非空白で桁 0 でない） | 文字単位の exclusive | `jd:4<CR>`（4 行目が `    line04`）は `    line02 abc⏎line03 abc⏎    ` |
   | 同じ行（`:.`） | 文字単位の exclusive（行頭へ戻る） | `jlld:.<CR>` は `li` |

3. 跳ばない命令（`:set …` `:noh` `:colorscheme` と、要求を出して成功する `:tabnext`（タブが 1 つ）・`:only` と空の行）は Ex を 1 回だけ評価した答えを、評価の写しの 1 本（`ran_ex_result`・`evaluate_command` と共用・#1526 の `refused` の形のまま）に通し、成功ならキャレットまでの空の範囲で閉じる（`VimCommandMotion{typed, 空}`）。空の範囲は検索の後ろのオペレータと同じ道で、`d` は何も消さず、`c` は INSERT に入り、`y` は空の文字単位を取り（`yyjy:noh<CR>p` は何も貼らない）、`g~` は何も変えず、`>` は 1 行を字下げする（Vim の `op_shift` は `line_count` 1 の行・`j>:set so=2<CR>`）。欲しい列は置き直す（`$d:set so=2<CR>jj` は 8 桁目・Vim の `w_set_curswant`）。Vim の実測（10 行・2 行目）: `d:set so=2<CR>x` `d:colorscheme blue<CR>x` `d:tabnext<CR>x` `y:tabs<CR>x` は空の範囲の後に `x` が 1 文字を消し、`c:noh` `c:colorscheme blue` `c:tabnext` `c:tabs` `c:only` `c:syntax on` `c:nmap a b` の後の `X<Esc>` は INSERT で入る。命令がタブを替えて engine の待ちが消えていれば閉じない。
4. 評価の失敗（未知の命令・E481・`Not supported`・設定の保存の失敗）と、命令の要求が後で断った形（`:tabnext 9` の範囲の外など、写し先が `reject` を通った・Vim の `did_emsg` の `clearopbeep`）は、知らせと警告音と再生の打ち切り（`refused`）に加えて、engine の `vim_interrupted` でオペレータを捨てる（Vim の `cmd_result == FAIL` の `clearop`）。要求の断りは `reject`（断りと警告音の唯一の口）を通った数（`refusals_`）の前後で読み、要求の写し先ごとに失敗を返す形にはしない。窓が後で行う要求（`:tabclose` の閉じる `close_request`・一覧から頼んだ操作 `operation_request`）は、要求を出して同じ `vim_interrupted` でオペレータを捨てる（閉じた後の位置へ空の範囲を閉じない）。Vim の実測: `c:tabnext 9<CR>x` `c:tabclose<CR>x`（最後のタブ・E784） `d:tabnext 9<CR>x` は本文が変わらず NORMAL のまま（`x` は流れない）。
5. 入力行が確定の外で閉じたら、オペレータごと捨てる（NORMAL）。Esc・Ctrl+C・空の行の Backspace に限らず、Ctrl+P の面・タブの一覧・操作の一覧・文字コードの一覧・通常モードの入力行・クリック・タブの切り替え・エクスプローラーへの焦点など、Vim の入力行（Ex の行と `/` `?`）を閉じるか別の面に置き換える入口の全部を、入口ごとに書かずに 1 か所で拾う: 入力行を開いた効果（`perform(VimOpenCommandLine)` と `perform(VimOpenSearch)`）が engine の待ちを見て印 `line_operator_` を立て、意図の出口（`apply` の終わりの `settle_transient` の中）と engine へ鍵を流す直前（`deliver_vim_key`・再生の中の取消の後の鍵）の `settle_line_operator` が、印が立ち・Vim の入力行が開いておらず・engine が待っていれば `vim_interrupted` で捨てる。欲しい列は保つ（`$d:<BS>jj` は行末・実測）。`vim_interrupted` はクリックなどの割り込みと同じ捨て方で、口を増やさない。検索の Esc は今までどおり `vim_cancelled_input`（欲しい列も捨てる・ADR 0032 の決定 1）で、`d/` → Ctrl+P → Esc で保留が残る前からの穴（V-1556 の F1）も同じ 1 か所で閉じる。再生の中の `<Esc>` は今までどおり確定（Vim の `c_<Esc>`・ADR 0048 の決定 8）。
6. `.` の記録は `:` までの記録（`d:`）に入力行に打った本文の文字と `<CR>` を鍵として足したもの（`command_motion_recorded` → 記録の 1 本 `vim_recorded`・Vim の `do_pending_operator` が redo に積む `repeat_cmdline` と NL）。再生は同じ入力行の口（`deliver_vim_key` → `command_key`）を通るので、`jd:4<CR>.` は同じ `:4` へ、`jd:.,.+1<CR>j.` は今の行から 1 行を消す（実測）。マクロの録画はこの PR では変えない（`:` の行を録らない）。#1528 の直し（#1552・`macro_recorded` が `VimOpenCommandLine` を除かず、入力行の文字と確定を録る）と一緒に積むと、録画中の `d:5<CR>` は `d:5<CR>` の全体が録られ、`@a` は `operator_command` で閉じる（`vim_command_operated` は `macro_recorded` を通らないので二重に録らない）。
7. 打った文字が本文の鍵にならないことは、窓の鍵の道（`VimKeyPress` で `d` `:`・入力行の `CommandText`・`SubmitCommand` / `CancelCommand` / Backspace・`OpenCommandPalette` `OpenTabList`・`PlaceCaret`・`SwitchTab`）の単体試験 `verify_ex_operator_colon_typed` `verify_ex_operator_colon_motion_and_cancel` `verify_ex_operator_colon_refused_request` `verify_ex_operator_colon_replaced_line` が守る（`--ex-settings`）。
8. 限界（Vim と違いを残した形・どれも入力行の文字を本文に入れない）:

   | 形 | Vim 9.1 | Nib |
   |---|---|---|
   | 範囲だけの跳び（`d:'a` `d:.+2` `d:1,5` `d:%` `d:'<,'>3`） | `line2` までの移動 | `Not supported`・オペレータを捨てる（#1540） |
   | NORMAL で打った `d:'<,'>` | `'>` の行までの移動 | 跳ばない・空の範囲（#1526 の決定 8 の表のまま） |
   | 録画中の `d:5<CR>` の後の `@a`（#1552 を積むまで） | `d:5<CR>` を流す | 録画は `d` だけ（`:` の行を録らない）で、`@a` の後に `d` が待つ（#1528・#1552 を積めば消える・決定 6） |
   | タブや文書を替える命令（`c:tabnew<CR>` `d:tabnext<CR>`（タブが 2 つ以上）・`d:e x<CR>`） | 新しい窓の位置が本文の中なら閉じる（`1Gc:tabnew<CR>X` は新しいタブで INSERT・`2G` からは `clearopbeep`・実測） | 待ちが消え、何もしない（入力行の文字は本文に入らない） |
   | `c:tabs<CR>` | 一覧を出し、空の範囲で INSERT | Nib の `:tabs` はタブの一覧の面を開くので、決定 5 でオペレータを捨てる |
   | 最後のタブの `c:tabclose<CR>x` | E784 で断り、`x` は流れない | 窓を閉じる要求（施主決定 D20）を出してオペレータを捨てる。`x` は NORMAL の鍵 |

9. fixture は接頭辞 `operator-colon-`（40 件・#1547 と同じ窓のある形）。Esc・Ctrl+P・クリック・タブの切り替えの取消は fixture にしない（再生の中の `<Esc>` は確定・面とクリックは鍵ではない）ので単体試験で守る。

## 検証

### 固定済みSTLの参照追加

`FontSize::parse` の `std::from_chars` は、固定toolset 14.44.35207の `charconv:691` にある `const uint32_t _Large_power_data[578]`（十進べきの不変表）を参照する。`string_view::rfind` と候補数の `std::min(initializer_list)` は同toolsetの `xutility:106/132` の範囲走査 `__std_find_end_1` / `__std_min_8u` を参照する。この3シンボルだけをcoreの許可表へ完全一致で加える。C++ランタイムは既存の固定済み依存であり、新しいライブラリ・runtimeDependencies・platformLibrariesはない。

`std::format` を導入すると（文字列だけを渡しても）ロケール参照までcoreへ入ることを実ライブラリの検査で検出したため採用しない。Exの成功メッセージは固定文字列と検証済み入力を連結して表示し、保存時の数値表記は既存adapterが担う。locale・OS・threading・自由な外部呼び出しの拒否は維持する。許可した正例とロケール/未知関数の反例、実ライブラリのシンボルを確認する。

純関数の解析/補完/UTF-8編集、controllerの本文/履歴/レジスタ保持と保存失敗、NORMALからの入口、nativeの入力/取消/テーマ/サイズ切替/再起動を確認する。C2で成功した保存競合・codecの不変部分・無関係なVim oracle/ファイルI/Oは再利用する。対象のコードが変わった場合だけ関連する検証を更新する（ADR 0021）。

## 却下

- UIにコマンド解析と保存を持つ: 状態の正本と検証が分裂する。
- コマンド文字列を本文へ一度入力して戻す: undoと選択を壊す。
- 設定変更ごとに保存処理を複製する: C2と競合保護が分裂する。

## 追補（2026-11-12・#1525）`:set {name}&` と真偽の反転

Vim 9.1 の実測（道具 `D:/NeNeNib/scripts/20261111/c10/gen4.py`）: `{name}&` `{name}&vim` `{name}&vi` は既定値へ戻す。真偽の option の `{name}!` `inv{name}` `no{name}!` は反転する。数の option の `{name}!` は E488、`inv{name}` と `no{name}` と `no{name}&` は E474、`invno{name}` は E518、`{name}!?` `{name}&?` は E488。

1. `set_option` は読む前に `toggled_text`（反転）と `reset_text`（既定値）で今の読みの形（`wrap` / `nowrap`・`scrolloff=0` など）へ 1 段だけ写す。写した形は `&` `!` `inv` を持たない。
2. 真偽の option の表は 1 つ（`switch_options`・wrap guide ignorecase/ic smartcase/scs relativenumber/rnu minimap explorer lsp hlsearch/hls incsearch/is）で、名前と今の値を読む関数を同じ行に持つ（名前の列と値の if の列を分けない・レビュー V-1552 の L3）。今の値は `EditorSettings` の欄か、設定に無い hlsearch と incsearch は `evaluate_ex` の引数 `ExWindowState`（明暗と並べて controller が `VimState` の `highlight` `incsearch` を渡す・明暗だけの形では初期値）から読む。hlsearch は `:noh` の後（suspended）も立っている（Vim の no_hlsearch は別の旗）。
3. 既定値は保存する設定の正本 `default_editor_settings()` の 1 か所から読む（scrolloff・真偽の 8 つ・fontsize・guifont（字体と大きさ）・root（決めていない））。設定に無い hlsearch と incsearch は `ExWindowState` の初期値 `vim_default_highlight` `vim_default_incsearch`（`VimSearchHighlight.hpp`・VimState の始まりの形と共用・施主決定 D17 / D18 なので Vim の既定 off とは違う）で、短い名前の `hls&` `is&` も表から受ける（レビュー V-1552 の L4）、clipboard は外す。
4. 受けないもの（残る違い）: 文書の値（filetype / ft・fileencoding / fenc・fileformat / ff・display）の `&`（文書ごとの値で、Vim の既定の意味（空・unix）を Nib の閉じた値に写す決めが要る）。短い名前の `:set hls` `:set nohls` `:set is` と、Vim が受けて Nib が断るほかの行（レビュー V-1552 の F3・後続の Issue）。hlsearch と incsearch の反転 `hls!` `invhls` `nohls!` `is!` `invis` は受ける（Vim 9.1 の実測 `D:/NeNeNib/scripts/20261112/c10b/h.vim`・`map` の右辺によく書かれる形）。`:set` の複数の引数（`:set so=3 so&`）は今どおり受けない。
5. 窓の決め直し（ADR 0019 の #1524 の追補・`rechecks_window`）は、写した後の形で `with_window_recheck` が見る（まとめ枝 #1563 で #1524 と重ねた）。`so&` `hls!` `invwrap` `rnu&` も値を置く形なので決め直し、`ic!` `is&` は決め直さない（Vim の do_set は反転と既定値でも check_redraw を通る・契約 `verify_ex_window_recheck`）。

## 追補（2026-11-12・まとめ枝 #1563）断りは `refused` の 1 本・オペレータ待ちの `:` と INSERT の `Ctrl-o`

1. 命令の断りの読み方を 1 本にする。`ran_ex_result`（Ex の行とオペレータ待ちの `:` が共用する答えの写し）は、評価の失敗に加えて、命令の要求が後で断った（`reject` を通った・`:tabnext 9` `:tabnext 0` の E475 など）ときも `refused` を返す（`reject` の数 `refusals_` を要求の前後で比べる 1 か所）。#1525 の再生の打ち切り（`evaluate_command` の `refused`）と #1549 のオペレータの捨て方（`refused_operator_command` は返り値だけを読む）が同じ答えを読む。窓で打った鍵は今までどおり後ろを捨てない（`accept(SubmitCommand)` は返り値を捨てる）。振る舞いが変わるのは再生（`.`・`@a`・fixture の :normal!）の中で要求が断られた形で、今までは後ろの鍵が流れていた。Vim 9.1 の `-u NONE -es` の `normal! @a` は `:tabnext 9` `:tabnext 0` `3tabnext` `:tabclose` `:w` `:w!` の後ろの `j` を流さず、`:tabprevious 9` `:tabnext 1` は流す。最後のタブの `:tabclose`（Vim は E784）と無題の `:w`（E32）は Nib では断りではなく窓を閉じる流れと保存の面へ進む（施主決定 D22）ので、再生は今どおり続く（残る違い・道具 `D:/NeNeNib/scripts/20261112/1563-chain/e/e.py`・Vim に近づく向きだけ・契約 `--vim-macro` の `ex_replay_cases` と `verify_ex_failure_stops_operator_replay`）。
2. `submit(CommandLine)` は、待ちがあれば `operator_command`、無ければ `evaluate_command` を呼び、どちらの後にも `resume_vim_insert()` を通す（ADR 0182 の決定 8）。`vim_command_operated` は鍵の出口と同じ `insert_restart_noted` を確定の `<CR>` で通すので、`<C-o>d:5<CR>` `<C-o>y:5<CR>` は命令が終わって始め直し、打った `<C-o>c:5<CR>` は OP_CHANGE の KeyTyped で印を消し、:normal! の `c:5<CR>` は印を保って c の Esc の後に始め直す。`resume_vim_insert` は INSERT にいる間は始め直さない（印を保った `c` の INSERT を外から始め直さない）。Vim 9.1 の実測（oracle の viewport の形・道具 `D:/NeNeNib/scripts/20261112/1563-chain/measurev.py`）と契約 `--vim-insert-normal` の `verify_operator_lines`。

## 追補（2026-11-12・#1568）入力行の上限は 4096 バイト・あふれた行の確定は丸ごと断る

決定 1 の「1 行 256 bytes」は理由の書かれていない値で、表示の 1 行（`DisplayText::maximum_bytes`）と同じ数を入力にも使っていた。入力行は上限を超えた字を 1 字ずつ断り、鍵の列は止まらずに `<CR>` が切れた命令を実行していた（`:s/` ＋ `y` × 300 が `:s/y{254}` として走り、本文が意図と違う形に書き換わる・レビュー V-1564b の後続 2）。Vim の入力行に実質の上限は無い。

1. 上限は `core::input_line_limit`（4096 バイト・`InputText.hpp` の 1 か所）。入力行の全部（Ex の `:`・検索の `/` `?`・通常モードの「検索: 」「置換後: 」「行: 」・Ctrl+P の面）は `InputText` の純関数を共用しているので、同じ値と同じ規則になる（ADR 0023 決定 1・ADR 0032 決定 1・ADR 0174 決定 3）。`evaluate_ex` も同じ値で断る（`ExFailure::too_long`・文言は値から作る）。
2. 上限を超えた差し込みは断らず、行に持って「あふれた」行にする（`overflows` / `CommandLine::overflowed` / `SearchLine::overflowed`・application の `overflows_command`）。打っている間は超えた所で 1 行知らせ、Backspace などで上限の内へ戻れば知らせを消してふつうに確定できる。行が持てる最大は上限の 2 倍（`input_line_hold`）で、これを超える差し込みは字を変えずに印 `lost` を立てる。`lost` は Backspace などの編集では消えない（打った・流した文字と行が違うので、戻しても切れた命令になる）。履歴の Up で呼んだ行は履歴の完全な行なので `lost` を持たず（`InputHistory::shown`）そのまま確定でき、Down で打っていた行へ戻ると `lost` も戻る。
3. あふれた行の確定（Enter・再生の `<CR>`・再生の中の `<Esc>` の確定・Ctrl+Enter の全件置換・「検索: 」「置換後: 」の F3 / Shift+F3 / Ctrl+G / Ctrl+T（ADR 0174 の決定 6）・面の候補のクリック）は `refused_overflow` の 1 か所で丸ごと断る。取消と同じに入力行を閉じ（検索の preview の起点へ戻す・`/` の待ちを捨てる・INSERT の `Ctrl-o` を始め直す）、1 行知らせて鳴らし、`refused` を返して再生（`.`・マクロ・`map` の右辺・fixture の :normal!）を打ち切る。本文・レジスタ・マーク・未保存の印には触れない。あふれた行は履歴（#725）に足さないので、履歴の行はどれも上限の内（呼び出しても切れない）。
4. 入口ごとに書かない。打った鍵・再生の鍵・`map` の右辺は `deliver_vim_key` → `accept(CommandText)`、貼り付け（Ctrl+V）と IME の確定と F5 の日時も `accept(CommandText)` → `inserted_command` → `inserted_input` の 1 本を通る。入力行に `<C-r>` のレジスタの差し込みは無い（後続で足すときも同じ口を通す）。
5. 長い入力が知らせの 1 行（256 バイト）を作る所（`fontsize=` `guifont=` の値・`:N` と `.,.+N` の行・面の打った値の候補の名前）は `clipped_display_text`（末尾を code point の境目で落として「…」）で切る。`DisplayText` の生成経路は `parse` のまま。字体の名前（設定に持つ値）は 256 バイトを超えれば今どおり `invalid_font_family`。
6. 描画: 入力行は `write_status` の保持の外で毎 frame 文字組みする（ADR 0070 / 0079）。長い行で全部を組み直さないよう、`draw_command` はキャレットと変換の周りの窓（`input_line_window`・前 1024・後ろ 256 バイト・code point の境目）だけを組む。横の送りは前からある「キャレットが右端を越えたら左へずらす」で、窓の中のキャレットの位置から決めるので見え方は同じ。窓の端で切れた code point は `leading_continuations`（先頭）と `whole_code_points`（末尾）で落とす。1 打鍵の写しは行の複製（最大 8 KiB）が増える。incsearch の preview（と入力中の塗り・Ctrl-G / Ctrl-T）の照合の費用は pattern の長さに比例して増える（`*` を含む pattern は 1 文字ごとに原子の数だけ払う）ので、あふれた行では照合しない（`previewed_line` が値なしを返し、preview は消えて起点へ戻る・Esc の戻り先は変わらない）。上限の内の長い pattern の費用は後続（#1584）。
7. 設定に書く経路: `:map` の右辺は `settings.v4`（上限 16 KiB）に書く。4 KiB の右辺を 4 本書くと設定の上限（`Win32SettingsAdapter` の書く前の検査）に当たり、今の「Settings could not be saved」で断る（閉じた失敗・本文は不変・読みで、実機では確かめていない）。履歴（`history.v1`）と一覧（`session.v2`）は入力行を書かない。
