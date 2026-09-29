# ADR 0056 — 複数タブは、アクティブな文書を今の形のまま持ち、ほかの文書は不変の束として脇に置く

- 状態: 受理（設計席 2026-09-29・Issue #237 / #238 / #239 / #240・**施主決定 D20 / D21 / D22**。決定 2・4・13・14 は #237 の実装で、決定 8 の窓の最小の大きさは #238 の実装で設計席が補正）
- 日付: 2026-09-29
- Issue: #237（状態）・#238（帯とマウス）・#239（鍵と閉じ方）・#240（一覧と Vim の鍵）
- 影響する規則: FR-005 / FR-002 / FR-003 / ARC-001 / ARC-004 / ARC-005 / ARC-007 / ARC-010 / ARC-011 / CPP-002 / CPP-005 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / QLT-014
- 前提: [ADR 0007](0007-first-slice-frameless-window-direct2d-line.md)（窓）・[ADR 0008](0008-adopted-look-tabs-titlebar-statusbar-mica.md)（タブの見た目・寸法）・[ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)（`EditorState` と意図）・[ADR 0010](0010-file-slice-fileport-encoding-detection-atomic-save.md)（開く・保存・未保存の印）・[ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（`VimState`）・[ADR 0023](0023-command-palette-and-shared-input-session.md)（Ctrl+P の面）・[ADR 0035](0035-vim-visual-block-as-column-ranges.md)（Vim の鍵と OS の鍵がぶつかるときの先例）

## 文脈

Nib は 1 つの窓に 1 つの文書で、タブの帯にはタブが 1 本と「＋」がある（「＋」と × は押しても何も起きない）。FR-005 は複数タブ。見た目（幅 120〜200 DIP・高さ 32・アクティブは下線 2・帯は不透明の `title_bar`）は ADR 0008 と D16 で決まっている。

施主決定（2026-09-29・案の画は `docs/design/tabs/`・キャンバス https://claude.ai/artifact/PQgBzS3mYMtTxiUbqTQo2K ）:

- **D20**: タブが帯に収まらないときは、幅 120 DIP で止めて横へ送る。右の「∨」で開いているタブの一覧。
- **D21**: 閉じるボタン × は、アクティブなタブとマウスを載せたタブにだけ出す。
- **D22**: 最後の 1 つのタブを閉じたら窓を閉じる。

設計席が示して施主が異を唱えなかった挙動: ファイルは新しいタブで開く（今のタブが何も書いていない「無題」ならそのタブに開く）・同じファイルは既に開いているタブへ切り替える・Ctrl+T 新しいタブ・Ctrl+Tab / Ctrl+Shift+Tab 切り替え・Ctrl+W 閉じる（Vim モード中は閉じない。× か Ctrl+F4）・Vim のレジスタと「通常 | Vim」の切り替えは窓全体で 1 つ・未保存のタブを閉じるときは今と同じ「保存しますか」・ドラッグの並べ替えと前回のタブの復元は後回し。

現物（Sonnet の probe `out/probes/probe-tabs-2026-09-29.md`・main `c72e144`）:

- 「1 窓 1 文書」の前提は 3 か所に集中している: `EditorState` が本文・選択・履歴・文書の情報・スクロールを 1 つずつ持つ／`EditorFrame.document` が 1 つ／ui が `tab_count = 1` を固定で渡す。
- `EditorState` は不変の値で、`with_*` は次の状態を返す（ARC-005）。1 打鍵ごとに状態を写す。
- `core::TitleBarLayout` は既に `tab_count` を受け、2 タブの配置の契約がある。あふれ・∨・× の領域は無い。
- 「保存しますか」は ui（`EditorWindow::confirm_discard`）が出し、application に確認の意図は無い。
- マウスを載せた状態（hover）はどこにも無い。再描画の口は `invalidate()` の 1 か所で、renderer は `EditorFrame` だけを受ける。
- Ctrl+T は入力行が開いている間は incsearch の `SearchHop` に使っている。Ctrl+W・Ctrl+Tab・Ctrl+F4 は未使用。
- 起動引数の 2 つ目以降のファイルは黙って無視される。
- `tests/unit` は core と application だけに依存する。ui にだけある状態は単体テストで守れない。

## 決定

**アクティブな文書は `EditorState` が今の欄のまま持つ。ほかのタブの文書は、不変の束 `DocumentState` を共有の参照で脇に置く。タブを切り替えるときだけ、今の文書を束にして置き、相手の束を欄へ広げる。これで既存の編集の経路はどれも「アクティブな文書」に対して今のまま動き、1 打鍵の重さはタブの数に依らない。帯の送り量とマウスを載せたタブは application の状態で、ui は意図を送るだけにする。**

1. **文書の束（application）**: `DocumentState { TextBuffer text; Selection selection; EditHistory history; Document document; LineNumber first_visible; std::optional<VimWantedColumn> wanted_column; std::optional<VimCount> scroll_lines; DocumentView view; }`（`src/application/DocumentState.hpp`・1 型）。`view` は置くときに 1 回だけ作るタブの表示値（題名・未保存の印）。束は置いた後は変えない。
2. **`EditorState`**: 今の欄（アクティブな文書と窓全体の値）はそのまま。足すのは `parked_`（`std::vector<std::shared_ptr<const DocumentState>>`・アクティブを除くタブを帯の順で）と `active_`（帯の上のアクティブの位置）と `tab_scroll_`（帯の送り量・DIP）と `title_bar_width_`（ui が知らせる帯の幅・DIP）と `hovered_`（マウスを載せている帯の要素・`std::optional<core::TitleBarTarget>`）。最後の 1 つのタブを閉じる意図の 1 回だけ立つ `closing_` も状態に置く（1 意図で消える `last_failure_` と同じ置き方・#237 の工程 1 の実装席の判断を受理）。`text()` `selection()` `history()` `document()` `with_edit()` などの既存の関数の意味は「アクティブな文書」のまま変えない。状態の写しが写すのは参照の列だけで、ほかのタブの本文と履歴は写さない。
3. **意図（`EditorIntent` に足す・帯の 3 つ `PointTitleBar` `ScrollTabs` `TitleBarWidth` は `VisibleLines` と同じく Vim の報せを消さない。マウスを動かすだけで E486 などが消えないように）**: `NewTab`（空の「無題」をアクティブの右に足して切り替える）・`SwitchTab { std::size_t index }`・`StepTab { core::TabStep step }`（`next` / `previous`・帯の位置の順で端は折り返す・使った順にはしない）・`CloseTab { std::size_t index }`・`PointTitleBar { std::optional<core::TitleBarTarget> target }`（マウスを載せた要素が変わったときだけ ui が送る）・`ScrollTabs { int notches }`（帯の上のホイール）・`TitleBarWidth { int dip }`（`VisibleLines` と同じく窓の寸法を 1 意図で知らせる）。閉じた和型なので写し漏れはコンパイルが落とす（CPP-002）。
4. **切り替え**: 今の文書を束にして `parked_` へ置き、相手の束を欄へ広げる。その前に、開いている入力行（`:` と検索）は取消・IME の変換中の文字列は捨てる・検索の preview は閉じる。Vim の状態は core の純関数 `vim_switched_document(state, wanted_column, scroll_lines)` を通す: 回数・保留のオペレータ・次キー待ち・選んだレジスタ・INSERT の入力の記録・組み立て中の `.` の記録を捨て、モードは NORMAL に戻し（「通常 | Vim」が Vim のとき）、`wanted_column` と `scroll_lines` は相手の文書の値に置き換える。レジスタ・マクロ・録画中の録画・直前の検索と文字検索・`.` の直前の変更・`hlsearch` `incsearch` は窓全体で 1 つのまま保つ。出ていく文書の選択はキャレットへ畳む（VISUAL の選択を持ち越さない）。undo の単位は切り替えの前後で閉じる。切り替えた後、Vim の NORMAL のキャレットは既存の寄せ（`settle_vim_caret`）を通す（INSERT の行末から出た文書へ戻ったとき用）。入ってきた文書は、覚えていた先頭行を今の窓の行数に収めるだけで、キャレットを追ってスクロールしない（通常モードでスクロールだけ動かした位置を文書ごとに保つ）。
5. **開く**: `OpenDocument` は次の順。(a) 同じファイルが開いていればそのタブへ切り替える（本文は読み直さない）。(b) アクティブが「パスが無く、本文が空で、履歴が空」の無題ならそのタブに開く（今の挙動）。(c) それ以外はアクティブの右に新しいタブを足して開く。同じファイルの判定は `FilePort::same_file(a, b)`（adapters が OS の規則で比べる・大文字と小文字を区別しない序数比較。テストの替え玉は文字列の一致）。開けなかったときはタブを足さず、今の「失敗の告知」のまま。
6. **閉じる**: `CloseTab` は確認をしない（確認は ui・今の `confirm_discard` のまま）。閉じた後のアクティブは、閉じたのがアクティブなら右隣（無ければ左隣）、そうでなければ今のまま。最後の 1 つを閉じる `CloseTab` は状態を変えず、`EditorFrame.closing` を立てる（D22・ui がそれを見て窓を閉じる）。ui の流れ: タブを閉じる操作 → そのタブが未保存なら切り替えてから「保存しますか」→ はい（保存できたら）/ いいえ → `CloseTab`、キャンセルは何もしない。窓を閉じる操作は、未保存のタブを帯の左から順に切り替えて同じ確認を出し、キャンセルが出たらそこで止める（それまでに保存したタブは保存されたまま）。
7. **表示値**: `EditorFrame` に `tabs`（`std::vector<DocumentView>`・帯の順）と `active_tab` と `tab_scroll` と `hovered` と `closing` を足す。今の `document` はアクティブなタブの表示値として残す（既存の契約 40 か所はそのまま通る）。未保存の印「● 」は今までどおり題名の先頭。
8. **帯の配置（core の純関数・`TitleBarLayout`）**: 入力は 1 つの値 `TitleBarInput { 帯の幅（物理画素）・DPI・タブの本数・アクティブの位置・送り量（DIP）・マウスを載せた要素 }`（引数の上限 4 つのため）。application は帯の幅と送り量を DIP で持ち、送り量の計算は 96 DPI の配置で行う（DIP と物理画素の写しは今までどおり整数演算の `to_pixels`）。タブの幅は「空きをタブの本数で割った値を 120〜200 DIP に収めた値」。全部が入らないとき（D20）はタブの幅を 120 に固定し、「∨」（幅 32）を出し、タブの並びを送り量ぶん左へずらして、タブの領域の外は描かない。送り量は `tabs_scrolled_into_view(...)` が「アクティブなタブの全体が見える」最小の動きで直す（切り替え・閉じる・開く・帯の幅の変化のたび）。ホイール 1 刻みはタブ 1 本ぶん（122 DIP）で、端で止まる。並びは左から `[タブの領域][∨][＋][掴む余白 40 以上][窓の操作 46 × 3]`。あふれていないときは「∨」を出さず「＋」は最後のタブの右。× の領域（24 × 24・タブの右端から 6）は、アクティブなタブとマウスを載せたタブにだけある（D21）。**窓の最小の大きさ**は幅 360 × 高さ 200 DIP（`core` の純関数が返し、ui が `WM_GETMINMAXINFO` に写す）。掴む余白 40 を数えると、帯の幅 340 DIP 未満ではタブが 1 本でもあふれ、380〜420 では 1 本のタブが幅 200 より縮む（#238 の工程 1 の実装席が気づいた）。最小の幅 360 で 1 本のあふれは起きず、360〜420 で 1 本のタブが 140〜200 に縮むのは受け入れる（掴む余白を優先する）。
9. **帯の hit test**: 要素の種類は今の閉じた enum `TitleBarHit` に `tab_close` と `tab_list` を足す。位置つきの結果は `TitleBarTarget { TitleBarHit hit; std::size_t tab; }`（`src/core/TitleBarTarget.hpp`・`tab` は `tab` と `tab_close` のときのタブの位置）で、`title_bar_target(layout, x, y)` が返す。ui は `WM_NCHITTEST` とクリックと `WM_MOUSEMOVE` で同じ 1 本の関数を呼ぶ。クリックの写し: タブ → `SwitchTab`・× → 閉じる流れ（決定 6）・「＋」→ `NewTab`・「∨」→ 一覧（決定 11）・タブの上の中ボタン → 閉じる流れ。マウスが窓を出たら `PointTitleBar{nullopt}`（`TrackMouseEvent` と `WM_MOUSELEAVE`）。**押した要素を覚える**: タブの切り替えは押したときに、× と「＋」と中ボタンは離したときに動かすが、離したときの要素が押したときの要素と同じ（種類もタブの位置も）ときだけ動かす。判定は core の純関数 `title_bar_released(pressed, released)` で、ui は押したときの結果を 1 つ覚えるだけ（見た目に出ない値なので application の状態にはしない）。右が欠けたタブを押すと切り替えで帯が左へ送られ、同じ点がそのタブの × の上に来る。覚えないと、離した瞬間にそのタブが閉じる（#238 の工程 2 の差分で設計席が気づいた）。本文で押して「＋」の上で離したときも何も起きない。**帯の配置の入力は表示値を作らずに読む**: `WM_NCHITTEST` と `WM_MOUSEMOVE` はマウスが動くたびに来るので、窓は `EditorController::title_bar_input(幅, DPI)`（状態の 4 つの値を読むだけ）を使い、見えている行を組み立てる `frame()` を呼ばない。renderer は表示値から同じ値を作る（`title_bar_input(frame, 幅, DPI)`）。2 つが同じ値を返すことは `--tabs` の契約で守る。
10. **鍵**: Ctrl+T は `NewTab`（入力行が開いている間は今までどおり `SearchHop`）。Ctrl+Tab / Ctrl+Shift+Tab は `StepTab`。Ctrl+F4 は閉じる流れ。Ctrl+W は、通常モードでは閉じる流れ、Vim モードでは何もしない（Vim の Ctrl-W は INSERT の語の削除と窓の命令の接頭辞で、押し間違えてタブが閉じると失うものが大きい。Vim の側の実装は後続）。Ctrl+1〜9 は後続。**鍵の表の形（#239）**: ui は OS の仮想キーを閉じた enum `core::TabKey`（`control_t` `control_tab` `control_shift_tab` `control_f4` `control_w`）に写すだけ（`src/ui/win32/TabShortcut.hpp`・`FontShortcut.hpp` と同じ形）。鍵とモードから命令を決めるのは core の純関数 `tab_command_for(TabKey, EditMode) -> std::optional<TabCommand>`（`TabCommand` は `open` `next` `previous` `close`）で、「Vim モードの Ctrl+W は何もしない」はここに 1 か所だけ書き、契約 `--tabs` が 5 つの鍵 × 2 つのモードを守る。窓はタブの鍵を Vim の Ctrl の表より先に引き、値なしの Ctrl+W はどの表へも流さない。入力行（`:` と検索と Ctrl+P）が開いている間はタブの鍵を引かない。`close` は #238 の閉じる流れ（`close_tab`）の 1 本を通す。
11. **一覧（「∨」）**: 新しい面は作らず、Ctrl+P の面（`CommandPalette`）を「開いているタブ」の候補で開く。候補の表示は題名と場所、実行は Ex の `tabnext {N}`（N は 1 始まりの帯の位置）。`tabnext` `tabprevious` `tabnew` `tabclose` を Ex の表に足す（`:tabclose` は閉じる流れを通すため、controller は「閉じたい」を `EditorFrame` に載せて ui が確認する）。Vim の `gt` `gT` `{N}gt` は `g` の接頭辞の表に足し、効果 `VimStepTab` を controller が `StepTab` / `SwitchTab` に写す。**形（行き先の数え方・失敗・型）は [ADR 0057](0057-tab-destination-is-one-pure-function.md) が決める**（効果の名前は `VimSwitchTab`・`:tabclose` は Nib の閉じる流れを通す）。
12. **色**: `core::Palette` に `tab_hover`（マウスを載せた非アクティブなタブの面）を 1 つ足す。組み込みの 9 テーマは `title_bar` と `tab_active` の各成分の平均（切り捨て）を値として持ち（ダークの茄子色は #27071D）、ユーザーテーマは省略できる（省略時は `tab_active`）。× と「∨」と「＋」にマウスを載せたときの面は `toggle` を使う。ui に色のリテラルは書かない。
13. **起動**: 起動引数のファイルは全部を順にタブで開き、最後に開けたものをアクティブにする。`--measure <json>` の組の読み方は変えない。開けなかった引数はタブにしない。告知は最後に失敗した 1 件だけ（ui の起動の告知は表示値の失敗を 1 件読む・#237 の工程 2 で確認。全件の告知は後続）。引数が無ければ空の「無題」が 1 本。
14. **縦切り（4 本の Issue・どれも main を動く状態に保つ）**:
    1. **状態（#237）**: 決定 1〜7 と 13 の application と core。ui はタブを 1 本だけ描くままで、見た目は変わらない。ただし**窓を閉じるときに未保存のタブを帯の左から順に確かめる所（決定 6 の後半）だけは、この縦切りで ui に入れる**。2 つ目のファイルを開くと見えないタブができるので、入れないと見えないタブの未保存の変更が確認なしで失われる（設計席が工程 2 の差分で気づいて前倒しした）。選ぶのは application の純関数 `next_unsaved_tab` で、ui はその結果で回る。
    2. **帯とマウス（#238）**: 決定 8・9・12 と renderer。タブを複数描く・クリックで切り替える・× と「＋」が効く・hover・ホイール。
    3. **鍵（#239）**: 決定 10 と、`eng/verify-window.py` の検査。
    4. **一覧と Vim の鍵（#240）**: 決定 11。
15. **範囲の外（後続）**: タブのドラッグの並べ替え・窓の外へ切り離す・前回のタブの復元・Ctrl+1〜9・使った順の切り替え・同じ文書を 2 つのタブで見る・外で書き換えられたファイルの検知・Vim の Ctrl-W の実装。

## 強制

- 契約（application と core の純関数）: #237 の分（決定 1 の束・決定 2 の `parked_` `active_` `closing_`・決定 3 の `NewTab` `SwitchTab` `StepTab` `CloseTab`・決定 4・5・決定 6 の `CloseTab`（ui の確認の流れは #239）・決定 7 の `tabs` `active_tab` `closing`・決定 13）は **active**（scope `--tabs` 88 checks・`tests/unit/TabsTests.cpp`・`python eng/protected-diff.py` の scope ごとの checks 数）。帯の送り量・hover・帯の幅（決定 2・3・7 の残り）と決定 8〜12 は **planned**（#238〜#240 の実装で **active**）。
- 閉じた和型の写し漏れ: `EditorIntent`（`NewTab` `SwitchTab` `StepTab` `CloseTab`）は **active**（#237・`std::visit` の網羅性）。`EditorIntent` の残り（`PointTitleBar` `ScrollTabs` `TitleBarWidth`）・`TitleBarHit`・`VimEffect` は **planned**（各 Issue の実装で **active**・`std::visit` と `switch` の網羅性）。
- 1 打鍵の重さがタブの数に依らないこと: **planned**（1 本目の Issue で、タブを 50 本開いた状態の打鍵のベンチを `eng/measure-speed.py` に足すかを実測して決める。足すまでは設計席が merge の前に手で測る・QLT-014）。
- 実機の確認（クリック・hover・鍵・閉じる流れ）: **planned**（3 本目の Issue で `eng/verify-window.py` に足す・QLT-013）。
- core が OS に触れないこと・ui に色のリテラルが無いこと: **active**（既存の `eng/symbols.py` と conformance）。

## 結果

得られるもの: 1 つの窓で複数の文書を開ける。既存の編集・Vim・保存の経路は「アクティブな文書」に対して今のまま動くので、fixture 1844 件と既存の契約は触らずに済む。1 打鍵で写すのは参照の列だけ。帯の送り量と hover が application の状態なので、単体テストで守れる。
失うもの・残る穴: `EditorState` が「アクティブな文書の欄」と「脇に置いた束」の 2 つの形で文書を持つ（切り替えの 1 か所だけが両方を知る）。マウスを載せた要素が変わるたびに意図が 1 つ流れる（動くたびではない）。同じファイルの判定は大文字と小文字だけで、短い名前（8.3）とリンクは別のファイルとして開く。Vim モードでは Ctrl+W でタブを閉じられない。タブを切り替えると VISUAL の選択と組み立て中の命令は消える。一覧は Ctrl+P の面なので、「∨」の真下に出る小さい面（案の画）とは位置が違う。

## 縦切りを切るときの条件

どの縦切りの後でも、main に「使う人が書いたものを確認なしで失う道」が無いこと。縦切りの順を決めるときは、各段階の後の姿で、見えない状態（描かれないタブ・届かない鍵）が未保存の変更を持ちうるかを確かめる。持ちうるなら、その変更を守る所（確認・保存）を同じ縦切りに入れる。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `EditorState` をタブの数だけ持つ | 窓全体の値（テーマ・設定・モード・Vim のレジスタ）がタブごとに複製され、どれが正かを決める同期が要る（ARC-001） |
| 全部のタブの文書を `EditorState` の中に値で並べる | 状態は 1 打鍵ごとに写すので、写す量がタブの数と履歴の長さに比例する。速さのゲート（QLT-014）に乗る |
| 既存の `text()` `with_edit()` などにタブの位置の引数を足す | 編集・Vim・保存の全部の経路（`accept` のほぼ全部）を書き換えることになり、fixture と契約の全部が揺れる。アクティブな文書だけが編集の対象である間は要らない |
| hover と送り量を ui の一時の値にして、renderer に 2 つ目の引数を渡す | ui にだけある状態は単体テストで守れない（tests は core と application だけに依存）。renderer の入力が 2 本になる |
| 「保存しますか」を application の状態と意図にする | 今の確認は ui の 1 か所で動いている。確認の面を Direct2D で描く仕事が増え、タブの縦切りの外 |
| Ctrl+Tab を使った順（MRU）にする | 帯の並びと押した結果が合わず、予測しにくい。位置の順の方が画と一致する。使った順は後続の候補 |
| 「∨」の一覧のために新しい面を作る | Ctrl+P の面が既に候補の一覧・絞り込み・鍵の操作を持っている。面を 2 つ作らない（ARC-001） |
| Vim モードでも Ctrl+W でタブを閉じる | Vim の INSERT の Ctrl-W（語の削除）の手癖で、未保存の確認つきとはいえタブが閉じる。施主に説明したとおり Vim の鍵を優先する |
