# ADR 0090 — ブロックカーソルと録画表示も保持済みの文字を使う

- 状態: 受理（比較後に採否判断）
- 日付: 2026-10-09
- Issue: #335
- 規則: ARC-001/002/004/008/011、CPP-002/004/008/011/012/016/017、QLT-001/012/014、D41

## 文脈

#334の2a31bb4は本文glyphとstatus固定枠を保持するが、ブロックカーソル内の文字は行全体のDrawTextLayoutを再び通り、録画表示の位置は直前に描いたmode_labelのlayoutをもう一つ作って測る。読み取り調査はこれらをR3/R7と記録した。推定時間を採用根拠とはしない。

## 決定

1. 本文の保持glyph選択と描画loopは既存draw_body_text一つのまま、private引数にD2D1_DRAW_TEXT_OPTIONSを受ける。通常本文の既存callerはCLIP、ブロックカーソルはNONEを渡す。glyphが無ければ渡されたoptionsで従来のDrawTextLayoutへ戻す。
2. 保持glyphがある場合、CLIPだけが従来どおり行areaのclipをpush/popする。NONEではcallerが既に置いたblockのclipだけを使う。ブロックの塗り・角丸・HitTest・最小幅・caret_rectangle・on_accent・描画順は不変。glyphの新収集、切出し、置換、cache keyや寿命の変更はしない。collectorが全体棄却した行は同じfallbackへ戻る。96dpi・恒等・整数原点の前提はADR0073/0077のまま。
3. statusの固定枠の照合/作成/位置前進をprivate status_text_layoutへ集め、const StatusTextLayout&をその場だけ借りる。枠は従来の固定arrayで、text/format/幅高さと寿命の規則を変えない。描画もprivate draw_status_textへ集め、通常write_statusはこの2つを呼ぶ薄いwrapperになる。
4. draw_status_leftはmode用の枠を一度だけ取得し、描画してからその同じ枠をdraw_recordingへ渡す。recordingが無い時はGetMetricsを呼ばない。ある時のwidthIncludingTrailingWhitespace/ceil/12DIPの隙間/右項目の手前のclip/recording文字と色は従来通り。cursor-1で探さず、ComPtrをcopyせず、width用の別cacheも作らない。
5. 作成失敗で空になったmode枠はそのframeのmode/recordingを描かず次frameで再試行する。従来はmode描画に失敗した後、recording側が同じlayoutをもう一度生成できた。本変更ではADR0070決定5の「空枠・次描画で再作成」に揃え、同frameに第二の生成経路を置かない。GetMetrics失敗もrecordingだけをそのframeで省く。UI書式再作成前の破棄、DPI/device寿命は同じ。

## 対象検証と比較

- rendererと直接依存をDebug/Releaseのstrict/tidyでbuildし、format/conformance/build graphを確認する。core/application sourceは変えないため既存成功を再利用し、全unitを回さない。
- tests/uiの既存WIC glyph検証へ、同じlayoutのNONE+caller clipと保持glyph+同じclipの画素一致を追加する。左右端/中ほどの狭いclipを3family/3size/4width/5textの既存入力で確認し、以前の全文画素と所有寿命検証は残す。WICへの時計追加はしない。
- GPU実機の同一画面比較は、通常本文、NORMALのblock、録画開始/INSERT/終了、幅/字体/テーマを含める。字形の未対応fallbackとリソース失敗の限界は記録する。
- before/afterの同じ固定文書とVim非編集移動/録画操作で、入力受理から当該frame提示までを固定交互比較する。これはrendererだけのCPU時間ではなく一つの描画操作の区間であり、比較の限界を明記する。全値を保存し、外れ値の選別/基準変更/合格までの再測定をしない。正式gate/独立review/CI/実機試用は最終統合で確認する。

## 限界

録画の失敗frameは決定5のとおり第二生成をしなくなる。生成の失敗注入は既存同様レビュー事項。WICはGPU/ClearTypeと別の証拠。glyph全列の走査とGetMetrics自体は残り、測定で差が見えなければそのまま記録する。保存schema/入力/IME/本文状態は不変、waiverなし。
