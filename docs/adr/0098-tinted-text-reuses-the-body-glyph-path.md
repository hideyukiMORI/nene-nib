# ADR 0098 — 色を塗り直す文字も本文の字形経路を使う

- 状態: 設計固定・同値と固定比較による採否待ち
- 日付: 2026-10-10
- Issue: #362
- 規則: ARC-001 / ARC-002 / ARC-004 / ARC-007 / ARC-008 / ARC-011 / CPP-003 / CPP-008 / CPP-011 / CPP-012 / CPP-016 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014 / D41

## 文脈

R3のblockはADR0090で保持glyphへ統一したが、tint_runsは通常本文の制御文字置換と本文/入力欄IMEのother節でDrawTextLayoutを範囲ごとに呼ぶ。#360は端点のprefix反復を除いたものの、置換1024の製品応答は約64.8msを残す。これは全応答の観測であり、tint単体の所要時間とはしない。

## 決定

1. tint_runs内のDrawTextLayout一呼出しを既存draw_body_text(text, area, D2D1_DRAW_TEXT_OPTIONS_NONE)へ置換し、不要になるorigin localだけ削除する。brushの色、runs_of、範囲順、callerのclipのpush/pop、paint順を保つ。
2. draw_body_text/BodyGlyphCollector/cacheの実装を変更しない。通常本文とIME合成行はlayout_ofで現在frameに登録された同じlayoutを使う。保持glyphがあれば同じ全保持runを描き、glyphが無い/未登録なら従来のDrawTextLayout(NONE)へ戻る。glyph切出し/色ごとのcache/新decoder/描画方式の分岐は追加しない。
3. command/palette入力欄のlayoutは本文保持に登録しない。したがって既存lookup missが正規fallbackを選び、中央寄せ/高さ調整/整数に揃えたIME原点とcaller clipを保つ。本文保持へ入力欄を混ぜない。
4. 製品contextは96dpi・恒等変換・整数area原点、collectorは同じ前提のまま（ADR0073/0077）。OS windowのDPI120は文字寸法へ適用されるがcontextの96dpiとは別である。描画/HitTest/所有/失効/透明度/brush/設定schema/警告/flags/基準/許容/fixture/抑制を変えない。

## 直接検証

通常Debugの実製品を正規tidy/ASan/UBSanでbuildし、変更sourceのformat/conformanceを確認する。新source/target/API/試験口を追加しない。既存WICのNONE+caller clip画素比較は同じdraw/collector/試験の成功を再利用し、変更されていない1521等の全scopeを一律に再実行しない。実Rendererの到達とGPU/ClearTypeの同値は次の実機比較で別に確かめる。

通常Release前後で同じ文書/設定/絶対path/1280×800/DPI120/刺激の本文/statusを完全画素比較する。固定入力はASCII制御文字、BMP/補助平面/結合文字/Tab、双方向文字、複数置換、右端clipを含める。選択/検索/current/blockの重なり、幅/字体/サイズ/テーマも対象とする。実日本語IMEは本文と検索/一覧の入力欄でcomposition/Space/確定またはcancelを記録し、元のIME open statusへ戻す。刺激の差分と本文/設定の不変、正常終了を保存する。異なるOS候補や差を成功まで撮り直さない。

## 固定性能比較と正式gateの再利用

前版は#360 ca4ad39の保存済み通常Release（現main3b114d6と製品/test/flags一致）、後版も通常Release。既存製品--measure/window_driverと#360の入力対応harnessを使い、`a\x01 `×1024、`日a🖋\u200b `×1024、`a\x01 `×2を固定する（すべて末尾CRLF）。検索/a<CR>準備後、n/Nの暖機一組、20回の交互n/N、0.4秒間隔、ABBA×3で各120対応組を一度比較する。原marksを次inputまでで分割し、暖機2と計測20すべてのframe到達、試料と区間値の一致を確認。画像の到達と復帰、全trial相互の同値を検査し、全試料/失敗/遅い組を残す。起動/準備/検索生成を含めず、GPU完了ではない製品応答として扱う。

正式gateの#360のsingle/burst200/burst200-16MiB/single-long-line成功を再利用する案を先行固定する。これらの入力はIMEを作らず、制御置換のない本文と空文書であり、今回変更するtint_runsへ入らない。基点からdraw_body_text/collector/幾何/本文経路/道具/入力/flagsを変えないことを機械照合し、独立レビューで経路を確認する。共通の描画経路を変える追加修正が必要ならこの再利用根拠は失効し、その直接境界を選び直す。単にcommit/工程が違うだけで既存全8benchを回さず、正式gate未実行の候補を実行済みとも記さない。

独立レビュー、根拠付き正式gate再利用、CI、実機の同値と密/疎3条件の利益・代償を合わせて採否判断する。利益を確認できない/画素が違う場合は採用しない。RSS/実確保回数、実Rendererの資源失敗注入、任意装飾/IME/フォント/DPI/全入力は未測として残す。waiver none。
