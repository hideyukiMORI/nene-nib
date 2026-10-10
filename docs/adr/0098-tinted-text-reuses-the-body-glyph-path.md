# ADR 0098 — 色を塗り直す文字も本文の字形経路を使う

- 状態: 技術受理（2026-10-10・混在長行の応答短縮と同値を確認）
- 日付: 2026-10-10
- Issue: #362
- 規則: ARC-001 / ARC-002 / ARC-004 / ARC-007 / ARC-008 / ARC-011 / CPP-003 / CPP-008 / CPP-011 / CPP-012 / CPP-016 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014 / D41

## 文脈

R3のblockはADR0090で保持glyphへ統一したが、tint_runsは通常本文の制御文字置換と本文/入力欄IMEのother節でDrawTextLayoutを範囲ごとに呼ぶ。#360は端点のprefix反復を除いたものの、置換1024の製品応答は約64.8msを残す。これは全応答の観測であり、tint単体の所要時間とはしない。

## 決定

1. tint_runs内のDrawTextLayout一呼出しを既存draw_body_text(text, area, D2D1_DRAW_TEXT_OPTIONS_NONE)へ置換し、不要になるorigin localだけ削除する。brushの色、runs_of、範囲順、callerのclipのpush/pop、paint順を保つ。
2. draw_body_text/BodyGlyphCollector/cacheの実装を変更しない。通常本文とIME合成行はlayout_ofで現在frameに登録された同じlayoutを使う。glyphs_readyなら同じ全保持runを描き、保持glyphが利用不可（!glyphs_ready）/未登録なら従来のDrawTextLayout(NONE)へ戻る。取得成功の空runは正常な空描画でありfallback条件とはしない。glyph切出し/色ごとのcache/新decoder/描画方式の分岐は追加しない。
3. 入力欄のlayoutは本文保持に登録しない。IMEが実際に到達する一覧入力欄では既存lookup missが正規fallbackを選び、中央寄せ/高さ調整/整数に揃えたIME原点とcaller clipを保つ。Ex/検索入力は既存ImeStance::closedとcomposition_ignoredによりIMEを受けないため、そのIME描画を実機到達として数えない。本文保持へ入力欄を混ぜない。
4. 製品contextは96dpi・恒等変換・整数area原点、collectorは同じ前提のまま（ADR0073/0077）。OS windowのDPI120は文字寸法へ適用されるがcontextの96dpiとは別である。描画/HitTest/所有/失効/透明度/brush/設定schema/警告/flags/基準/許容/fixture/抑制を変えない。

## 直接検証

通常Debugの実製品を正規tidy/ASan/UBSanでbuildし、変更sourceのformat/conformanceを確認する。新source/target/API/試験口を追加しない。既存WICのNONE+caller clip画素比較は同じdraw/collector/試験の成功を再利用し、変更されていない1521等の全scopeを一律に再実行しない。実Rendererの到達とGPU/ClearTypeの同値は次の実機比較で別に確かめる。

通常Release前後で同じ文書/設定/絶対path/1280×800/DPI120/刺激の本文/statusを完全画素比較する。固定入力はASCII制御文字、BMP/補助平面/結合文字/Tab、双方向文字、複数置換、右端clipを含める。選択/検索/current/blockの重なり、幅/字体/サイズ/テーマも対象とする。実日本語IMEは本文と一覧の入力欄でcomposition/Space/確定またはcancelを記録し、元のIME open statusへ戻す。変換前のother節と横溢れも確認する。刺激の差分と本文/設定の不変、正常終了を保存する。異なるOS候補や差を成功まで撮り直さない。

初回の旧版GUI道具は検索欄もIMEを受けると誤認し、other色が0のため未到達として失敗を記録した。画像にはASCIIのnihongoが現れ、既存ImeStanceとcomposition_ignoredの拒否を確認した。製品不具合や変更後の退行とは扱わず、初回script/rawを保持して直接範囲を本文/一覧へ訂正する。未実行だった一覧は別folderで採る。

## 固定性能比較と正式gateの再利用

前版は#360 ca4ad39の保存済み通常Release（現main3b114d6と製品/test/flags一致）、後版も通常Release。既存製品--measure/window_driverと#360の入力対応harnessを使い、`a\x01 `×1024、`日a🖋\u200b `×1024、`a\x01 `×2を固定する（すべて末尾CRLF）。検索/a<CR>準備後に先頭のaへ固定してから、n/Nの暖機一組、20回の交互n/N、ABBA×3で各120対応組を一度比較する。ASCII/短行はgg0、混在はgg0lで開始し、準備input数は7/8/7。間隔はASCII/短行0.4秒、混在3秒。原marksを次inputまでで分割し、暖機2と計測20すべてのframe到達、試料と区間値の一致を確認。画像の到達と復帰、全trial相互の同値を検査し、全試料/失敗/遅い組を残す。起動/準備/検索生成を含めず、GPU完了ではない製品応答として扱う。

正式比較前の旧版だけの道具確認で、混在は約1.8秒frameのため初回0.4秒間隔では入力が合流した。短行2一致は検索が第2一致から始まり折返し通知で画像復帰が変わった。初回raw/script/planを保持し、3秒間隔と開始位置を訂正。第二版の混在はgg0が日でありaでないため、lを追加した第三版を別名で確認した。ASCII/短行の第二版成功は再実行せず再利用。元の3文書/ABBA/20回/比較区間は維持し、速度比較の選別や合格までの再測定ではない。継承planに残った旧source/GUI metadataも初回ファイルを残して対象Issueの値へ訂正した。

正式gateの#360のsingle/burst200/burst200-16MiB/single-long-line成功を再利用する案を先行固定する。これらの入力はIMEを作らず、制御置換のない本文と空文書であり、今回変更するtint_runsへ入らない。基点からdraw_body_text/collector/幾何/本文経路/道具/入力/flagsを変えないことを機械照合し、独立レビューで経路を確認する。共通の描画経路を変える追加修正が必要ならこの再利用根拠は失効し、その直接境界を選び直す。単にcommit/工程が違うだけで既存全8benchを回さず、正式gate未実行の候補を実行済みとも記さない。

独立レビュー、根拠付き正式gate再利用、CI、実機の同値と密/疎3条件の利益・代償を合わせて採否判断する。利益を確認できない/画素が違う場合は採用しない。RSS/実確保回数、実Rendererの資源失敗注入、任意装飾/IME/フォント/DPI/全入力は未測として残す。waiver none。

## 採用結果

製品36ebb615を採用。通常Releaseの固定ABBA×3/各120組では、混在1024置換の応答中央値1793170→20619.5µs、対応比中央値0.011477568で全120組短縮。ASCIIは対応比1.001570154で横ばい、短行は601→605.5µsと4.5µs増、68組遅延を残す。全般的な描画改善や無退行とはしない。37 GUI場面と計測中99画像比較は差0、実IMEの本文保持/一覧fallbackも確認。正規Debugの実機sanitizer、通常Release、全72暖機/720計測入力の原marks監査、独立レビューを確認し、正式編集4成功は非到達経路の根拠で#360から再利用した。全コマンド/初回道具失敗/限界/恒久収載は[gate-proofs 5-dk](../quality/gate-proofs.md)を正本とする。
