# ADR 0191 — キャレットの動きは尾を引く多角形を本文の上に重ね、四隅は core の純関数、進みは合図の意図が運ぶ

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
3. **進みは合図の意図 `CaretMotionTick` が運ぶ。** 経過は ui が application の port `MotionClockPort`（実装は adapter の `Win32MotionClock`・QueryPerformanceCounter）で測る。窓は描いた後に renderer の `caret_motion_generation()` を読み、動いている間だけ窓のタイマー `WindowTimer::caret_motion`（10 ms）を張り、番号が変わったら経過の起点を測り直す。タイマーが切れるたびに `CaretMotionTick{generation, elapsed}` を送り、controller は値を覚えて frame の `caret_motion` に載せるだけ。止まったらタイマーを外す（止まっているときの CPU は 0）。renderer は frame の番号が今の動きと同じときだけ経過を読み、違えば 0 と読む（古い動きの進みで新しい動きを進めない）。`CaretMotionTick` は `detail::is_signal` に載る合図で、`keeps_tab_walk` と `begin_intent` は `not_user_operation` を経て合図の側に数える（歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない）。
4. **描く所は本文の字形の上に重ねる層。** `BodyTextPainter` の字形と `BodyTextLayouts` の保持は作り直さない。動いている間は行の描画でふつうのキャレット（縦棒・ブロックの塗りと字の描き直し）を描かず、操作中の枠を描き終えた後に `CaretMotionPainter` が四隅の多角形を本文の領域に切って描く。本体は尾の側（出発の中心）から頭（行き先の中心）への線形の勾配で、頭はキャレットの色 `accent`、尾の先は既存のトークン `ime`（組み込みの 2 つの配色では茄子色の側の紫 #D7C4E5 / #5E2750・トークンは足さない）。にじみは頭の色の半透明の多角形を外へ 3 段（6 DIP × 1.0 / 0.6 / 0.3・不透明度 0.08 / 0.12 / 0.18）広げて重ねる（Direct2D の効果は使わない・1 frame あたり path geometry 4 つと勾配の brush 1 つ）。IME の候補窓の位置は行き先。
5. **いつ動かすか。** キャレットの矩形が変わったとき全部（打鍵・Vim の移動・検索・クリック・スクロールで画面の中の位置が変わったとき・挿入の縦棒とブロックの形の切り替え（四隅がそのまま新しい形へ向かう）・折り返しの行またぎ・格子（巨大な行）の画面行・split で操作中の枠が替わったとき）。タブが変わった frame（`active_tab` が変わった）は跳ぶ。
6. **動かさないとき。** 設定がオフ・Windows の「アニメーション効果」がオフ（`SPI_GETCLIENTAREAANIMATION`・窓が起動と `WM_SETTINGCHANGE` で読み、描く前に renderer へ置く）・IME の変換中（frame に変換がある）・キャレットが描かれない frame（入力行・印の 1 パス・面）の後は跳ぶ。受け身の枠（split のもう 1 つ）の印は動かさない。Vim の矩形の挿入は Vim と同じくキャレットが 1 つなので、特別な扱いは要らない。
7. **オフの道に費用を足さない。** オフのとき renderer が読むのは `frame.settings.caret_motion` の 1 つで、行の描画は今と同じ道を通る（`caret_motion_on_` の 1 つの真偽で分ける）。オンの費用は別の基準で扱う（D60 の LSP と同じ考え方・速さのゲートの `key-to-frame-*` には混ぜない）。

## 限界

- タブの見分けは `active_tab` の位置で、閉じた後に同じ位置へ来たタブは跳ばずに動く。
- 窓の合図は OS のタイマー（最小 10 ms・実際は 15.6 ms の刻みのことがある）なので、120 ms の動きは 8〜12 frame。
- renderer を作り直す（device lost・本物の device の採用）と覚えは空から始まり、最初の frame は跳ぶ。

## 結果

- 設定の版が 1 つ増える（`settings.v6`）。1.1.0 の書く v5 の見本は `nib_adapter_tests` の `released_v5_settings`。
- 仕様（SPECIFICATION.md）の D 番号への記入は設計席が文書の PR で行う。
