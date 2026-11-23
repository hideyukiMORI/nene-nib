# ADR 0192 — キャレットの動きは尾を引く多角形を本文の上に重ね、四隅は core の純関数、進みは合図の意図が運ぶ

- 状態: 提案（実装席 J 2026-11-22・施主の決め 2026-11-22 の形 A。動かさない条件と色は hide の試用の後に受理・D41）
- 日付: 2026-11-22
- Issue: #1933・見本 `D:/NeNeNib/evidence/design-20261122/caret-motion.html`
- 規則: ARC-001 / ARC-003 / ARC-007 / ARC-012、CPP-002 / CPP-011 / CPP-013、CNF-016、QLT-014、D60 / D82 / D91

## 文脈

hide は動く見本を見て「動きは A 尾を引く、粒なし、にじみあり、動き 120 ms だけど、設定値で変更もできるようにして」と決めた（#1933 のコメント）。既定はオフ。見本の式は「四隅のうち進む向きに近い隅ほど早く着く（係数 0.45 / 0.6 / 0.85 / 1.0 × 時間・ease-out の 3 乗）・動いている間は尾の側が紫から橙への勾配・にじみ」。

現在時刻を持てるのは `src/adapters/win32` だけ（ARC-007）。設定の `settings.v5` は 1.1.0 で鍵の集合を閉じた（CLAUDE.md §2 の設定の節）。

## 決定

1. **設定は `settings.v6` の 2 つの鍵。** `caret_motion=on|off`（既定 off）と `caret_motion_time=<ms>`（既定 120・40〜300・10 進・先頭の 0 なし）。v6 は v5 の全部の鍵を同じ名前・同じ順で持ち、2 つの鍵は `finder_preview` の後ろ・`lsp.<language>.command` の前に書く。版は `SettingsVersion::v6`、経路は `SettingsPaths.current`（`settings.v6`）で、v5 は `shipped`（1.1.0 が読み書きする版）へ移した。読む順は `reading_order` の 1 か所（v6 → v5 → v4 → v3 → v2 → v1）。v6 が無いときだけ v5 を 1.1.0 と同じ厳しさで読み（v5 に v6 の鍵があれば未知の鍵として拒否）、2 つの鍵は既定を補う。書くのは設定を変えたときだけで、旧ファイルは bytes のまま残す（ADR 0114 の形のまま）。`:set caretmotion` `nocaretmotion` `caretmotion?` と `:set caretmotiontime=N` `caretmotiontime?`（範囲の外は E474 と同じ失敗）、設定の画面は「編集」の節にスイッチ（`EditorFeature::caret_motion`・自分の鍵）と段の上下（`SettingsValue::caret_motion_time`・20 ms ずつ）。**v6 を書く最初の公開版はまだ決まっていない**（次の版で鍵の集合を閉じる）。
2. **四隅は core の純関数。** `CaretGlide.hpp` の `caret_motion_followed(track, target, clock, duration)` が 1 frame 分の覚え `CaretMotionTrack` を作る。行き先（その frame のキャレットの矩形）が前と違えば、今描いている四隅（本文の領域の中へ寄せる・画面の外から来る尾は画面の端から）から動きを始め直し、動きの番号 `generation` を 1 つ進める。同じなら、合図が運ぶ経過で四隅を進める。隅 i の進みは `elapsed / (duration × 係数 i)` を 0〜1 に切り詰めて ease-out の 3 乗で写す。係数は「行き先の中心から見て進む向きへ遠い隅」から 0.45 / 0.6 / 0.85 / 1.0（見本と同じ）。途中で次の移動が来たら、今の四隅から次へ向かう。時間が 0 なら跳ぶ。試験は時計を読まない（`--caret-motion`）。
3. **進みは合図の意図 `CaretMotionTick` が運ぶ。** 経過は ui が application の port `MotionClockPort`（実装は adapter の `Win32MotionClock`・QueryPerformanceCounter）で測る。窓は描いた後に renderer の `caret_motion_generation()` を読み、動いている間だけ窓のタイマー `WindowTimer::caret_motion`（10 ms）を張り、番号が変わったら経過の起点を測り直す。タイマーが切れるたびに `CaretMotionTick{generation, elapsed}` を送り、controller は値を覚えて frame の `caret_motion` に載せるだけ。止まったらタイマーを外す（止まっているときの CPU は 0）。窓が最小化されているか見えなければ（`IsIconic`・`IsWindowVisible`）、タイマーの手続きが合図を送らずにタイマーを外し、renderer の覚えを捨てる（最小化した窓には `WM_PAINT` が来ないことがあり、描いた後だけに頼らない・次に見えた frame は跳ぶ・#1956 の R1）。renderer の frame ごとの覚えは ui の `CaretMotionFollower` の 1 つで、窓は描く前に毎回「最後に追った動きの番号」を下限として渡す。renderer を作り直しても（device lost・本物の device の採用）番号は下限より上から数えるので、controller に残った前の renderer の進みと次の動きの番号が当たらない（作り直しのとき controller の進みを捨てる案は、合図の意図を 1 つ増やし作り直しの道ごとに送る所が要るので採らない・#1956 の R2）。renderer は frame の番号が今の動きと同じときだけ経過を読み、違えば 0 と読む（古い動きの進みで新しい動きを進めない）。`CaretMotionTick` は `detail::is_signal` に載る合図で、`keeps_tab_walk` と `begin_intent` は `not_user_operation` を経て合図の側に数える（歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない）。
4. **描く所は本文の字形の上に重ねる層。** `BodyTextPainter` の字形と `BodyTextLayouts` の保持は作り直さない。動いている間は行の描画でふつうのキャレット（縦棒・ブロックの塗りと字の描き直し）を描かず、操作中の枠を描き終えた後に `CaretMotionPainter` が四隅の多角形を本文の領域に切って描く。本体は尾の側（出発の中心）から頭（行き先の中心）への線形の勾配で、頭はキャレットの色 `accent`、尾の先は既存のトークン `ime`（組み込みの 2 つの配色では茄子色の側の紫 #D7C4E5 / #5E2750・トークンは足さない）。にじみは頭の色の半透明の多角形を外へ 3 段（6 DIP × 1.0 / 0.6 / 0.3・不透明度 0.08 / 0.12 / 0.18）広げて重ねる（Direct2D の効果は使わない・1 frame あたり path geometry 4 つと勾配の brush 1 つ）。IME の候補窓の位置は行き先。
5. **いつ動かすか。** キャレットの矩形が変わったとき全部（打鍵・Vim の移動・検索・クリック・スクロールで画面の中の位置が変わったとき・挿入の縦棒とブロックの形の切り替え（四隅がそのまま新しい形へ向かう）・折り返しの行またぎ・格子（巨大な行）の画面行・split で操作中の枠が替わったとき・診断の跳び `]d` `[d` F8 と Ctrl+P の一覧から選んだ着地）。タブが変わった frame（`active_tab` が変わった）は跳ぶ。操作中の枠の側（`active_side`）が同じまま本文の領域が変わった frame（split の境界線のドラッグ #1941・窓の大きさ・エクスプローラーの開閉）も跳ぶ（枠の配置で画面の位置が動いただけで、キャレットは動いていない・main の取り込みで足した・試験は `nib_window_tests --caret-motion`）。
6. **動かさないとき。** 設定がオフ・Windows の「アニメーション効果」がオフ（`SPI_GETCLIENTAREAANIMATION`・窓が起動と `WM_SETTINGCHANGE` で読み、描く前に renderer へ置く）・IME の変換中（frame に変換がある）・キャレットが描かれない frame（入力行・印の 1 パス・面・全面のプレビュー）の後は跳ぶ。frame の入口と出口（`begin` と `end`）は本文の面を描く `draw_body_area` の 1 本で、本文を描かない全面のプレビューの道も通る。出口はその frame でキャレットが見えなければ覚えを捨てるので、本文を描かない道を後から足しても動きの番号は返らない（#1956 の M1・試験は `nib_window_tests --caret-motion`）。受け身の枠（split のもう 1 つ）の印は動かさない。Vim の矩形の挿入は Vim と同じくキャレットが 1 つなので、特別な扱いは要らない。
7. **オフの道に費用を足さない。** オフのとき renderer が読むのは `frame.settings.caret_motion` の 1 つで、行の描画は今と同じ道を通る（`CaretMotionFollower::on()` の 1 つの真偽で分ける）。オンの費用は別の基準で扱う（D60 の LSP と同じ考え方・速さのゲートの `key-to-frame-*` には混ぜない）。

## 限界

- タブの見分けは `active_tab` の位置で、閉じた後に同じ位置へ来たタブは跳ばずに動く。
- 窓の合図は OS のタイマー（最小 10 ms・実際は 15.6 ms の刻みのことがある）なので、120 ms の動きは 8〜12 frame。
- renderer を作り直す（device lost・本物の device の採用）と覚えは空から始まり、最初の frame は跳ぶ（番号は窓の下限より上から数えるので、古い進みと当たらない・決定 3）。
- 道具（`eng/window_driver.py` の `SETTINGS_FILE`）は `settings.v6` だけを書く。v6 を読まない exe（1.1.0 とこの ADR より前の main）を道具で動かすと、書いた設定は効かず既定のまま動く（minimap・lsp のベンチと `verify-settings.py` など。設定を書かない 8 本の基準のベンチは影響しない）。古い exe を測る道具は今は無いので直さない（#1956 の R5）。

## 未決

- 尾の色（`ime`）は hide の試用で決める（#1956 の R3）。派生の配色（123 個）では `ime` が前景 75% ＋ `accent` 25% なので、尾は字の色に近い。`ime` と `accent` の RGB の成分の差の最大は中央値 72・最大 192 で、差 0（尾と頭が同じ色・勾配が見えない）が 4 つ（intellij-light・monalisa・quiet・ron）、32 未満が 8 つ、48 未満が 26 個。試用では差の小さい側（catppuccin 29・lunaperche 23）と大きい側（monokai）も見る。
- ブロックの形で動いている間は、行き先の字の上にも多角形が載る（ふつうのブロックの「字の描き直し」をしない）。尾が行の字の上を通るときも同じ。動きは 300 ms 以内なので、試用で見て決める（#1956 の L6）。

## 結果

- 設定の版が 1 つ増える（`settings.v6`）。1.1.0 の書く v5 の見本は `nib_adapter_tests` の `released_v5_settings`。
- 仕様（SPECIFICATION.md）の D 番号への記入は設計席が文書の PR で行う。

## 追補（2026-11-23・#1979）打ったときの粒

施主の決め（2026-11-23 朝・#1979）「足して既定オフで。」。D108 の「粒なし」は既定の形として残し、見本（`caret-motion.html` の `sparks`）の粒を設定で入れられるようにする。設計席の決め 1〜7 を次の形で入れた（提案・hide の試用の後に受理・D41）。

1. **設定は `settings.v6` の鍵 1 つ。** `caret_sparks=on|off`（既定 off）を `caret_motion_time` の後ろ・`lsp.<language>.command` の前に書く。v6 は未公開なので足してよい（v5 / v4 には足さない。v5 に在れば未知の鍵として拒否する・決定 1 と同じ形）。`:set caretsparks` `nocaretsparks` `caretsparks?`（短い名前は無い）、設定の画面は「編集」の節のキャレットの動きの 2 行の後ろにスイッチ 1 行（`EditorFeature::caret_sparks`・自分の鍵・文は `UiMessage` の 2 つを 3 言語に）。
2. **粒は `caret_motion` と独立。** 動きがオフでも粒だけ出せる。Windows の「アニメーション効果」がオフ（決定 6 と同じ `animation_setting_` を窓が renderer へ置く）・IME の変換中（frame に変換がある）は出さない。止めている間の打鍵は、後から粒にならない（通し番号だけを覚える）。
3. **粒が出るのは「使う人が打った文字が本文に入った」とき。** application が打鍵の通し番号 `EditorFrame.typed` を持ち、進めるのは 2 か所だけ。通常モードは `accept(InsertText)`（WM_CHAR・Tab・通常モードの IME の確定）で、本文の改訂番号が進んだときだけ（読むだけの文書で入らなかった打鍵は数えない）。F5 の日時は同じ挿入の道 `insert_text` を通るが数えない。Vim は #1962 の「打った文字か」の印（`watched_vim_key` が流す鍵ごとに置く `typed_insert_` と、鍵の後に `typed_vim_character` が見る「流す前のキャレットの直後にそのまま入ったか」）をそのまま使い、見張りを「言語サーバーが running か粒がオンか」で立てる（言語サーバーへ渡すのは今までどおり running のときだけ）。打った鍵の入口 `accept(VimKeyPress)` だけが通るので、再生（`.` `@`）・`:normal!`・Vim の IME の確定・`map` の右辺・1 鍵の引数（Ctrl-r・Ctrl-v）は数えない。貼り付け・undo / redo・キャレットの移動はどちらの道も通らない。Nib に REPLACE モード（`R`）はまだ無い（`VimAction.hpp`）ので、数えるのは INSERT だけ。engine は変えない。
4. **乱数と時刻を core / application に持ち込まない。** 粒の姿は core の純関数 `caret_spark_at(serial, index, age)`（`CaretSparks.hpp`）の 1 本で、向き・速さ・大きさ・色は打鍵の通し番号と粒の番号の整数のハッシュ（lowbias32）から決まる。見本と同じく 1 打鍵 7 個・寿命 420 ms・扇は真上から ±1.1 rad（三角関数を使わず 9 本の向きの表を線形補間・core から libm のシンボルを出さない）・速さ毎秒 40〜130 DIP・下向き毎秒毎秒 260 DIP・一辺 2〜4.5 DIP・不透明度は寿命の間に 1 から 0 へ線形・16 のうち 6 が 2 つ目の色。経過は決定 3 と同じ道で、窓が時計の port で測り、合図 `CaretMotionTick` の 2 つ目の欄 `sparks`（粒の時計の番号と経過）が運ぶ。同じ通し番号と同じ経過なら同じ絵（試験は値と画素で縛る）。
5. **上限と止まり方。** 生きている束は直近の 8 打鍵まで（`caret_spark_bursts`・粒は最大 56 個・越えたら古い束を捨てる）。覚え `CaretSparkTrack` の進め方は core の純関数 `caret_sparks_followed`（寿命の過ぎた束を捨て、通し番号が変わってキャレットが見えた frame に束を足す・生きている束が 0 から 1 つになるたびに粒の時計の番号を 1 つ進める）。renderer の frame ごとの覚えは ui の `CaretSparksFollower` の 1 つで、粒が全部消えたら番号を返さないので、窓は合図のタイマーを外す（動きと粒の両方が止まったときだけ外す・タイマーは `WindowTimer::caret_motion` の 1 つを共用）。窓が見えなくなったときは動きと同じく粒も捨てる（#1956 の R1）。作り直した renderer は窓が最後に追った粒の時計の番号を下限に数える（#1956 の R2 と同じ形）。粒の時計の経過は動きと別の起点で測る。
6. **色は既存のトークン。** 1 つ目は `accent`（キャレットの色）、2 つ目は尾の先と同じ `ime`（組み込みの 2 つの配色では茄子色の側の紫・見本の violet に当たる）。トークンは足さない。粒は本文の領域で切って、操作中の枠を描いた後（動きの多角形の後）に重ねる（`CaretSparksPainter`・四角を塗るだけで Direct2D の効果は使わない）。
7. **オフの道。** 打鍵の道に足すのは真偽 1 つの読み（通常モードは `accept(InsertText)` の入口、Vim は `accept(VimKeyPress)` の「言語サーバーが起きているか」の隣）。描く側は frame の入口で設定の欄を 1 つ読み、キャレットを描く所は「動きか粒のどちらかがオンか」の真偽を読むだけ。オンでも速さのゲートの 8 本（既定オフで測る）は変わらない。オンの費用は動きと同じく打鍵の基準に混ぜない（`docs/quality/feature-costs.md` の行）。

### 限界（追補）

- 粒の時計は合図の届いた時点の経過で束の生まれを数えるので、粒が生きている間に打った次の束は最大 1 合図ぶん（10〜16 ms）遅く生まれ、その分だけ長く見える。
- 粒の位置は打ったときの画面の位置に固定で、直後のスクロールには付いて行かない（寿命 420 ms の間だけ）。
- Vim の IME の確定（`type_as_vim_keys`）は打った鍵の入口を通らないので粒は出ない（通常モードの IME の確定は出る）。