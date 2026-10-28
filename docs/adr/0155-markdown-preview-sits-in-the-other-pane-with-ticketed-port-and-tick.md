# ADR 0155 — Markdown プレビューは split の other の枠に置き、更新は券つきの `MarkdownPort` と 200 ms の tick の間引きで行う

- 状態: 提案（段 5 の ui と hide の試用の後に受理）
- 日付: 2026-10-22
- Issue: #800（段 4・5 の application と core の側・この ADR）・#801（adapters と ui・次の工程）・親 #763
- 規則: FR-007 / D7、ARC-001 / ARC-003 / ARC-007 / ARC-010 / ARC-011 / CPP-002 / CPP-003 / CPP-004 / CPP-005 / CPP-011、ADR 0004（ワーカー 1 本と仕事ごとの port）/ 0062（券と `collect`・決定 7・15）/ 0078（操作の表）/ 0137（tick の合図・改訂番号で比べる）/ 0148（split の other）/ 0151（`parse_markdown`）
- 計画: `docs/plans/2026-10-21-markdown-preview.md` §3.4。調査: `D:/NeNeNib/outputs/20261022/survey-preview-stage45.md`

## 文脈

段 1〜3（ADR 0151・#764 #765 #784）で、md4c を `third_party` に置き、core の `parse_markdown` が leaf ブロックの平らな列 `MarkdownDocument` を返し、ui に `PreviewTextLayouts` / `PreviewPainter` がある。製品はまだ `parse_markdown` を呼ばず、プレビューを出す場所も更新の道も無い。split（ADR 0148）は最大 2 枠で、もう 1 つの枠は受け身の `PaneView` だけを置ける。

段 4・5 は「どこに出すか」と「いつ・どう解析を頼むか」を決める。解析は 16 MiB で数十 ms（`docs/quality/md4c-probe-2026-10-21.md` の 63.8 ms）なので UI スレッドで打鍵ごとに回せない。スレッドは adapters のワーカー 1 本だけ（ADR 0004）。

## 決定

1. **置き場は other の枠。** `SplitState.other` は `std::optional<OtherView>`、`OtherView = std::variant<PaneView, PreviewView>`（`OtherView.hpp` は `using` 別名の 1 ファイル・CPP-011）。`.other` を読む所は全部 `std::visit`（`default` 無し・選択肢が増えたらコンパイルが落ちる・CPP-002）で、`EditorState` は読む口 `other_pane()` と `preview()`（どちらも visit で選び、違う選択肢なら `nullptr`）を持つ。`EditorFrame.other` は `std::optional<OtherFrame>`、`OtherFrame = std::variant<PaneFrame, PreviewFrame>`。
2. **`PreviewView` は文書の値ではなく窓の値。** `PreviewView{ document: shared_ptr<const core::MarkdownDocument>（見せている結果・最初は空）, ticket（最後に出した券・0 は未要求）, requested_revision（最後に要求した本文の改訂番号）, seen_revision（前の tick で見た改訂番号）, anchor: core::Offset（錨）, follows（本文に追従しているか）, notice: optional<DisplayText>（16 MiB 超・失敗の 1 行） }`（公開 aggregate・比較は非メンバーで `document` は指す先の同一性）。改訂番号の型は `EditHistory::revision()` と同じ `std::size_t`。プレビューは操作中の文書に追従し、文書が替わると `document` と `notice` を捨てて即時に頼み直す（決定 9）。
3. **開く / 閉じるは操作 1 つ。** `core::EditorOperation::toggle_preview`（enum の末尾・文 `Markdown プレビュー`・読み `まーくだうんぷれびゅー`・説明 `Markdown のプレビューを右（下）の枠に出す・もう一度で閉じる`・鍵 `KeyChord{true, true, OperationKey::m}`（Ctrl+Shift+M）で `OperationModes::both`・`OperationKey` に `m`・表示名 `M`）。意図 `TogglePreview`（空の struct・使う人の操作）。開く: other が無ければ左右（`SplitLayout::vertical`・操作中の枠は `first`）で `PreviewView` を置き、other が `PaneView` なら配置と側を保ったまま置き換え（枠は 2 つまで・ADR 0148 決定 1）、既に `PreviewView` なら閉じる（`without_split`）。置くのは `EditorState::with_preview` の 1 か所。開くときに前に開いていたときの残りを `collect()` で 1 回捨てる（たまりが空でないと次の合図が来ない・ADR 0062 決定 15 と同じ）。本文が `core::preview_byte_limit`（`16 * 1024 * 1024`・`MarkdownParse.hpp` の `markdown_size_fits` の隣の定数 1 つ）を超えるときは頼まず `notice` に「プレビューは 16 MiB までです」。
4. **プレビューの枠は操作中にならない。** `SwitchPane`（F6・`Ctrl-w w/W/p/h/j/k/l`・クリック）は other が `PreviewView` なら何も変えず `command_message` に「プレビューの枠には入れません」を 1 行（鳴らさない・情報）。`ClosePane`・2 枠の `:q` `:q!` `:wq` `:x`・`Ctrl-w c` `Ctrl-w q`・`Ctrl-w o`・`:only` は other が `PreviewView` なら `without_split`（本文は閉じない・`:q` 系と `Ctrl-w c` は `SwitchPane` を通さない）。`SplitPane`（Ctrl+\ 等・`Ctrl-w s/v`）は other が `PreviewView` なら捨てて `PaneView` を置く（通常の split と同じ）。`ScrollPane{lines}` は other が `PreviewView` なら錨を本文の行で動かす（決定 7）。`EditorState::with_pane_swapped` もプレビューへは移らない（呼び手の誤りでも状態を変えない）。
5. **port は券つきの非同期 1 本。** `application::MarkdownPort`: `void parse(MarkdownRequest)`（即時に返る）・`[[nodiscard]] std::vector<MarkdownOutcome> collect()`。形は `FolderPort` と同じで、合図を結ぶ `bind(std::function<void()>)` は adapter が持つ（port には置かない）。`MarkdownRequest{ ticket, text: shared_ptr<const std::string> }`（本文は UI スレッドで `TextBuffer::text()` が連続 1 本に写す・要求のときだけ）。`MarkdownOutcome{ ticket, result: std::expected<core::MarkdownDocument, core::MarkdownFailure> }`。`EditorPorts` に `MarkdownPort &markdown`。合成ルートは `Win32MarkdownAdapter` を folders の隣に置き、窓ができてから `work_signal()` を `bind` する。
6. **券。** 券は controller の数 1 つ（`preview_tickets_`）が要求ごとに 1 つ進め、`PreviewView.ticket` に写す。閉じて開き直しても戻さない（閉じる前の要求の結果を、開き直した後の券と取り違えない）。16 MiB を超えて頼まないときも券は進め、前の要求の結果を捨てる。`WorkCompleted` の `collect()` で、券が `PreviewView.ticket` と同じ結果だけを使い、ほかは捨てる（ADR 0062 決定 15）。成功は `document` に置いて `notice` を消す。`stopped` は捨てる（次の券が来る）。`too_large` は「プレビューは 16 MiB までです」、`parser_rejected` は「Markdown を解析できませんでした」を `notice` に 1 行。差し替わるまで前の `document` を出したまま（計画 §3.4・古い結果を確定として出さない）。`insufficient_memory`（builder か述語の `bad_alloc`・#817）は「メモリが足りずプレビューできませんでした」を 1 行（md4c 自身の確保の失敗は `parser_rejected` のまま）。
7. **錨と片方向の同期。** `anchor` は本文のバイト位置。`follows == true` の間は frame を作るときに `anchor = text.line_start(first_visible)`（本文が送られれば追従・状態には書かない）。`ScrollPane{lines}` が `PreviewView` に来たら、追従中なら今の先頭行の行頭を起点に、`line_of(anchor) + lines` を 1〜最後の行に収めて `line_start` を置き、`follows = false`。本文が送られる（frame の本文の先頭行が前の frame と違う）と `follows = true` に戻す。比べる前の値は `PreviewView` に持たず controller の `preview_body_top_` に持ち、読み書きするのは `frame()` の入口の `follow_preview_on_scroll` の 1 か所（`ScrollPane` はプレビューを送った時点の先頭行を置き直す）。枠に描くときは、`anchor` を含むブロック（`source_begin` の `upper_bound` − 1・無ければ 0）の上端を枠の上端に置く（ui・#801。frame は `anchor` を運ぶだけ）。
8. **`PreviewTick` は使う人の操作ではない。** `AutosaveTick` と同じく `keeps_tab_walk` と `begin_intent` の一覧に書く（Ctrl+Tab の歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない）。`TogglePreview` は使う人の操作の側で、どちらの一覧にも書かない。handler: other が `PreviewView` でなければ何もしない。`revision = history().revision()`。`revision == seen_revision && revision != requested_revision` なら頼む（打つのを止めてから 1 tick 以上）。そうでなければ `seen_revision = revision`。履歴の位置ではなく改訂番号で比べる（CLAUDE.md §2 の自動退避と同じ理由）。tick の間隔（200 ms）とタイマーは ui（#801）。
9. **即時の要求。** `TogglePreview` で開いたときと、アクティブな文書が替わったときは tick を待たずに頼む（`requested_revision = seen_revision = revision`・`document` と `notice` を捨て `follows = true`）。文書が替わる道は `enter_document` の 1 か所（`SwitchTab`・`WalkRecentTab` の 1 歩ごと・`CloseTab` の後の隣・`NewTab`・新しいタブへ開く・復元）と、無題の未変更タブへ開く `with_opened` の道の 2 つ。違う文書は改訂番号が同じ値になり得るので、tick の比較には任せない。
10. **静かな道。** プレビューが開いていない間は `MarkdownPort` を 1 回も呼ばない（`WorkCompleted` でも `collect()` を呼ばない）。契約 `verify_quiet_paths`（`--history`）と `verify_folder_quiet_paths`（`--background-work`）に替え玉の `requests().empty()` と `collects() == 0` を足す。`WorkCompleted` の handler の順は 退避 → フォルダ → Markdown。
11. **frame。** `PreviewFrame{ document: shared_ptr<const MarkdownDocument>, anchor: Offset, notice: optional<DisplayText> }`。`add_other_pane` は `std::visit` で `add_other_view` の 1 対（`PaneView` → `PaneFrame`・`PreviewView` → `PreviewFrame`）を選ぶ。`PreviewFrame` は値の写しだけで本文を歩かない（結果は `shared_ptr` を共有し、frame ごとに写さない）。renderer の `draw_other_body` も `std::visit` で `draw_other` の 1 対を選び、今は `PreviewFrame` の側は枠の地だけを塗る（中身は #801）。
12. **編集でずれるものは無い。** `with_other_adjusted`（編集で other の `PaneView` をずらす）は `PreviewView` では何もしない。追従中の錨は次の frame で先頭行から引き直す。`follows == false` の錨は編集の前後で動かさない（限界）。
13. **セッションに載せない。** `SessionCodec` は other を書かない（ADR 0148 決定 12）。プレビューは起動で開かない。
14. **限界と後続。** 言語（#776）による出し分けは後続で、今は操作中の文書が何でも Markdown として解析する。`:preview` は後続。プレビュー内の検索・リンクを開く・プレビューの枠を操作中にする・錨の編集追従（決定 12）は後続。

## 1 打鍵の道の費用

プレビューが閉じていれば、打鍵の道に足したものは `frame()` の入口の `follow_preview_on_scroll`（`std::exchange` 1 回と `preview()` の visit 1 回）と、`adjust_other` の中の visit 1 回だけ。開いていても打鍵は改訂番号を進めるだけで、本文の写し（16 MiB で数 ms）は tick の要求と文書の切り替えのときだけ。frame は結果の `shared_ptr` を写すだけで本文を歩かない。

## 限界（分かっていて今回は採らない）

- 文書の種類を見ない（`.txt` でも Markdown として解析する・#776）。
- `WalkRecentTab` は 1 歩ごとに文書が替わるので、歩くたびに頼む（確定まで待たない）。古い券の結果は捨てる。
- 錨は追従をやめた後の編集で動かない（決定 12）。
- md4c 自身の確保の失敗は `-1` で返るので `parser_rejected` と同じ 1 行になる（決定 6）。builder と述語の `bad_alloc` は `insufficient_memory`（#817）。

## 実装の所在（#801）

- 解析: `src/adapters/win32/Win32MarkdownAdapter` が `Win32Worker` に `parse_markdown` を載せ、打ち切りの問いは「worker の止める合図か、券が今の券でない」。結果は `MarkdownShelf`（`FolderShelf` の形・1 ファイル 1 型）に置き、棚は最新の結果 1 つだけを持つ（新しい結果が来たら古いものを置き換える・上限 1）。棚が空から 1 つになったときだけ合図する。今の券と閉じた印は棚の不可分な値（`std::atomic`・adapters/win32 だけ）。棚は仕事が `shared_ptr` で持ち、本文の `shared_ptr<const std::string>` も仕事が終わりまで持つ。壊す順は 窓 → controller → adapter（棚を閉じて走っている解析を打ち切る）→ worker。
- tick: `src/ui/win32/WindowTimer.hpp` の `WindowTimer::preview = 3` と `preview_interval_milliseconds = 200`。`EditorWindow::deliver` の終わりの `follow_preview_timer` が、other がプレビューの間だけ `SetTimer` を張り（張っていれば張り直さない）、外れたら `KillTimer`。`WM_TIMER` で `PreviewTick` を送る。
- 鍵と窓: `OperationShortcut.hpp` の `'M'` → `OperationKey::m`。プレビューの枠のクリックは何もしない（`SwitchPane` を送らず本文のキャレットも動かさない）・ホイールは `ScrollPane`。Ctrl+Shift+F4（`close_pane`）は other がプレビューなら `ClosePane` だけを送る。
- 描画: `Direct2DRenderer` が `PreviewPane`（`src/ui/win32/PreviewPane`）を 1 つ所有し、その中に `PreviewTextLayouts`・`PreviewBrushes`・縦の位置 `PreviewBlockTops` を frame をまたいで持つ。縦の位置は文書（指す先の同一性）・字体（書式・DPI）・錨の要素・枠の幅と高さが変わったときだけ測り直す。`BodyTextLayouts` と違い位置解決に使わないので application の `TextLayoutPort` に載せない（ADR 0114 の範囲外）。`draw_other_body` の `std::visit` の `PreviewFrame` の側が、地を受け身の枠と同じく塗り、`notice` があれば枠の上の 1 行に本文の色で左から書き、その下に `PreviewScene` を描く。枠の上端は `anchor` を含む要素（`source_begin` の `upper_bound` − 1・`preview_anchor_block`）の上端で、最初の要素なら上の余白ごと見せる。送りは px で持たない。文書が空（最初の解析の前）なら地だけ。配色は `core::selected_theme(settings, appearance)`、字体は `guifont` と pt と窓の DPI。
- 測るのは錨から見えている分だけ（#811 のレビュー R1）: `PreviewBlockTops::measure(document, layouts, style, PreviewViewport{first, width, height})` は錨の要素から順に高さを足し、枠の高さを超えたら止める。錨より前と枠の下の要素は測らず、総高さ（`total`）は持たない（縦のスクロールバーが無いので要らない）。文書が差し替わっても作る layout は見えている分だけ。リストの記号の列はリストの全体から、深さと記号の文字の長さごとに 1 つだけ測る（番号の数字は等幅）。リストの区切りを求める走査は文書の全体を 1 回なめる（layout は作らない）。16 MiB の文書の差し替えの費用は後続の Issue で測る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| プレビューを 3 枠目に置く | 枠は 2 つまで（ADR 0148 決定 1）。配置と `BodyTextLayouts` の幅の鍵を変えない |
| `PreviewView` を文書の束（`DocumentState`）に持つ | タブごとに結果を持つと切り替えで古い結果を出すか、全タブを解析し続けることになる。窓に 1 つで操作中の文書に追従する |
| 打鍵ごとに要求する | 16 MiB で数 ms の写しと数十 ms の解析を打鍵の道に載せる。tick で間引く |
| 券を `PreviewView` の中だけで数える | 閉じると券が 0 に戻り、閉じる前の要求の結果を開き直した後の券と取り違える |
| port に `bind` を置く | `FolderPort` と `AutosavePort` は合図を adapter に持つ。application は合図の結び方を知らない |
| tick の改訂番号で文書の切り替えも拾う | 違う文書の改訂番号は同じ値になり得る（どちらも 0 など）。切り替えは `enter_document` で即時に頼む |

## 検証

- 契約 `--preview`（`tests/unit/PreviewTests.cpp`）: 開く → `PreviewView`・券 1・本文の写しが要求に載る／`WorkCompleted` で今の券の結果が `document` に・古い券は捨てる・`stopped` は捨てる・`parser_rejected` と `too_large` は 1 行／tick の間引き（打鍵 → tick（要求なし）→ tick（券 2）→ tick（要求なし））／タブの切り替えと新しいタブで即時に頼み前の結果を捨てる／`SwitchPane` は変えず 1 行・`ClosePane` と `:q` と `Ctrl-w c` はプレビューだけを閉じる・`SplitPane` で本文の枠に・`TogglePreview` で本文の枠を置き換え・もう一度で閉じる／16 MiB + 1 バイトで要求 0 と 1 行／`ScrollPane` で錨が行で動き端で止まり、本文を送ると追従に戻る／tick は歩き・`.`・undo を動かさない／閉じている間と閉じた後は口を呼ばない／閉じて開き直すと券 2 で頼み券 1 の結果は捨てる／`Ctrl-w q` `Ctrl-w o` `:only` `:q!` `:wq` `:x` もプレビューだけを閉じ（`:wq` `:x` は保存してから）／開いたまま開く・閉じたタブの隣で即時に頼む／プレビューが開いているときの `WorkCompleted` は歩き・`.`・undo を動かさない。
- ui と adapters（#801）: `nib_window_tests --preview`（`PreviewPane` が同じ文書を 2 回描いても layout を作り直さない・錨の上の送り）・`--timers`（`preview` の番号と 200 ms）・`eng/verify-window.py --preview --scheme dark|light|grid`。
- 静かな道: `verify_quiet_paths`（`--history`）と `verify_folder_quiet_paths`（`--background-work`）。
- 既存: `--split` `--operations` `--autosave`、`nib_window_tests`（renderer の `std::visit`）。

## 追補 2026-10-22（#813）

- tick は変わったときだけ描く: `EditorDelivery.repaint` は `PreviewTick` が要求を出さなかったとき（見た改訂番号を進めただけを含む）と `AutosaveTick` が退避を頼まなかったときだけ false で、ui の `deliver` はそのとき `invalidate()` しない。結果を採るのは `WorkCompleted`（true）。契約は `--preview` の静かな tick 10 回と `--autosave`。
- リストの区切りは測る範囲だけ: `PreviewBlockTops::measure` はリストの区切りを錨の 1 つ前から前へ決め（錨より前の項目は読まない）、記号の列は測る範囲に入ったリストを範囲の中の最初の項目からリストの終わりまで（上限 10,000 項目・超えたら番号を 5 桁として）測って決める。番号は前へ増えるので最大の番号の桁数が入り、送っても字下げは動かない。読む要素は測る範囲＋1＋リストの残り（上限）以内で、文書の長さに依らない（`nib_window_tests --preview` の 10 万項目）。後の項目で列が広がれば錨から置き直す。
- tick は狙いの横位置を捨てない: `PreviewTick` は `keeps_wanted_x` に載り、プレビューが開いている間も `j` `k` と下向きの移動の狙いの横位置が残る（契約 `--preview`）。

## 追補 2026-10-27（#903）

プレビューの枠は枠の大きさのオフスクリーンの `ID2D1Bitmap1`（`D2D1_BITMAP_OPTIONS_TARGET`・描く先と同じ画素形式と alpha と DPI）に描き、鍵 `PreviewPictureKey`（描く先の同一性・文書の `shared_ptr` の同一性・錨の要素・枠の幅と高さの物理画素（知らせの帯の高さはここに入る）・`PreviewTextStyle`（DPI を含む）・テーマの名前（brush の鍵と同じ））が前の frame と同じなら `DrawBitmap` 1 回（最近傍・等倍・枠の左上は整数の画素）で枠を描く。鍵が違えば `GetTarget` で今の描く先を控えて `SetTarget(bitmap)` し、透明で消して原点 (0, 0) の枠に要素を描き、描く先を戻してから写す。要素を描く関数は `PreviewPane::draw_scene` の 1 本で、直接の道（`ID2D1DeviceContext` を引けない描く先・bitmap を作れないとき・失敗は知らせない）と bitmap の道が共用する。bitmap は大きさが同じなら使い回し、大きさが変われば作り直す。描画の失敗は今と同じ `RenderFailure` で返して保持の鍵を捨てる。device lost は renderer ごと作り直され、bitmap は renderer の持つ `PreviewPane` と一緒に消えるので、この経路では何もしない。文字の AA は device context の状態で描く先に依らず、要素の前に枠の全体を不透明な地で塗るので、本窓の描く先と同じ（試験は WIC の描画先で、bitmap を経た画素が直接描いた画素と一致することを確かめる）。段 1（PR #901・保持の探索・効果・矩形）が効かなかったのは、費用の主が見えている全要素の `DrawTextLayout` と記号・引用の線・行内コードの面そのもので、tick の無い打鍵では中身が変わらないのに毎 frame 描いていたからである。契約は `nib_window_tests --preview` の `check_pane_picture`（同じ鍵の 2 frame 目は要素の描画 0・`DrawBitmap` 1、錨・枠の大きさ・テーマの変化）。

## 改訂 2026-10-29（#1052・D71）: 見せ方 3 つと全面プレビューの選択

hide の要望（2026-10-28 夜）「全面 md プレビューも欲しい。編集はできないけど選択は出来る感じで」。決定 1〜14 はそのままで、次を足す。

15. **見せ方は閉じた enum `PreviewPlacement{off, side, full}`。** `side` は今の other の枠（決定 1）。`full` は本文の面の全部（gutter も本文も描かずプレビューだけ・タブの帯とステータスバーは残す・split の other は保ったまま隠す）。`PreviewView` は `side` と `full` で同じ型（錨・券・追従は共用）で、置き場だけが `SplitState.other` か `EditorState.full_preview`（`std::optional<PreviewView>`）かで違う。同時に 2 つは持たない（`full` にすると other の `PreviewView` は移る）。
16. **操作と Ex。** `toggle_preview`（Ctrl+Shift+M・側）はそのまま。`preview_full`（「Markdown プレビュー（全面）」・鍵なし・F1 の一覧）を足す。Vim は `:preview full` / `:preview side` / `:preview off` / `:preview?`（`ExPreviewRequest`・決定 14 の後続を閉じる）。Esc は `full` → 直前の見せ方（`side` から来たなら `side`・`off` から来たなら `off`）。
17. **編集はできない。** `full` の間、文字の鍵・IME・貼り付け・削除・Vim の変更の鍵（`i` `d` `p` …）は本文に届かず、1 行知らせる（「プレビュー中は編集できません。Esc で戻る」・鳴らさない）。届くのは: Esc・スクロール（ホイール・PgUp / PgDn・矢印・Vim の `j` `k` `Ctrl-d` `Ctrl-u` `Ctrl-f` `Ctrl-b` `gg` `G`）・選択（下）・Ctrl+C / `y`・Ctrl+A・タブの鍵（帯・Ctrl+Tab・`gt`）・Ctrl+P・F1・窓の操作・`:`（Ex は通す・`:w` も通す）。スクロールは錨（決定 7）を本文の行で動かし、`off` に戻るとき本文の見える位置を錨の行に揃える。
18. **選択はできる。** 描いた文字の上のマウスのドラッグと Shift+矢印で選ぶ。選択の単位は描いた layout の文字（block ごとの `IDWriteTextLayout` の hit test・位置は `PreviewBlockTops` と bitmap の保持（#903）の上に block ごとの layout の行の範囲を残す）。`PreviewSelection{block_begin, offset_begin, block_end, offset_end}` は `PreviewView` の値。Ctrl+C（Vim は `y`）で**見えている文字**（Markdown の記号を除いた描いた本文・block の境は改行 1 つ・リストの点と番号は入れない・コードブロックはそのまま）を `ClipboardPort` へ（文書の改行の形・ADR 0051 の規則）。Ctrl+A は全 block。選択の色は本文と同じ `selection` の面・新しいトークンは足さない。コピーの本文を作るのは core の純関数 `preview_selection_text(document, selection)`。
19. **port と静かな道は不変。** 解析は同じ券つきの `MarkdownPort`（決定 5・6）。`full` の間も `PreviewTick` と即時の要求（決定 8・9）は同じ。`off` なら port を呼ばない（決定 10）。
20. **契約。** application `--preview-full`（3 つの見せ方の遷移・Esc の戻り先・編集の拒否と知らせ・通る鍵の一覧・選択の範囲とコピーの本文（見出し・段落・リスト・コード）・錨の往復・`verify_quiet_paths`）。ui: `nib_window_tests` の画素（`full` で本文が描かれない・選択の面）と `eng/verify-window.py --preview-full`。速さ: `off` と `side` の打鍵の道は不変。`full` の間は本文を描かないので frame は軽い。
21. **限界（後続）。** リンクを開く・プレビュー内の検索・ダブルクリックの語の選択・選択の Markdown のソースのコピー（`:preview` の選択を本文の範囲に写す）は後続。
