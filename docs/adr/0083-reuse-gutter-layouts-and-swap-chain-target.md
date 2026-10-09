# ADR 0083 — 行番号の文字組みと描画先を寿命の内で再利用する

- 状態: 受理（実装・画素・性能の受理は別）
- 日付: 2026-10-09
- Issue: #324
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / ARC-011 / CPP-004 / CPP-014 / CPP-016 / CPP-017 / QLT-001 / QLT-010 / QLT-012 / QLT-013 / QLT-014 / ADR 0069 / ADR 0077

## 文脈

描画器は行番号を毎回組版し、一覧の題名は描画と後続の説明の位置決めのために二度組版する。swap chain の描画先 bitmap も毎回取得・生成し、`utf16_at` は長さしか使わない UTF-16 文字列を生成する。

事前調査の `display_line` の「3回decode」は不正確だった。実装は code point 数のための非継続バイトの走査、`code_point_at` の一回のdecode、`next_code_point` の境界走査、`append_utf8` の再encodeである。このADRでは再encodeだけを省く。改善量は事前の推定を実測のように扱わず、#329の同じ入力の比較で確かめる。

## 決定

1. **行番号は描画器だけが現在と直前の二列に保持する。** 専用 `GutterTextLayout`（所有する表示文字列・原点0の幅と高さ・COM layout）と `GutterTextLayouts`（二列の寿命と検索）を `ui/win32` に置く。キーは全文の文字列と寸法。原点、色、文書ID、行IDは含めない。毎描画のbeginで現在と直前を交換し、同じキーは直前から現在へ移し、endで直前の残りを捨てる。現在の中も検索し、同じ要求を同じ描画で重ねて保持しない。画面外の永続的な辞書は作らない。
2. **行番号の書式と生成は既存の正典を通る。** gutter書式は一つなので、書式を作り直す前に両列をclearし、字体とDPIの変更を寿命でキーに含める。保持の型はfactory・UTF変換・描画・色を知らず、生成は既存 `text_layout` の一か所。生成に失敗したnullは保持せず次回再試行する。公開lookupは不在をoptionalで示す。本文の字形保持やステータスの固定枠の形式は変えない。行番号はglyph収集をせず、右寄せの同じ書式と領域で `DrawTextLayout` する。
3. **一覧の題名は一回作ったlayoutを描画と寸法取得に使う。** 説明を持つ行で、同じ題名layoutを `DrawTextLayout` と `GetMetrics` に渡す。説明の開始位置・12 DIPの間隔・trim・描画順・色・領域は不変。説明を持たない行には現在の一回の `write` を残してよい。新しい永続キャッシュは作らない。
4. **swap chainの描画先bitmapはrendererが一個所有する。** 未取得のときだけ `GetBuffer(0)` → `CreateBitmapFromDxgiSurface` し、同じ資源の寿命の間再利用する。`ResizeBuffers` の前にcontextのtargetをnullにしてbitmapの参照を解放する。resizeの成功・失敗とも次回は必要なら新たに取得する。drawの `EndDraw` / `SetTarget(nullptr)` / Present の順、失敗の分類は保つ。bitmapとcontextのDPIは96固定なので、`set_dpi` だけでこの資源を作り直さない。device lostでrendererを捨てればこの資源も捨てられ、別deviceへ持ち越さない。根拠は [Microsoftのdevice context例](https://learn.microsoft.com/en-us/windows/win32/direct2d/devices-and-device-contexts) と [ResizeBuffersの参照解放の契約](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)。
5. **長さだけが必要なUTF-16変換は確保しない。** `core::Utf16` に `std::expected<std::size_t, TextFailure> utf16_length(std::string_view)` を加える。既存 `validate_utf8` で検証し、符号値は既存 `Utf8` で読み、U+10000以上を二単位とする私有の規則を `append_utf16` と共有する。UTF-8をuiで独自decodeしない。`utf16_at` は従来同様にbyte位置をclampしたprefixを渡し、失敗時は0。途中のbyte境界や不正UTF-8を旧 `widen` と違う成功値にしない。実文字列が必要な場所は従来の `to_utf16` のまま。
6. **`display_line` は置き換えない文字の元のUTF-8を写す。** 次の境界を先に求め、一度decodeした符号値で既存 `display_width` のswitchを通す。`^X` / `<xxxx>` / `<xx>` は既存の一か所で組み立て、通常文字は元のsliceをappendする。表示位置と本文桁の表の意味は不変。`starts` の容量を正確に予約するcode-point-countの前走査は残す。byte数+1の予約は日本語の表の容量を約三倍にするので採らない。一走査化は別候補として測ってから判断する。

## 検証

- Debug buildとclang-tidy、conformance/build graph、symbols、差分formatを対象とする。UTF処理の変更はDisplayLineとUTF16の既存試験および追加境界を選ぶ。ASCII・2/3/4byte、BMP/補助平面、空・不正・prefixの途中・長い列、制御文字/C1/Tab/結合文字・startsの両方向対応を確認する。
- `nib_window` に行番号保持の契約を加える。同じキー・文字列と寸法の違い・原点/色がキーでないこと・次フレームへの移動と古いものの破棄・clear・失敗を保持しないことを確認する。test専用の本番分岐は作らない。WICで行番号の右寄せと一覧題名の `DrawTextW` / `DrawTextLayout` の画素が一致することを確認する。これは変更した保持と描画の直接境界である。
- ADR0077の試験区画に窓・device・swap chainを追加しない。描画先bitmapの参照解放順はレビューし、実機では通常描画→resize→最小化/復元→字体変更の再描画を確認する。物理DPI遷移・device lostの実再現が無ければ未確認と記録する。
- rendererの共通描画を変えるため、親が既存22場面の前後画素比較を一度行う。同じ成功証拠をPR/mergeで再利用する。#329のdisplay-line-longとUTF16長さの同じ入力の前後比較を固定条件で行い、全値を残す。D41の正式速度ゲート・CI・独立レビュー・施主試用は別途必要で、未実施を合格と書かない。

## 却下と限界

- 汎用の無制限文字列cache、本文/ステータスcacheの巻き込み、target全体のSIMD指定は採らない。
- C4の走査回数を減らすためだけの過大な予約や別decoderを作らない。保持・コピー削減の実効果を先に測る。
- 微小な効果は外部負荷の揺れに埋もれ得る。正式ゲートの許容内であることだけで改善を主張しない。画素一致と同じ処理の比較を別々に記録する。
- 保存スキーマ、色、字体、配置、操作、undo/IMEの意味は変更しない。waiverなし。
