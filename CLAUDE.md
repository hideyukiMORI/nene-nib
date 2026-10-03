# CLAUDE.md — NeNe Nib

Claude Code / AI エージェントがこのリポジトリで作業するための**中核ハンドブック**。
簡潔な英語版の入口は [AGENTS.md](AGENTS.md)。詳細の正本は `docs/` にあり、ここには複製しない。

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

スレッドは `src/adapters/win32` の `Win32Worker` の 1 本だけで、最初の仕事を受けたときに起きる（合成では起こさない・ADR 0004 / 0062）。仕事を足すときは仕事の種類ごとの port を application に宣言する（最初は `FolderPort` の `list` と `collect`）。
完了の知らせは中身の無い窓メッセージ（番号は `EditorWindow.cpp` の `work_message` の 1 か所）で、ui は意図 `WorkCompleted` を送り、中身は application が `collect()` で引く。`send` の途中で届いた合図は、いちばん外の `send` の終わりで 1 回だけ送る。
`WorkCompleted` は Ctrl+Tab の歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない（入口の `keeps_tab_walk` と `begin_intent`・契約 `--background-work`）。新しい意図を足すときは、この 2 か所にどちらの側かを決めて書く。
adapters の試験は時計を読まない。待つのは合図の受け皿（セマフォ）で、止まらなければ CTest の `TIMEOUT` で落ちる。

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

### Mica には `WS_EX_NOREDIRECTIONBITMAP` と `DWMWA_USE_IMMERSIVE_DARK_MODE` が要る

無いとタイトルバーに透けない・ダークでも明るいままになる（Issue #5 で実測。ADR 0008 決定 9）。

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

### `u` の後のキャレットは `Edit.restore`。`edit.at` から決めない

戻り先を書くのは `EditorController::replace` の 1 か所（そのときのキャレット）。オペレータと VISUAL は engine が `VimStep.restore` に「Vim が最初の変更を保存する瞬間のカーソル」を添え、controller が効果を写す前にキャレットをそこへ置く（ADR 0052）。
`Edit` を作る所は `restore` を必ず書く（既定値なし）。oracle は鍵を 1 本の `:normal!` に流すので、`u` を含む fixture は変更が 1 つだけの鍵列で書く。

### Vim の 1 文字は `vim_character_end` / `vim_character_start`。`next_code_point` で歩かない

Vim の 1 文字はコードポイント 1 つと直後に続く幅 0（`DisplayWidth::zero`）のコードポイントの列（ADR 0053）。判定の表は仮想桁の表の 1 つだけで、結合文字のための表を作らない。
`next_code_point` / `previous_code_point` はバイト列の走査（表示・UTF-16 の変換・照合器）のためのもの。1 打鍵ごとに呼ばれる所は行の全体を `text_range` で写さない（キャレットの周りの窓だけを読む）。

### fixture の記法（`<Esc>` `<NL>` など）を足すのは `eng/vim-oracle.py` の `KEY_TABLE` の 1 行だけ

C++ の表 `tests/vim/VimKeyNames.hpp` は生成物で、手で書かない（`python eng/vim-oracle.py --key-names`・ADR 0054・CNF-010）。
表は oracle の測定の領域にあるので、表を変えた commit を `--reuse-ref` にしてから fixture を部分再生成する。

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
一覧 `session.v1` を書くのは窓を閉じるときの意図 `EndSession` だけ。道具は `eng/window_driver.py` の `start` が起動の前に自分の profile の一覧を消す（復元の検査だけ `keep_session=True`）。

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

### 本文の文字組みは `layout_of`。描画とクリックで共用する

通常行・IMEの差し込み済み行・クリックは同じ入口で、全文と幅・行高が一致する現在/直前のlayoutを再利用する。本文の書式を作り直したら全て破棄する（ADR 0069）。選択や検索の描画で共有layoutの書式を変更しない。整列・高さを変える右寄せやコマンド入力のlayoutは、この保持へ入れない。

### リポジトリの外の作業場所は `D:\NeNeNib\`

依頼書は `D:\NeNeNib\briefs\`、1 回限りのスクリプトは `D:\NeNeNib\scripts\`、実機の確認の profile と文書は `D:\NeNeNib\evidence\`（施主指示 2026-09-30。C ドライブの Temp と scratchpad を使わない）。道具の出力と席の報告はリポジトリの `out/`。
D は HDD で、そこに置いた profile では保存の書き込みが遅い（履歴の 1 回が 30〜40 ms・最初は数百 ms）。撮影のスクリプトは、書き込みを伴う操作の後に 1.5 秒待つ。

### ADR を書く前に仕様の施主決定を読み直す

仕様（SPECIFICATION.md の D 番号と FR）にある施主の決定と違う形は、設計席が決めない。書く前に施主へ確かめる（ADR 0058 の末尾・Ctrl+Tab の順で 1 本ぶんの手戻りになった）。

### 窓を最前面に出す確認と計測は、施主に確かめてから回す

`eng/measure-speed.py` と、`eng/verify-window.py` の `<C-v>` や鍵の検査は、テスト用の窓を最前面に出して本物のキー入力を送る。施主が打っている文字がその窓へ入る。実機のクリップボードを書き換える確認も同じく先に確かめる。

### md4c はまだ入っていない

Markdown プレビュー（FR-007）の Issue で、別 target に `/W4 /WX` だけを当てて tag と SHA-256 で固定して入れる（ADR 0003）。厳格集合を当てると 20 件超で落ちる。

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
必要な検証（成功結果を再利用）→ 規則 ID ごとの自己レビュー → PR（draft）→ 検証記録を整えて Ready → 必須 check → squash merge。

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

2026-09-15: Issue #1（Phase 0〜2）、#3（最初の縦切り・ADR 0007）、#5（見た目・ADR 0008）、#7（編集・ADR 0009）、#11（ファイル・ADR 0010）。2026-09-16: #13（UTF-16 の変換を core に 1 本化）、#16（速さ・ADR 0011）、#19（起動の内訳）、#22（Vim の最初の縦切り・ADR 0012）。2026-09-17: #24（窓を先に見せる起動・ADR 0013）、#28（IME・ADR 0014）。2026-09-18: #43（Vim の 2 本目・ADR 0015）、#44（生成物の SHA・CNF-010）、#47（CI の速さの基準値・ADR 0016）。2026-09-19: #52（カラーテーマ C1・ADR 0017）、#53（VISUAL・ADR 0018）。2026-09-20〜21: #58 画面移動（ADR 0019）、#60〜#70 設定と C2〜C4b（ADR 0020〜0025）、#72 文字検索（ADR 0026）、#76 gg/G（ADR 0027）、#79 o/O（ADR 0028）、#77 / #81、#84 r（ADR 0029）。2026-09-22: #87 `.`（ADR 0030）、#88 検証方針の文言、#93 テキストオブジェクト（ADR 0031）、#94 CI の検査文言の UTF-8、#100 検索（ADR 0032）、#98 fixtures.json の整形（CNF-011）、#91 VISUAL の `.`（ADR 0033）、#92 割り込みの 1 本化、#99 テキストオブジェクトの残差、#108 仮想桁（ADR 0034）、#111 引用符の対、#112 矩形 VISUAL（ADR 0035）、#106 cp932、#85 literal CR（ADR 0036）、#123 検索の強調（ADR 0037・施主決定 D17）。2026-09-23: #128 / #136 席ごとのモデルと実装席の形（ADR 0038 / 0039・施主指示 手 1〜3）、#129 `eng/build-release.ps1`、#130 `eng/protected-diff.py`（保護対象の差分 0 確認と scope ごとの checks 数の前後比較）、#131 `eng/verify-window.py --capture --keys` と `eng/compare-frames.py`（PNG を設計席が Read で見る）、#124 README と current.md の要約化、#146 `eng/usage-report.py`（席ごとの usage）、#117 制御文字の `^M` `<200b>` の描画（ADR 0040・core の `display_line` と桁の対応表）、#151 / #147（ブロックキャレットの幅・C1 の 4 桁）、#148 incsearch（ADR 0041）、#160 単体テストを scope ごとの翻訳単位に（ADR 0042・`tests/unit/` は 1 scope = 1 ファイル・表と `main` は `NibTests.cpp`）、#140 verify-window の覆いの検査と `--keys` の節目、#165、#168 incsearch の Ctrl-G / Ctrl-T（ADR 0043・preview の起点を当たりへ動かし確定の鍵が起点を運ぶ・向きは本文の順で Vim のソースと一致・D18）、#175 Tab は空白 8 個ぶんの tab stop（ADR 0045）、#174 add バッファは 64 KiB の chunk の列（ADR 0044）、#176 マクロ `q` `@`（ADR 0046・打った鍵を engine が録り再生は `.` と同じ経路・oracle は録画を観測できないので fixture は `register` 欄で再生だけ）、#180 `recording @a`、#179 / #184 16 MiB の 200 打鍵のベンチと改行の索引の共有（ADR 0047・462 ms → 7 ms）、#190 oracle の `q` の拒否を状態機械で狭める、#191 `erase().insert()` を `replaced` 1 回に、#193 名前つきレジスタ `"a`（ADR 0048・マクロと同じ 26 本の本文の表・鍵列 ↔ 本文の写しは core の 1 対・再生は窓の鍵と同じ口）、#198 鍵の表を `VimKeyTable.hpp/.cpp` へ（ADR 0042 決定 6・表を触るときはここ）、#200 `<Space>` `<BS>` は行をまたぐ `l` `h`（ADR 0049・オペレータ待ちと VISUAL では行末の位置に一度止まる・fixture の記法に `<Space>` と矢印）。2026-09-29: #204 数字レジスタ `"0`〜`"9` と小削除 `"-`（ADR 0050・書き手は `registers_written` の 1 本で「名指しの書き込み → `"1` の規則 → `"-` の規則 → 無名」・`"1` へ行くかはレジスタの値から決め検索の移動だけ印を運ぶ・読みは `register_read` の 1 本で `p` `P` と `@` が共用・`.` は記録の先頭の `"{1〜8}` を 1 つ進める）、#210 クリップボードのレジスタ `"+` `"*`（ADR 0051・engine は OS に触れず controller が鍵を流す直前に `ClipboardPort` から写しを置き engine の書きは `VimStep.clipboard` に添える・fixture は不能で契約 `--vim-clipboard`）、#209 `.` の記録は次の 1 鍵を待つ状態の数字を引数として残す、#205 改行を含む文字単位の `p` `P` はキャレットを貼った本文の先頭に置く、#206 NORMAL の `X`（`dh` と同じ範囲を `x` と同じ道で・行頭では失敗にしない）、#208 `u` と Ctrl-r の後のキャレット（ADR 0052・`Edit.restore` に編集を始める瞬間のキャレットを覚え `u` も Ctrl-r もそこへ行ってその時点の本文で寄せる・オペレータと VISUAL の戻り先は engine が `VimStep.restore` に添える・通常モードの Ctrl+Z は戻した本文の末尾のまま）、#216 Vim の 1 文字と結合文字（ADR 0053・1 文字はコードポイント 1 つと直後に続く幅 0 の列・判定は仮想桁の表の 1 つ・歩く関数は `vim_character_end` / `vim_character_start` の 1 対・ZWJ の連なりと国旗は Vim と同じく別々の文字）、#222 `W` `E` `B` `ge` `gE`（`w e b` と同じ歩き方に語の分類を渡す）、#224 回数の途中で本文の先頭に当たった `b B ge gE` は着地まで動いて失敗しオペレータを打ち消す、#226 前向きの語の移動は Vim の `fwd_word` と `end_word` が失敗する所で失敗の印を返す（`w W` は回数の周の始めだけ・`e E` は周のどの歩でも）、#229 fixture の鍵の記法の表は `eng/vim-oracle.py` の `KEY_TABLE` の 1 つで C++ の表 `tests/vim/VimKeyNames.hpp` は生成物（ADR 0054・CNF-010 が全文の一致を守る）、#235 貼り付けた本文の改行は文書の改行の形に揃える（ADR 0055・施主決定 D19・畳む関数は core の `clipboard_line_feeds` の 1 本）、#230 fixture の再生は `:normal!` と同じく失敗した鍵の後ろを打ち切る（`press_vim_keys` と `vim_normal`・手書きの契約は打った鍵の意味の `vim_replay`）、#237 複数タブの状態（ADR 0056・施主決定 D20〜D22・アクティブな文書は `EditorState` が今の欄のまま持ちほかのタブは不変の束 `DocumentState` を脇に置く）、#238 タブの帯とマウス（帯の配置は core の `title_bar_layout(TitleBarInput)` の 1 本で描画と hit test が同じ入力・押した要素を覚えて離した要素と同じときだけ動かす `title_bar_released`・窓の最小は 360 × 200 DIP）、#239 タブの鍵（`core::TabKey` → `tab_command_for(TabKey, EditMode)`・`eng/verify-window.py --tabs`）。2026-09-30: #240 タブの一覧と Ex の `tabnext` と Vim の `gt` `gT`（ADR 0057・行き先は `tab_destination` の 1 本・engine は `VimEditorView.tabs` を借用して失敗を決める・一覧は `CommandPalette` の 2 つ目の出どころ・`:tabclose` は Nib の閉じる流れを通す）、#248 Ctrl+Tab は最近使った順（ADR 0058・施主決定 D23・Ctrl を押している間は順を固定して歩き離したときに確定・使った順は application の `TabRecency`）、#252 / #253 前回のタブの復元（ADR 0059・施主決定 D24〜D27・窓を閉じるときに `session.v1` に覚え・ファイルを指定しない起動で「まだ読んでいない文書」として帯に並べ・読むのは `reach_tab` の 1 本で見るときに読む・`eng/verify-window.py --restore`）、#258 / #259 Ctrl+P の統合の 1 本目と 2 本目（ADR 0060・施主決定 D28〜D30・面の候補は 1 つの列で行頭の記号 `#` `@` `:` が出どころを絞る・記号の表は core の `palette_marks`・閉じたファイルの履歴は `history.v1` に閉じたときだけ書く）。2026-10-02: #264 Ctrl+P の面の日本語入力（ADR 0061・施主決定 D31・D32・IME の構えは application の `ImeStance` の 1 つで面はオフで開いて使う人に任せる・照合はコードポイントの境目）、#270 面は絞り込みの結果を入力と列が変わったときだけ作り frame には見えている行だけ（ADR 0062・`CommandPalette` の読む口は `count` `choice_at` `rows`・行数の上限は `palette_row_limit`）、#271 裏のワーカー 1 本とフォルダの列挙（ADR 0062・施主決定 D33・D34・スレッドは `src/adapters/win32` の `Win32Worker` だけで最初の仕事で起きる・application は型のある `FolderPort` の `list` と `collect`・完了は中身の無い窓メッセージの合図から意図 `WorkCompleted`）。起動すると枠なし窓（Snap と影は OS のまま）に Mica のタイトルバー、帯に並ぶタブ（クリック・× と中ボタン・「＋」・hover・あふれたら「∨」の一覧とホイール）と窓の操作、
piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）、ステータスバーの「通常 | Vim」トグルを Direct2D で描き、OS のライト／ダーク（茄子色 D11・橙 D12）に従う。
Ctrl+O / Ctrl+S / Ctrl+Shift+S と起動引数でファイルを開いて保存し、UTF-8 / UTF-8 BOM / Shift_JIS と CRLF / LF を読んだ形のまま保ち、未保存の印「● 」と「保存しますか」を出す。
速さは `eng/measure-speed.py` が Release の exe で 5 本のベンチを測り、`eng/perf-reference.json` の機械ごとの基準値と比べてゲートで落とす（QLT-014・測るのは差分が速さに関わるときと `-Full` の明示実行で、CI の必須 check は測らない・施主の実機と CI の EPYC 7763 の指紋で active・他の CI host は記録だけと 1 行言って通る・ADR 0016。実機: 起動 191 ms・窓が見えるまで 35 ms・1 打鍵 0.9 ms（空の文書）・16 MiB を開く 250 ms・16 MiB で 200 打鍵 7.4 ms）。描画は `WM_PAINT` で 1 フレームに 1 回。
見た目の正本は `docs/design/2026-09-15-look.md` と `docs/design/2026-09-15-editing-look.md`。カラーテーマとフォントサイズの計画は `docs/plans/2026-09-15-colorschemes.md`（D13 / D14）。
Vim は NORMAL / INSERT（`h j k l 0 $ w b e W B E ge gE ^ gg G`（1 文字は結合文字と異体字セレクタを前の文字に含める・ADR 0053）・`f/F/t/T`と`;`/`,`・Home / End・回数はオペレータ側と移動側の掛け算・`x` `X`・`r`（回数/選択範囲の文字置換）・`d c y` ＋移動・`dd cc yy`・`D C Y`・`p P`・種類つきの無名レジスタ（本文は LF）・`i a I A`・`o O`（回数付き入力・`i a I A` の回数も反復）・`.`（直前の変更を鍵の列として記録し同じ経路へ再生・`N.` は回数を置き換えて以後も引き継ぐ・VISUAL の変更は範囲の大きさを記録して選び直してから再生し回数は使わない・ADR 0033）・テキストオブジェクト `iw aw iW aW i" a" i' a' i` a` i( a( i{ a{ i[ a[ i< a<`（`b` `B` と閉じ括弧の鍵も同じ表・d/c/y と VISUAL で同じ範囲関数 1 本・VISUAL は常に文字単位で `viwiw` `vi(i(` は 1 つぶん広げる・取消でも Vim と同じにキャレットと選択が動く・`it` `ip` `is` は後続）・検索 `/ ? n N * #`（Ex と同じ入力行から入り確定は 1 つの鍵・`magic` の部分集合の照合器で未対応構文は閉じた失敗・E486 / E35 / E348 と折り返しの報せ・ADR 0032・見えている行の全一致を `search` の面で塗りキャレットを含む一致は `accent` の枠・`hlsearch` は既定オンで `:noh` `:set (no)hlsearch`・ADR 0037・`incsearch` は入力中の当たりを engine の外の preview として見せ Esc で入力前の画面へ戻り `hlsearch` off なら今の当たりの枠だけ・既定オン（施主決定 D18）・`:set (no)incsearch`・ADR 0041・Ctrl-G / Ctrl-T で次・前の当たりへ移り Enter でそこへ着く・ADR 0043）・`j k H M L` の欲しい列と VISUAL の `.` の桁は仮想桁（Tab は 8・全角は 2・表は固定 Vim の実測値・ADR 0034）・Esc・`u` / Ctrl-r（キャレットは Vim と同じく undo の単位が覚えた戻り先へ・ADR 0052）・INSERT 1 回が undo 1 単位・VISUAL `v V` と矩形 `Ctrl-v`（仮想桁の矩形・`o O` `$`・`d x y r`・幅つきの矩形レジスタの `p P`・`.`・Vim の NORMAL / VISUAL では Ctrl+V は矩形の鍵で OS 貼付は通常モードと INSERT だけ・ADR 0035）・`H M L`・Ctrl-d/u/f/b・PgUp/PgDn）で、再現度は本物の Vim 9.1 の oracle が生成した fixture 1853 件（`tests/vim/`・`eng/vim-oracle.py --regenerate` が正準形で書き戻す・生成物と json の一致は CNF-010・json の整形は CNF-011）を CTest が再生して守る。IME は IMM32 を ui/win32 が受け、変換中の文字列は本文の外（`core::Composition`）に持って renderer がキャレットの行に差し込んで描き、確定は 1 意図（通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ）。Vim NORMAL では IME を切り INSERT で戻す（ADR 0014）。coreに組み込み9テーマ（ADR 0017）があり、版付き設定の保存/復元（C2・ADR 0020）、NORMALの `:` から `colorscheme` / `set fontsize=` / `set guifont=` の変更と補完（C3a・ADR 0022）を接続済み。通常/Vim全モードからCtrl+Pの面（C3b・ADR 0023）も利用でき、候補・入力session・評価・保存はExと共用する。Ctrl+P は入力が空で開いて、開いているタブとブックマーク（1024 件）と閉じたファイルの履歴（100 件）と同じフォルダのファイルを名前と場所のファジー検索で出し、行頭の `#` はタブだけ・`*` はブックマークだけ・`@` は履歴だけ・`/` は同じフォルダだけ・`:` は設定のコマンド（履歴から選んだファイルが無ければ 1 行知らせて履歴から外す・ADR 0060）。改行の形（LF / CRLF）は `TextBuffer` が 1 つ持ち、LF 文書の `\r` は文字として残る（VISUAL の `r<CR>` は literal CR・ADR 0036・制御文字は `^M` `<200b>` `<85>` の形で描く・ADR 0040）。Vim のマクロは `q{a-z}` `q{A-Z}` `q` `@{a-z}` `@@` `[count]@a`（undo 1 単位・失敗で打ち切り・入れ子の上限 100・録画中はステータスバーに `recording @a`・`"ap` はマクロの鍵列を文字として貼る）。名前つきレジスタは `"{a-z}` `"{A-Z}`（追記）`""` `"_` の接頭辞で `x d c y p P` と VISUAL / 矩形の `d x y` に効き（名前つきへ書くと無名も同じ・`"add` の後の `.` は同じ名前・`"Add` は追記を繰り返す）、`"ayy` の後の `@a` は本文を鍵として実行する（末尾の改行は `<NL>`・NORMAL / VISUAL の `<NL>` は `j`、`<CR>` `+` `-` は隣の行の最初の非空白・ADR 0048。矩形への追記は後続）。数字レジスタ `"0`〜`"9` と小削除 `"-` は yank が `"0`、行単位か複数行の削除・変更と検索の移動の削除が `"1`（`"9` まで繰り下がる）、1 行の中の名前なしの削除が `"-` で、`""` の明示は `"0` の名指しと同じ・`"1p` の後の `.` は `"2p` `"3p` と進み・`@0`〜`@9` `@-` `q0`〜`q9` が効く（ADR 0050）。クリップボード `"+` `"*` は `"+yy` `"+dd` `"+p` `"+P` `@+` が OS のクリップボードを読み書きし、`.` とマクロの中でも再生の時点の中身を読み、読んだ本文は CRLF を LF に畳んで末尾が改行なら行単位、出す本文は Ctrl+C と同じ文書の改行の形（ADR 0051。矩形の種類の往復・読み取り専用のレジスタ `". ": "/ "%`・`q"` は後続）。タブの鍵は Ctrl+T・Ctrl+Tab / Ctrl+Shift+Tab（最近使った順）・Ctrl+F4・通常モードの Ctrl+W、Vim は `gt` `gT` `{N}gt` `{N}gT` と `:tabnext` `:tabprevious` `:tabnew` `:tabclose` `:tabs`（帯の位置の順・`{N}gt` は絶対で範囲の外は失敗・`{N}gT` は相対で折り返す・Vim 9.1 の実測）。ファイルを指定せずに起動すると前回のタブ（保存済みのファイルだけ）が並びと見ていたタブとカーソルと画面の位置ごと戻り、起動で読むのは見ていたタブだけで、ほかはそのタブを見るときに読み、無くなったファイルはタブを外してステータスバーに「開けませんでした: <名前>」と 1 行知らせる（ファイルを指定した起動は指定だけを開く・ADR 0059）。矩形の `I A c C > <`・タブの並べ替え・窓の位置と大きさの復元・タスクバーのジャンプリスト・一般Exは後続。ブックマークは通常の Ctrl+D / Vim の Ctrl+Shift+D で本文または一覧の選択ファイルを付け外しし、`bookmarks.v1` へ明示操作のときだけ保存する。Vim の Ctrl+D は半画面移動のまま、消えた登録は知らせて残す（ADR 0063・D36・D37）。Ctrl+Pの面は日本語入力に対応し（ADR 0061）、同じフォルダは裏で読み選択を保って候補を足す（ADR 0062）。起動は窓が約 35 ms で見え、本文は `D3D11CreateDevice`（NVIDIA のドライバ初期化 160 ms・呼び方では縮まない）の後に約 200 ms で出る（ADR 0013）。実機の目視は Claude デスクトップの別セッション（computer-use）で行う。最新は [日報](docs/reports/2026-10-04.md) / [引き継ぎ](docs/handoffs/2026-10-04.md)。

---

## 7. 上位のポリシーと雛形の所在

このリポジトリの規約は **AYANE 厳格規約ポリシー**の 3 回目の適用例である（1 回目 nene-loupe・C++23 / MSVC、2 回目 nene-folio・C23 / clang-cl）。
ポリシー本文・初期化手順・雛形は施主の private ワークスペースにあり、このリポジトリには置かない。
雛形で足りなかったものを見つけたら、このリポを直すと同時に施主へ還流点として報告する。
