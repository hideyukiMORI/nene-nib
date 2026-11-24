# CLAUDE.md — NeNe Nib

Claude Code / AI エージェントがこのリポジトリで作業するための**中核ハンドブック**。
簡潔な英語版の入口は [AGENTS.md](AGENTS.md)。詳細の正本は `docs/` にあり、ここには複製しない。
このリポジトリのコード名は **Nib**（設計席は「Nibリナ」）。

---

## 0. まず読むもの（production コードに触れる前に必ず）

0. [SPECIFICATION.md](SPECIFICATION.md) — 何を作るか（FR-NNN・施主決定 D1〜D10）
1. [docs/ARCHITECTURE_CONSTITUTION.md](docs/ARCHITECTURE_CONSTITUTION.md) — 憲章（ARC-NNN）
2. [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md) — モジュールと依存方向
3. [docs/CODING_RULES.md](docs/CODING_RULES.md) — C++23 (clang-cl) 規約（CPP-NNN）
4. [docs/QUALITY_GATES.md](docs/QUALITY_GATES.md) — **いま何が機械で守られているか**（QLT-NNN / CNF-NNN）
5. [docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) — 手順
6. [docs/COMMIT_CONVENTIONS.md](docs/COMMIT_CONVENTIONS.md) — Issue・ブランチ・コミット・PR（GIT-NNN）
7. [docs/GLOSSARY.md](docs/GLOSSARY.md) — 用語
8. 該当する ADR（`docs/adr/`）と有効な waiver（`docs/waivers/`）

---

## 1. このリポジトリの統治原則

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

その帰結として、次の 3 つを常に守る。

1. **正典の経路を先に特定してから編集する。** 「ここで書いたほうが早いから」で第 2 の経路を作らない（ARC-001 / ARC-012）
2. **ゲートを弱めて通さない。** 検査が落ちたらコードを直す。閾値・除外・重大度を触るのは ADR 相当の判断（QLT-010）
3. **`planned` を `active` と書かない。** 未実装の強制を実装済みに見せるのは、この規約体系で唯一「壊す」行為（[ADR 0001](docs/adr/0001-strictness-is-mechanically-enforced.md)）

---

## 2. このプロジェクトで間違えやすい所

### コンパイラは clang-cl だけ

`cl` は Phase 0 の比較対象としてだけ残っている（[ADR 0003](docs/adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)）。`eng/toolchain.ps1` が `CXX=clang-cl` を固定する。
製品も測定ビルドも将来の md4c も同じ 1 本。警告集合は `eng/targets.cmake` の 1 か所で、`eng/probes/language.json` の `clangStrict` と同じ並び。

### 現在時刻・ファイル・スレッドを持てる場所は 1 つしかない

現在時刻・乱数・既定ロケール・環境変数・ファイル・**スレッド**を持ってよいのは **`src/adapters/win32`** だけである（ARC-007 / ARC-003 / CPP-013）。
中核で必要なら、型のあるポートから注入する。**テストが実時刻を読むことも決定性の破壊である。**
検査はソースの名前ではなく **リンカのシンボル**で行う（`eng/symbols.py`）。`system_clock::now()` は `_Xtime_get_ticks`、`std::thread` は `_beginthreadex` として現れる。
`std::atomic` はリンカに見えないので、`<atomic>` の include を字句検査（CNF-009）が拒否する。

### 裏の仕事はワーカー 1 本と型のある port。合図は使う人の操作ではない

スレッドは `src/adapters/win32` の `Win32Worker` の 1 本だけで、最初の仕事を受けたときに起きる（ADR 0004 / 0062。合成ルートが起動のたびに最初の仕事として device の生成を投げるので、製品では起動で起きる・ADR 0131）。仕事を足すときは仕事の種類ごとの port を application に宣言する（最初は `FolderPort` の `list` と `collect`）。
完了の知らせは中身の無い窓メッセージ（番号は `EditorWindowLifecycle.cpp` の `work_message` の 1 か所・#1073 で `EditorWindow.cpp` を 6 翻訳単位に分けた）で、ui は意図 `WorkCompleted` を送り、中身は application が `collect()` で引く。`send` の途中で届いた合図は、いちばん外の `send` の終わりで 1 回だけ送る。
`WorkCompleted` は Ctrl+Tab の歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない（入口の `keeps_tab_walk` と `begin_intent`・契約 `--background-work`）。新しい意図を足すときは、この 2 か所にどちらの側かを決めて書く。
adapters の試験は時計を読まない。待つのは合図の受け皿（セマフォ）で、止まらなければ CTest の `TIMEOUT` で落ちる。

### 最初の frame は WARP で子窓に描く。本窓に WARP を結ばない

箱 `DeviceWarmup` は WARP（`take_provisional`）と本物（`take`）を 2 段で渡す。本物がまだなら `EditorWindow` は本窓の client を覆う子窓 `CoverWindow` に WARP の renderer を結び、`pump_messages` が箱の event を待って `adopt_device` で本窓に本物を作ってから子窓を壊す（節目 `device_adopted`）。
本窓の HWND に WARP の swap chain を一度でも結ぶと、差し替えた後も単打が 2 倍のまま残る（ADR 0132）。device lost の作り直しは古い renderer を先に解放する。

### 自動退避は合図・純関数の計画・型のある port。変わったかは改訂番号で比べる

ui のタイマーが送る意図 `AutosaveTick` は `WorkCompleted` と同じく使う人の操作ではない（`keeps_tab_walk` と `begin_intent` の両方に書いてある）。何を書くかは `autosave_plan(state, ledger)` の純関数が決め、書くのは `AutosavePort` の worker（ADR 0137）。
タブの名指しは `EditorState::autosave_ids()`（帯の順・通し番号・再利用しない）。帯の位置・パス・題名で名指ししない。「前回の退避から変わったか」は `EditHistory::revision()` で比べ、履歴の位置では比べない（まとめられた打鍵と undo の後の別の編集で位置は同じ値に戻る）。
本文のバイト列は保存と同じ `encoded_parts` の 1 本で作る。起動・開く・切り替え・保存・打鍵の道は `AutosavePort` を呼ばない（契約 `verify_quiet_paths`）。

### 網羅性検査を殺す分岐を書かない

閉じた選択肢の分岐に `default` / `else` / `_` を書かない。選択肢が増えたらコンパイルが落ちるのが正しい状態（CPP-002）。
`DefWindowProcW` へ渡す OS メッセージの既定分岐だけは例外（CPP-017）。

### 期待される失敗は例外にしない

検証エラー・見つからない・拒否・非互換・device lost は `std::expected` か閉じた `enum class` で返し、`[[nodiscard]]` を付ける（ARC-010 / CPP-005）。

### `std::optional` は `value()` で読む

MSVC STL の `optional` は `operator*` を clang-tidy が見ない（Phase 0 の T3-tidy-unchecked-optional-star-hole）。`value()` / `value_or()` で読み、`*` と `->` を書かない（CPP-004）。

### COM は `-Wno-language-extension-token` の上で書く

`__uuidof` / `IID_PPV_ARGS` は `-Wpedantic` で落ちる（W1）。名指しで外してあるので、警告集合を自分で触らない。COM の所有は `ComPtr`、device lost は結果型（CPP-017）。

### SIMD は `src/core/simd/` にだけ書く

関数ごとに `[[gnu::target("…")]]` を付け、fallback を必ず置く。target 単位の `/arch` は付けない（CPP-018・[ADR 0006](docs/adr/0006-speed-gate-simd-and-table-driven-dispatch.md)）。

### 大きな分岐は表で書く

60 分岐の `switch` は関数長 60 行で落ち、`constexpr` の表は通る（T8）。Vim のキー列 → 動作は表（CPP-012）。

### 公開 aggregate に `= default` の `operator==` を書かない

`RgbColor` / `Palette` のような公開 aggregate はメソッドを 1 つでも持つと lint が落ちる（Issue #3）。比較は非メンバーで書く（CPP-003）。

### `reinterpret_cast` は書けない。`std::bit_cast` と `ComPtr<IUnknown>` ＋ `As()`

clang-tidy が一律に拒否する。HWND ↔ `this` と `LPARAM` の読み替えは `std::bit_cast`、`IUnknown**` を要求する API は `ComPtr<IUnknown>` で受けて `As()`（CPP-009）。

### `.cpp` の無名名前空間の `struct` も 1 ファイル 1 型に数える

ローカルの補助は自由関数と `using` 別名で書く（CPP-011）。

### ui/win32 に色のリテラルを書かない

色は `core::Palette` のトークンだけ（テーマ拡張の前提・ADR 0008 決定 8）。足りない色はトークンを 1 つ足して採用案の表に書く。

### 窓の背景に Mica を掛けない（D86）。通常の窓に `DWMWA_SYSTEMBACKDROP_TYPE` を足さない

Windows 11 build 26300 では DWM が窓の中身の visual だけを約 4 px 丸く抜き、抜けた所から Mica が明るい点に見えた（#1488・実機の切り分け a / b / c）。窓は `WS_EX_NOREDIRECTIONBITMAP`（DirectComposition の絵だけ）と `DWMWA_USE_IMMERSIVE_DARK_MODE`（縁と影の色）と `DWMWCP_DONOTROUND` のままで、下地は無い。renderer が無い間（最初の frame の前・device の差し替え）は窓の後ろが透ける。描画を失って「保存しますか」を聞く間の下地だけは `WindowBackdrop` が掛ける（#1499・この 1 か所が例外で、消さない・増やさない）、残る約 4 px の丸みは #1496（ADR 0008 / 0013 の追補）。

### core で `std::lround` を呼ばない

libm のシンボルが core の外へ出て ARC-003 が落ちる。DIP → 物理画素は整数演算（ADR 0008 決定 5）。

### `std::string_view::find` などの STL の検索は許可シンボルに載っている

MSVC STL は `__std_find_trivial_*` 等の純関数を core の外へ出す（Issue #7 で実測）。`eng/symbol-allowlist.json` に足してある。他の `__std_*` が出たら「時刻・OS・スレッドに触れないか」を確かめてから足す。

### 意図は `std::variant` の閉じた和型

`EditorIntent` は `std::visit` で写し、選択肢が増えたらコンパイルが落ちる（ADR 0009）。方向や操作の種類は閉じた `enum` を持つ型に畳む。

### レジスタの書き手と読みは 1 本ずつ

書くのは `registers_written`（「名指しの書き込み → `"1` の規則 → `"-` の規則 → 無名」の順）、読むのは `register_read(state, selection)`、名前 → 選択は `register_selection_of`（`"` と `@` が共用）。
新しいレジスタは `VimRegisterTarget` に 1 値足し、`switch` が落ちた所を直す。呼び出し元で「`"1` へ行くか」を数えない（ADR 0050）。

### engine は OS のクリップボードに触れない

`"+` `"*` は controller が `vim_step` の直前に `ClipboardPort` から読んで写し（`VimState.clipboard`）を置き、engine が書いた本文は `VimStep.clipboard` に添えて返す（ADR 0051）。
本物の Vim の `"+` は `-es` でも実機のクリップボードを書き換えるので、fixture・probe・テストに `"+` `"*` を入れない。テストは替え玉 `ScriptedClipboard` だけ。実機を触る調査は施主の了承の後。
書き込みの失敗は知らせにし、変換できない本文は書かない（ADR 0134）。

### `u` の後のキャレットは `Edit.restore`。`edit.at` から決めない

戻り先を書くのは `EditorController::replace` の 1 か所（そのときのキャレット）。オペレータと VISUAL は engine が `VimStep.restore` に「Vim が最初の変更を保存する瞬間のカーソル」を添え、controller が効果を写す前にキャレットをそこへ置く（ADR 0052）。
`Edit` を作る所は `restore` を必ず書く（既定値なし）。oracle は鍵を 1 本の `:normal!` に流すので、`u` を含む fixture は変更が 1 つだけの鍵列で書く。

### Vim の 1 文字は `vim_character_end` / `vim_character_start`。`next_code_point` で歩かない

Vim の 1 文字はコードポイント 1 つと直後に続く幅 0（`DisplayWidth::zero`）のコードポイントの列（ADR 0053）。判定の表は仮想桁の表の 1 つだけで、結合文字のための表を作らない。
`next_code_point` / `previous_code_point` はバイト列の走査（表示・UTF-16 の変換・照合器）のためのもの。1 打鍵ごとに呼ばれる所は行の全体を `text_range` で写さない（キャレットの周りの窓だけを読む）。

### fixture の記法（`<Esc>` `<NL>` など）を足すのは `eng/vim-oracle.py` の `KEY_TABLE` の 1 行だけ

C++ の表 `src/core/VimKeyNames.hpp`（#1082 で `tests/vim` から core へ移した・`map` の設定の行も同じ表で読む）は生成物で、手で書かない（`python eng/vim-oracle.py --key-names`・ADR 0054・CNF-010）。
表は oracle の測定の領域にあるので、表を変えた commit を `--reuse-ref` にしてから fixture を部分再生成する。
fixture の `settings` は生成物の行の最後の欄に載り、再生（nib_tests・窓の道・`nib_vim_replay`）が窓の後・鍵の前に `:` の行として流す。Nib が断る行は試験が落ちる。流さないのは oracle の `NIB_FIXED_SETTINGS`（Nib の固定値と同じ行）だけで、余白の `set scrolloff=N` も同じ道（#1626・ADR 0012 の追記）。
`-es` の Vim は失敗した検索（`nowrapscan` の E385 など）で終了コード 1 を返して測れないので、そういう fixture は viewport つき（`--not-a-term`）で書く。
同じ鍵に 2 つの名前を足さない（`<C-i>` は `<Tab>`）。表の名前は鍵と 1 対 1 に往復し、`map` の一覧の表示名はそこから引く（#1081 の契約・#1037 で 1 件落ちた）。
fuzz（`eng/vim-fuzz.py`）の生成器 3 の `WORDS` に語を足さない（固定 seed の列が変わる・`WORDS_4`（生成器 4）と `WORDS_5`（生成器 5・viewport と画面移動）も同じ・`WORDS_6`（生成器 6・`<C-e>` `<C-y>`・`{ } ( )`・Del・レジスタの回数・#1353）も同じ・足すのは次の版（生成器 7）・#1135 / #1224）。oracle の測定コードを変えた PR（#1116 / #1124 の形）は `--only` が「全件を再生成せよ」で拒むので、乗せ替えでは枝の commit を `--reuse-ref` にして main 側にだけある fixture を測る（逆に main 側が oracle を変えていれば main を `--reuse-ref` にして枝の分だけ測る・`D:\NeNeNib\scripts\20261030\resolve-vim-conflicts.py`）。oracle の `-es` は Windows 版の既定 `isprint=@,~-255` で DEL を 1 桁に数える（Unix 版は `^?` の 2 桁・#1119 で Windows を正本に決めた）。

### 編集の経路が触るのは「アクティブな文書」だけ。ほかのタブは不変の束

`EditorState` の `text()` `selection()` `history()` `document()` `with_edit()` はアクティブな文書のもの。ほかのタブは `DocumentState` の束を `shared_ptr` で脇に置き、切り替えの 1 か所だけが置いて広げる（ADR 0056）。
タブの位置を引数に取る編集の関数を足さない。帯の送り量とマウスを載せた要素は application の状態で、ui は意図を送るだけ。Vim のレジスタと「通常 | Vim」は窓全体で 1 つ。

### タブの行き先は 2 本。帯の位置の順は `tab_destination`、最近使った順は `TabRecency`

Vim の `gt` `gT`・Ex の `tabnext` `tabprevious`・一覧の実行は `tab_destination`（ADR 0057）。Ctrl+Tab は `tab_recency_walked`（ADR 0058）。
呼び出し元で「次のタブ」を数えない。使った順を直すのは `EditorState` の切り替え・足す・閉じるの 3 か所だけで、歩きの途中（`WalkRecentTab`）は列を入れ替えない。
どの意図が歩きを終わらせるかは `EditorController` の入口の 1 か所が決め、ui には書かない。

### 帯の上のマウスは、押した要素と離した要素が同じときだけ動かす

タブの切り替えは押したとき、× と「＋」と「∨」と中ボタンは離したとき。押して帯が送られると、同じ点に別の要素が来る（`title_bar_released`・ADR 0056 の決定 9）。
マウスの動くたびに通る所（`WM_NCHITTEST`・`WM_MOUSEMOVE`）は `controller_.frame()` を呼ばず、`title_bar_input(幅, DPI)` を読む。

### まだ読んでいないタブを読むのは `reach_tab` の 1 本。状態はファイルに触れない

前回のタブは「まだ読んでいない文書」`UnloadedDocument` として脇に並ぶ（`ParkedTab` は束とこれの和型で、中身を読む所は `std::visit`・ADR 0059）。
タブが切り替わる入口（`SwitchTab`・`WalkRecentTab`・`CloseTab` の後の隣・起動）は、状態を動かす前に `EditorController::reach_tab` を呼ぶ。読めなければ帯から外して 1 行知らせ、切り替えない。
`EditorState` はまだ読んでいない文書へ切り替えない（指されたら何も変えない）。切り替えの入口を足すときは、先に `reach_tab` を呼ぶ。
一覧 `session.v2`（v1は不在時の読込だけ・ADR 0114）を書くのは窓を閉じるときの意図 `EndSession` だけ。道具は `eng/window_driver.py` の `start` が起動の前に自分の profile の一覧を消す（復元の検査だけ `keep_session=True`）。

### Ctrl+P の出どころを足すのは、記号の表に 1 行・候補の列に 1 種類

面の候補は開くときに作る 1 つの列で、入力の行頭の記号（`#` タブ・`@` 履歴・`/` 同じフォルダ・`:` 設定）がどの出どころを見せるかを決める。記号の表は core の `palette_marks` の 1 つで、案内の文字列も同じ表から作る（ADR 0060）。呼び出し元で先頭の文字を見ない（`palette_query_of`）。
出どころを足すときは、`PaletteScope` と `PaletteOrigin` に値を足して落ちた `switch` を直し、controller の `palette_entries()` に候補を足す。絞り込みと順は `listed_positions` の 1 本（候補の列の中の位置を返す）で、`std::stable_sort` は使わない（ARC-003）。面の結果は `CommandPalette` が 1 回だけ作って持ち、読む口は `count` `choice_at` `rows`（全件を写さない・ADR 0062）。
一覧から開くのは `open_listed`（失敗は 1 行の知らせ）、Ctrl+O と起動引数は `accept(OpenDocument)`（失敗はダイアログ）。開く道はどちらも `open_document` の 1 本。

### 履歴を書くのは閉じたときだけ。起動と開く道でファイルに触れない

閉じたファイルの履歴 `history.v1` は、タブを閉じたときと窓が閉じるときに `remember` が「読む → 足す → 書く」（ほかの窓の分を失わない）。読むのは Ctrl+P の面を開くときの 1 回。
起動・開く・切り替え・保存・打鍵では読まない・書かない（契約 `verify_quiet_paths` が port の呼ばれた回数を数える・ADR 0060 の決定 8）。
application の失敗の enum は `FileHistoryFailure`（core の `HistoryFailure` は undo の端）。ADR に型の名前を書く前に、同じ名前が無いか grep する。

### 右寄せの文字は `write_right`。幅を測って原点をずらさない

`status_format_` はもともと右寄せ（TRAILING）。欄の幅の text layout を作ってさらに原点を右へずらすと、右寄せが二重に効いて欄の外へ出る（#258 の差し戻し）。

### 本文の文字組みは `BodyTextLayouts`。描画と位置解決で共用する

合成ルートが1個を所有し、applicationの`TextLayoutPort`とrendererが同じDirectWrite layoutを使う（ADR 0114）。書式は行高・DPI・字体・折り返し設定（幅は「0 より大きいか」だけ）を鍵にし、entry は通常行・IME差し込み済み行の完全な表示文字列と幅を鍵に現在/直前のframeと最後の入力行だけを保持する（2 枠の幅が違っても捨て合わない・古い幅の entry は使わず frame の終わりに消える・ADR 0148 の 2026-11-22 の追補の決定 6）。書式変更で破棄し、横位置が変われば同じlayoutから可視範囲の字形を再収集する。選択/検索で共有layoutの書式を変更せず、右寄せ・コマンド入力のlayoutをこの保持へ入れない。格子の画面行も字形は同じ`BodyTextPainter`で描く（ADR 0140 の決定 5・#569）。

### ステータスの文字組みは `write_status`。保持するのは最大12枠

通常/Vim・モード・録画・通知・右側の4項目（位置・文字コード・改行・言語）・診断の件数と操作の案内は描画器の固定枠で、全文・書式・幅・高さが一致するとき再利用する（ADR 0070 / 0079 / 0154 / 0161 の決定 4）。枠の数の正本は `StatusLayoutDemand.hpp` の `status_layout_capacity`（12）。案内は鍵と説明の4枠を使い最大12（件数の枠を含む）、録画とは排他なので録画時は最大9。件数が 0 の frame は件数の枠を使わない。枠の位置は `draw_status_bar` の入口で戻し、UI書式の再作成前に全枠を消す。色と原点は毎回反映する。コマンド入力のキャレットや補完のlayoutをこの保持へ入れない。

### 設定の正本は `settings.v6`。旧版を読むのは新版が無いときだけ

v6 は v5 の全部の鍵に、`finder_preview` の後ろの `caret_motion` と `caret_motion_time`（ADR 0192 の決定 1・#1933）と `caret_sparks`（ADR 0192 の追補・#1979）、その後ろの `pane_resize_step` と `pane_resize_step_vim`（ADR 0148 の 2026-11-23 の追補・#1972）と、その後ろのインデントガイドの `indent_guide_style` `indent_guide_colors` `indent_guide_active` `indent_guide_width`（ADR 0193 の決定 4・#1975）を足した版。v5 は v4 の全部の鍵（既存 5 鍵に `ui_language` `lsp` `explorer` `root` `features` と後から足した鍵・任意の `lsp.<language>.command` の行・ADR 0170 決定 3・上限 16 KiB）に、`explorer_width` の後ろの `autoindent` と `finder_preview`（ADR 0189 の決定 1・#1785）を足した版（ADR 0170 の 2026-11-16 の追補・#1086）。`settings.v5` は 1.1.0 が読み書きする版、`settings.v4` は 1.0.0 が読み書きする版で、どちらも鍵の集合は凍結した。**v5 と v4 の読みを緩めない・v5 と v4 に鍵を足さない**（1.0.0 と 1.1.0 は知らない鍵が 1 つでもあると既定の見た目で始めて保存を断る・D91）。**v6 を書く最初の公開版はまだ決まっていない**（次の版）。公開までは v6 に鍵を足してよく、v6 を書く版を公開したらそこで閉じる（その後に足すなら `settings.v7`）。版は `SettingsVersion` に 1 値、読む順と lock・bytes の照合の順は `Win32SettingsAdapter` の `reading_order` の 1 か所（v6 → v5 → v4 → v3 → v2 → v1）。v6 不在時だけ v5、順に v4・v3・v2・v1 を厳密に読み、v5 以前は `caret_motion=off`・`caret_motion_time=120`・`caret_sparks=off`・`pane_resize_step=5`・`pane_resize_step_vim=off`、v4 以前は `autoindent=off`・`finder_preview=auto`、v3 以前は `ui_language=ja`・`lsp=off`・`explorer=off`・wrap は既定オン・v1 の guide は既定表示に補う。設定を変更するときだけ v6 へ書き、旧ファイルを bytes のまま残す。存在する新版が壊れていれば旧版へ戻らず保存を拒否し、移行保存時も読込元まで lock と bytes を照合する（ADR 0114）。1.1.0 の書く v5 の見本は `nib_adapter_tests` の `released_v5_settings`（`tests/adapters/SettingsVersionSixTests.hpp`）、1.0.0 の書く v4 の見本は `released_v4_settings`。案内の文字と鍵は `operation_text` と `shown_chord` が正本で、rendererに文字列の別表を書かない。

### 本窓を壊すのは `leave` だけ。描画を失っても「保存しますか」を通す

`DestroyWindow(window_)` を呼ぶのは `EditorWindow::leave` の 1 か所（デストラクタの作りかけの後始末を除く）。終わり方ごとの未保存の扱いは application の `exit_plan(ExitCause, 未保存の数)` が決め、ui は値に従うだけ（ADR 0133）。OS の終了（`WM_QUERYENDSESSION`）は未保存があれば `ShutdownBlockReasonCreate` で止め、`end_window` を通さず窓を壊さない。
描画の失敗は `recover()`（古い renderer を先に解放して `start_rendering`）で 1 回だけ作り直し（`present` は device lost のときだけ、サイズ・DPI・文字組みは失敗の種類を問わず）、だめなら `abandon()` が取消の無い「描画できなくなったので閉じます。保存しますか。」を通して `leave(SessionEnd::rendering_lost)` する。

### 一時ファイルは `<target>.nib-tmp-<pid>-<n>` を `CREATE_NEW` で作り、置き換えが成功した後に残骸を回収する

名前と回収は `src/adapters/win32/TemporaryFiles` の 1 か所（ADR 0135）。連番はプロセスで 1 つ（`next_temporary_sequence`）で、名前が既にあれば最大 8 回まで次を試す。回収は名前が `<16 進の pid>-<10 進の連番>` の形で、pid のプロセスが生きていない（`OpenProcess`・拒否は生きている側）か、自分の古い連番のものだけ。判定は純関数 `reclaimable_temporary` の 1 本で、時刻で決めない。起動時は合成ルートが設定の置き場で `reclaim_temporaries_beside` を 1 回呼ぶ。置けなかった後の始末は `placement_cleanup` の 1 本で決め、新しいバイト列を消さない（#1689）。固定名の一時ファイルを書かない。

### 画面の文は `ui_text` の表。直書きしない（CNF-016）

画面に出る文は `core::UiMessage` の名前で `ui_text(message, language)`（差し込みは `ui_text_filled` の `{0}` `{1}`）から引く。足すのは `UiMessage` に 1 値と、3 列（`UiTextsJapanese.hpp` `UiTextsEnglish.hpp` `UiTextsSimplifiedChinese.hpp`）の同じ位置に 1 行ずつ。最後の値の後ろに足したら `ui_message_count` の名指しも移す（列挙子の数・最後の値・3 列の行の数と順は `eng/conformance.py` の CNF-016 が照合する）。`src/**` の文字列リテラルに日本語・中国語を書くと CNF-016 が落ちる（許した場所は `eng/conformance-rules.json` の `uiTextLiteralAllowedPaths`・`static_assert` の文は英語）。言語は状態の `display_language()` の 1 つで、frame は `EditorFrame.language`、ui の箱は `EditorController::display_language()`。OS を観測するのは起動と `:set language=system` / `&` だけ（ADR 0123 の 2026-11-13 の追補）。Vim の報せ・モード名・鍵の名前は訳さない。

### 2026-11-10 に 1 本にした所（足すときはここ）

- **合図かどうか**は `detail::is_signal` の 1 本（`WorkCompleted` `AutosaveTick` など使う人の操作でない意図）。狙う横位置・面のクリックの印を捨てるかもここを読む（#1296）。
- **「保存しますか」**は ui の `ask_save`（TaskDialog）の 1 本で、文言は core の `save_question_text`、答えの後の一歩は `save_step`（#1282）。`MessageBoxW` で聞かない。
- **map の右辺**は生のまま持ち、流す時点のモードで 1 段だけ引き直す `rhs_unit_step`（#1276）。録画は打った鍵で、再生は今の表を引く（#1312）。
- **窓（バイト列の一部）を decode に渡す所**は `Utf8.hpp` の `whole_code_points` の 1 本（#1292・窓の端でコードポイントを割らない）。
- **窓を閉じる手順**は `closing_steps` の列を `run_closing_steps` が順に流す（先に窓を隠してから一覧と履歴を書く・#1340）。OS の終了（`WM_ENDSESSION`）では窓を隠さない。
- **CTest の `nib_unit_scopes`** は `nib_tests --every-scope`（全部の名指しの scope）。fixture を足したら、その fixture を選ぶ scope の件数の検査（`--vim-macro` は register 欄を持つ fixture の数、など）が動く（#1324）。
- **通常モード（Vim でないモード）の振る舞いは Windows の標準**（メモ帳・ブラウザの入力欄）に合わせる（施主決定 D82）。Vim のモードは oracle のまま。標準の細部は、直す前に実測する。

### リポジトリの外の作業場所は `D:\NeNeNib\`

依頼書は `D:\NeNeNib\briefs\`、1 回限りのスクリプトは `D:\NeNeNib\scripts\`、実機の確認の profile と文書は `D:\NeNeNib\evidence\`（施主指示 2026-09-30。C ドライブの Temp と scratchpad を使わない）。道具の出力と席の報告はリポジトリの `out/`。
D は HDD で、そこに置いた profile では保存の書き込みが遅い（履歴の 1 回が 30〜40 ms・最初は数百 ms）。撮影のスクリプトは、書き込みを伴う操作の後に 1.5 秒待つ。

### `HKCU\Software\NeNeNib` は「登録を外す」が丸ごと消す。ほかのものを置かない

エクスプローラーへの登録（ADR 0178・#1087）は `Software\NeNeNib`（`Capabilities`）と `Classes` の下の自分の木を `RegDeleteTreeW` で根ごと消す。設定や履歴をレジストリに置かない（置き場はファイル・ADR 0020）。登録の鍵と値は core の `shell_registration_plan` の 1 本で、adapter は計画を書くだけ。

### 改行の形は 2 つ。本文の形は `TextBuffer`、保存の形は文書の値で `encoded_parts` が写す

本文は改行を生のバイトで持ち、`TextBuffer` の `LineEnding` は読んだときの形（`o` や Enter が挿入する形）。保存の形（`:set ff=`・ステータスバーのクリック・F1 の操作で選ぶ値）は `Document.line_ending` で、本文の形と違うときだけ `encoded_parts` が `line_endings_converted` で写す（lone CR は残す・#1049・ADR 0010 追補）。本文を書き換えて形を変えない（undo と保存時点の位置が壊れる）。未保存の判定は `save_state_of` の 1 か所（履歴の位置 or 保存時点の文字コードと改行の形と違う）。

### 本文の字体・字形・幅の高速化も同じ正本に閉じる

字体選択はOSのMapCharactersだけ。FontFallbackCacheは完結した同じ要求のS_OKだけを最大128件再利用し、拡張sourceや未知の取得失敗は委譲する（ADR 0071）。
BodyGlyphCollectorはDirectWriteが決めた所有字形と位置をそのまま保持する。完全に画面外のrunだけ保守的に除き、未対応装飾は収集全体を棄却する。Tabの0字形をDrawGlyphRunへ渡さない（ADR 0073）。保持の run は文字の位置・cluster・advance も持ち、置き換えの描画は本文と同じ鍵の entry を引く（#531）。
display_widthのBMP索引はDisplayWidthRangeの正本からコンパイル時生成する。意味の表・実行時初期化・別の呼出し経路を増やさない（ADR 0074）。SDKが固定したCOMの署名は `// SDK-ABI: <Interface>::<Method>` と `NOLINTNEXTLINE(readability-function-size)` の2行で受け、本体6行以内で中へ渡す。印は `eng/sdk-abi-signatures.json` の表にあるものだけ（CPP-019・CNF-012・ADR 0076）。
長行の高速化の main 統合は、独立レビュー・正式な速さのゲート・CI・施主の試用の 4 つで受理した（ADR 0075・D41）。前の実験の `hold` は記録として残す。split は D64 で採用済み（ADR 0148）。根拠は[gate-proofs 5-cm](docs/quality/history/5-cm-font-glyph-width-split-reeval.md)と[5-cq](docs/quality/history/5-cq-improvement-bundle-acceptance.md)。

### 巨大な行のしきい値と仮想桁の部分和は core の 1 か所

巨大な行（表示文字列が 16,384 コードポイントを超える行）のしきい値は `GiantLine.hpp` の `giant_line_threshold` の 1 つで、窓 `giant_line_window`（錨を 2,048 刻みに切り下げた中心の前後 4,096・幅は引数にしない）も同じ所で、行を写さず `VirtualColumnIndex` の序数から引く（ADR 0139 / 0140）。表示形式 `DisplayForm` は文書ごとの値で、設定に入れない。
行頭からの仮想桁と格子の升の累積は `TextBuffer::virtual_columns()`（`VirtualColumnIndex`・約 64 KiB の塊ごとの要約）で引き、行を写して歩かない。索引は要る文書だけが `with_virtual_columns()` で持ち（既定は無し・持たない本文に問うと `VirtualColumnsFailure::not_held`）、持つ本文の `replaced` だけが触れた塊を読み直す。表示できる ASCII と改行だけの塊は歩かず改行の索引の行頭から引き算し、Tab と制御文字を含む塊は歩く。1 文字の桁は `column_advance`（`display_width` の表・Tab の止まりは `vim_tab_stop` の 1 つ）、格子の送りは `grid_placed_cell` の 1 本。格子の送りは幅 `columns` に依るので索引に持たせず、application の `GridCells` が借りた索引から答える（plain の塊は足すだけ・1 字を繰り返した一様な塊は要約の `uniform_bytes` `uniform_cells` から閉じた式・送りの対象の無い塊は要約の終わりの桁から足す・歩くのは問いの升が掛かる混じった塊だけ・同じ行の塊の始まりを覚えて行を 1 回だけ歩く・ADR 0140 の 2026-11-16 の追補）。frame は `GridCells` を 1 つ作って `GridLineRows` で見えている画面行を切り出し、索引を作り直さない（`EditorFrame.grid_scanned` が歩いた塊の数）。

### 測定の揺れの条件は小さな差を守るためのもの。確かな改善の統合を止めない

「前半 3 本と後半 3 本の中央値の差が 10% 以内」の条件は許容が値に比例して縮むので、速くなった版ほど外れる（#291 で 2 日止まった）。実験の `hold` は書き換えず、main へ入れるかは独立レビュー・正式な速さのゲート（QLT-014）・CI・施主の試用で決める（ADR 0075・D41）。
閾値を動かす・合格まで測り直す、のどちらもしない。改善を狙った本文以外で条件が外れたら、正式なゲートの該当ベンチで「悪化なし」を示す。

### ADR を書く前に仕様の施主決定を読み直す

仕様（SPECIFICATION.md の D 番号と FR）にある施主の決定と違う形は、設計席が決めない。書く前に施主へ確かめる（ADR 0058 の末尾・Ctrl+Tab の順で 1 本ぶんの手戻りになった）。

### 窓を最前面に出す確認と計測は、施主に確かめてから回す

`eng/measure-speed.py` と、`eng/verify-window.py` の `<C-v>` や鍵の検査は、テスト用の窓を最前面に出して本物のキー入力を送る。施主が打っている文字がその窓へ入る。実機のクリップボードを書き換える確認も同じく先に確かめる。

### 操作を足すのは `EditorOperation` に 1 値。鍵と一覧の文字は core の表の行

操作の名前・読み・説明は `operation_texts`、鍵とモードは `operation_bindings`（上から見て最初に当たる行が勝つ）。実行は ui の `EditorWindow::run_operation` の 1 本で、鍵で押しても F1 の一覧（Ctrl+P の面の `?`）から選んでも同じ（ADR 0078）。
controller に操作ごとの実行を書かない。一覧で選ぶと controller は `operation_request` を 1 意図だけ立て、ui が `deliver` の終わりで読む（`close_request` と同じ形）。
鍵の表示名（`Ctrl+Shift+S`）は `key_chord_label` が作る。文字列を別に書かない。ui の `press_control_key` に表の操作の `case` を足さない（`operation_chord` に鍵を足す）。
ui は `EditorWindowOperations.cpp` の表（`runner_of`）に 1 行と小さなメンバ関数 1 つを足す。群の関数と何もしない `case` の列は書かない（ADR 0078 の追補 #1188）。
タブとブックマークの鍵は `tab_command_for` と `toggles_bookmark` が同じ表を読む。コピー・貼り付け・Vim の鍵はまだ表の外にあり、一覧に載せるときに表へ移す。

### 速さを測る間は、同じ機械のほかの席とビルドを止める

`eng/measure-speed.py` は基準値との比較なので、機械が混んでいると製品が同じでも起動のベンチが落ちる（#300 で実測: 測定の途中に別の席がビルドを始め、起動の 3 本が約 25% 遅くなった）。
測る前に負荷を数回読み、背景の席を止め、施主にほかの Claude や Codex が動いていないかも聞く。落ちたら合格するまで測り直さず、原因を特定して、前提を満たしてから 1 回だけ測る。無効だった測定も記録に残す（gate-proofs 5-ct）。
測っている間にビルドが始まった回は無効。有効かどうかは結果を見ずに、測定の間のビルドの process の数で決める（道具 `D:/NeNeNib/scripts/20261124/int51/gate2.sh`・2026-11-24 にほかのプロダクトのビルドで 1 回無効になった）。

### 背景の席には worktree の絶対パスで書かせる

席は `D:\NeNeNib\worktrees\<名前>` を自分で切り、ファイルを書く道具とコマンドに絶対パスを渡す。相対パスで本体の checkout に書いた事故があった（#303）。席が権限で止められた操作を、設計席が黙って代わりに行わない。
席が設計の穴で止まって報告するのは正しい動きで、同じ工程の続きは同じ席に答えを送って再開する。

### 背景の席の使い方の決まりは 1 つ: 品質が同じなら、トークンの効率が高い方法を選ぶ

どのモデルに出すか・席を分けるか同じ席で続けるか・報告の長さ・何をどこまで読ませるかは、設計席がその仕事を見て毎回いちばん良い形を選ぶ。固定の割り振りと上限は持たない（施主指示 2026-11-15・全リポジトリ共通・ADR 0038 / 0039 の追補）。品質を下げてまで節約しない。裁定・受理・依頼書と仕様の文言・保護対象の判断は設計席が自分で行う。
判断の材料（決まりではない）: 費用はおおよそ「ターン数 × その時点の文脈の長さ」で増える。`model` を省いた席と `fork` は設計席のモデルで走る（`fork` は全文脈も継ぐ）。判断の無い照合と繰り返しは `eng/` の道具にすればモデルの費用が掛からない。モデルは別名（`opus` `sonnet` `haiku`）で書き、版の番号は書かない。
依頼書は `D:\NeNeNib\briefs\<日付>\`、席の報告は作業木の `out/reports/`、型は `docs/templates/implementation-seat-brief.md`。依頼書には目的・済んだ調べ・範囲・やらないこと・迷いそうな所の答え・止まって報告する条件を書き、手順を細かく指図しない。

### md4c は `third_party/md4c` の別 target。`parse_markdown` は段 2（#765）

md4c（`release-0.5.2`）は上流のバイト列のまま `third_party/md4c` に置き、`nenenib_third_party_target` が C17・`/W4 /WX /utf-8`・Debug のサニタイザだけで作る（ADR 0003 / 0151）。厳格集合を当てると 64 件で落ちる。ソースを 1 バイトも変えない。
`third_party/` の下のファイルは全部 `eng/conformance-rules.json` の `thirdPartyDigests` に LF の SHA-256 があり、CNF-013 が git の index の blob と照合する。core が結ぶが、`md4c.h` を読むのは段 2 の `src/core/MarkdownParse.cpp` の 1 か所だけ。


### Vim の鍵が undo の単位を作ったか・本文を変えたと数えるかは engine の印 `VimStep.change`

印は `std::optional<VimChangeMark>`（`VimChangeMark{kind, at}`・`kind` は `VimChange::changed` か `saved_only`・`at` は Vim が `changed_*` に渡した位置で changed のときだけ）。立てるのは `vim_step` の出口の `change_marked` の 1 か所（ADR 0181 と #1374 の追補）。
controller は編集の中身・`edits.front()`・改訂番号から「Vim が変えたか」を推し量らない。印があって本文に触れない鍵は `push_empty_unit` が履歴に空の単位を積み、窓の先頭を決め直すかは印の位置で `vim_top_still_valid` が決める。
未保存の判定は `EditHistory::content_position()`（本文を変えた単位だけを数える位置）。通常モードの Ctrl+Z / Ctrl+Y は `past_saved_only_units` で saved_only の空の単位だけを飛ばし、changed の空の単位は 1 回の取り消しとして消費する（● が戻る・#1535）。

### piece の列は累積の長さと改行の数を持ち、読む口は二分探索

各 piece は手前までの `prefix_bytes` と `prefix_newlines` を持ち、位置 → piece は `TextBuffer::piece_at`、行 → piece は `piece_of_newline` の二分探索で引く（ADR 0183・#1474）。0 番目から線形に歩かない。
累積を埋めるのは `TextBuffer.cpp` の `placed` の 1 か所（構築・collect・挿入はどれもここを通る）。最後に読んだ位置などの覚えは持たない。塊の列（段 2）は段 1 の実測の後。

### 打った文字を言語サーバーへ渡す入口は `typed_for_language` の 1 本

通常モードの `InsertText` も Vim の INSERT の 1 文字も、`EditorController::typed_for_language(打った文字)` を通って自動の補完（`open_triggered_completion`）へ行く（#1962・ADR 0190 の 2026-11-23 の追補）。Vim の側で「使う人が打った鍵か」を決めるのは `deliver_vim_key` の `watched_vim_key`（`typed_watch_` が立つ間だけ）と `typed_vim_character` で、再生・`:normal!`・IME の確定は通らない。engine は言語サーバーを知らないまま。
引数の情報（段 S6 c）など打鍵で頼む物を足すときは、この入口に並べる。別の入口を作らない。`initialize` で名乗る能力は `text_document_capabilities` の 1 か所で、実装した物だけを名乗る（#1961）。

### 言語サーバーの root の「同じフォルダか・下にあるか」は core の `FileFolder` の 1 本

比べは `core::same_folder` と `core::folder_contains`（ASCII の大小・`\` `/`・末尾の区切りを畳む。畳み方は `folder_spelling` の 1 本）で、上る上限 `lsp_root_folders` も同じ畳み方（#1963 のレビュー M1）。application で文字列を比べ直さない。
root は印（`lsp_root_markers`）を文書のフォルダから上へ探し、設定の root の下の文書は設定の root より上へ登らない（`LanguageRootRequest.ceiling`・#1960）。

### まとめ枝に積むとき

道具は `D:/NeNeNib/scripts/20261112/` の `stack.sh`（起点からの差分を `git apply --3way`）・`finish-stack.sh`（生成物と `fixtures.json` の衝突を oracle で作り直す・`join-fixtures.py`）・`union-json-list.py`（折り返す窓の fixture を名前で併合）。
2 本の PR が表に 1 つずつ足すと git が畳んで宣言の大きさと中身がずれる。積むたびに `tests/unit/NibTests.cpp` の `contracts` と `scopes`、`tests/ui/NibWindowTests.cpp` の `groups` を数え直す（数は現物を数える。ここに写さない）。
oracle の測定コードを変えた PR は、枝の commit を `--reuse-ref` にして main 側にだけある fixture を測る。製品のコードが衝突したら、設計席が手で解かず、席にまとめ枝を自分の枝へ merge させてからその差分を積む（#1551・入力行の鎖）。
まとめ枝の上に設計席が足す直しの commit も、本文に `Closes #<まとめ枝の Issue>` を書く（`Refs` だと `merge-pr.py --integration` が断る・#2015）。
merge の前に main が動いていないか確かめる。動いていたら、まとめ枝を組む前に main へ入れる物を済ませるか、`origin/main` から組み直す（#2020）。
PR 本文に `Closes #N` の字を 2 回書かない（説明の文の中でも GIT-001 で断られる・#2020）。
---

## 3. 検証コマンド

差分の挙動・直接の依存先と呼び出し元から、起こり得る退行を検出する最小限の検証を選ぶ。
何が壊れる可能性を確認するか説明できない検証は実行しない。既存の CMake target・CTest `-R`・unittest の対象指定を使う。
文書・コメント・規約変更にはアプリの動作テストは不要。フック・開発ツールは変更した道具だけを短く確認する。

実装・テスト・関連依存・必要な環境条件が不変なら成功結果を push / レビュー / merge で再利用する。
担当・工程・文書追記・SHA の変更だけでは再実行しない。関連する変更・失敗・具体的な未確認事項だけを再検証する。
全件は限定した検証では覆えない具体的な理由がある場合だけ、対象と理由を短く知らせて明示実行する。

```powershell
pwsh -NoProfile -File ./eng/check.ps1 -Full -Reason '限定した検証では影響を確認できない具体的な理由'
```

実行していない結果を書かない。本件が原因の失敗は直し、無関係な既存失敗は根拠とともに別 Issue へ記録して本件を続ける。
成功するまでの再試行や無関係な修正・全件再実行は禁止。PR には対象・退行の根拠・結果と所在・再利用の根拠を記録する。
正本は QLT-001 / QLT-012 と [ADR 0021](docs/adr/0021-diff-scoped-verification-and-result-reuse.md)。過去の全件・最終 HEAD ごとの実行指示より優先する。

---

## 4. 変更の進め方

[docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) が正本。要約すると:

Issue → 正典経路の特定 → ブランチ → （設計を変えるなら先に ADR）→ 最小の実装 →
必要な検証（成功結果を再利用）→ 規則 ID ごとの自己レビュー → PR（draft）→ 検証記録を整えて Ready → マージ直前のCI（文書のみなら不要）→ squash merge。レビュー済みの PR が並ぶときはまとめ枝（1 PR = 1 commit・手元の検証と CI は 1 回・`merge-pr.py --integration` で rebase merge・ADR 0021 追補・#1220）。

コミットは Conventional Commits（`type` と `scope` は英語、説明は日本語、末尾に `(#N)`）。形の正本は GIT-003。

---

## 5. 完了報告の形

作業を終えたら必ず次を報告する。

```text
Issue / 規則 ID:
変更したファイルと振る舞い:
実行した検証コマンドと結果:
ドキュメント・スキーマの変更:
Waivers: none | WVR-NNNN
残るリスク:
```

調査だけを頼まれたときは、編集・コミット・push・PR 作成・外部状態の変更を行わない。

---

## 6. いまの状況

現在のタスクは [docs/todo/current.md](docs/todo/current.md)。GitHub Issue が正で、そこは要約。

**この節には、いまの状態と所在だけを書く。日ごとの経緯・入った PR・数字の積み重ねは書かない**（日報と引き継ぎと `current.md` に任せる。2026-11-15 までこの節に積もった経緯 44 KB は [docs/reports/claude-md-status-log-2026-09-15-to-2026-11-15.md](docs/reports/claude-md-status-log-2026-09-15-to-2026-11-15.md) に移した・#1716）。状態が変わったら下の箇条を書き換える（足さない）。

- **版と公開**（2026-11-24）: **公開版は v1.1.0**（[GitHub の Release](https://github.com/hideyukiMORI/nene-nib/releases/tag/v1.1.0)・2026-11-23 12:43 JST と [Microsoft Store](https://apps.microsoft.com/detail/9nclgdtfq529)）。1.1.0 の公開元は main `4fcf258c`、記録は [docs/quality/release-1.1.0-2026-11-17.md](docs/quality/release-1.1.0-2026-11-17.md)、手順は `D:/NeNeNib/evidence/v1.1.0-release/README.md`。前の版 v1.0.0 の公開元は main `80fd7a53`、記録は [docs/quality/release-1.0.0-2026-11-14.md](docs/quality/release-1.0.0-2026-11-14.md)。**次の版は 1.2.0**（hide の決め 2026-11-24・SPECIFICATION の D115）で、公開の準備中。1.2.0 の中身と手順の所在は `docs/quality/release-1.2.0-*.md`（別の PR で入る）と `D:/NeNeNib/evidence/v1.2.0-release/`（手順書 `README.md`・決めの一覧 `open-questions.md`）。
- **1.2.0 の公開まで main に機能を入れない**（公開が次の順なら公開の準備が先・2026-11-24）。1.2.0 に入れる残りは `current.md` の「1.2.0 に入れる残り」だけ。クイックフィックス（#2005）は次の版で、1.2.0 の公開元を切るまで main に入れない（D116）。
- **`settings.v4`（1.0.0）と `settings.v5`（1.1.0）に鍵を足さない**。main は `settings.v6` を書き、v6 を書く最初の公開版は次の版 1.2.0（hide の決め 2026-11-24）。1.2.0 の公開までは v6 のまま・公開したら v6 にも鍵を足さない（D91・§2 の設定の節）。
- **再開するとき**: 最新の[引き継ぎ](docs/handoffs/)の「次回の最初の手順」→ `git fetch` と `gh pr list` で照合 → [日報](docs/reports/)。施主決定は [SPECIFICATION.md](SPECIFICATION.md) の D 番号、できることの一覧は [README.md](README.md)。
- **数字**（fixture の件数・基準値・ベンチの本数）はここに写さない。fixture は `tests/vim/fixtures.json`、速さは `eng/perf-reference.json` と `eng/measure-speed.py` の `BENCHES`、表の大きさは現物を数える。
- 実機の目視は Claude デスクトップの別セッション（computer-use）か、hide の了承の後に設計席が回す道具（§2 の「窓を最前面に出す確認」）。

---

## 7. 上位のポリシーと雛形の所在

このリポジトリの規約は **AYANE 厳格規約ポリシー**の 3 回目の適用例である（1 回目 nene-loupe・C++23 / MSVC、2 回目 nene-folio・C23 / clang-cl）。
ポリシー本文・初期化手順・雛形は施主の private ワークスペースにあり、このリポジトリには置かない。
雛形で足りなかったものを見つけたら、このリポを直すと同時に施主へ還流点として報告する。
