# ADR 0099 — 現在の描画面の完全外側にある着色clipを省く

- 状態: 提案（固定比較前）
- 日付: 2026-10-10
- Issue: #364
- 規則: ARC-001 / ARC-002 / ARC-004 / ARC-007 / ARC-008 / ARC-011 / CPP-003 / CPP-008 / CPP-011 / CPP-012 / CPP-016 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014 / D41

## 文脈

#362は混在文字の着色を短縮したが、ASCII制御文字1024個では製品応答約64msが残った。tint_runsはruns_ofが返した全clipの下で字形を描き直す。完全に画面の外でも同じ処理をする。これが残る費用の主因かは未測であり、性能比較で確かめる。

## 決定

1. tint_runsのループ前で現在のcontext_->GetSize().widthを一度取得する。各runについてrun.left > widthまたはrun.left + run.width < 0の場合だけcontinueする。境界の等値・部分交差は従来どおりPushAxisAlignedClip/既存draw_body_text(NONE)/PopAxisAlignedClipへ進む。
2. runs_ofのHitTest結果はarea.left/topを含むtarget座標である。本文area.rightは字形overhangや入力行の横送りに対してtarget境界ではないため使わない。glyphの範囲を推測せず、後続の全描画を制限するclipそのものがtarget外である場合だけ省く。垂直方向は変更しない。
3. 前提は既存と同じ通常の有限で妥当なDirectWrite metrics、context96dpi・恒等変換、target設定後の同期描画である（ADR0073/0077）。OS窓DPIは別に文字寸法へ適用する。将来contextの変換/単位を変える場合はこの前提も再評価する。NaN/壊れたmetricsや資源失敗を新たに定義せず、全失敗同値は主張しない。
4. 二挿入のみとし、HitTest/brush設定/範囲順/残るclip/字形/fallback/本文保持/失効/IME/callerを保つ。新state/cache/API/試験口/decoder/flags/設定schema/基準/許容/fixture/抑制を増やさない。

Microsoft一次資料（2026-10-10確認）: [GetSize](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-getsize)は現在のtargetのDIP寸法、[PushAxisAlignedClip](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-pushaxisalignedclip%28constd2d1_rect_f_d2d1_antialias_mode%29)は後続全描画を現在transformで変換した矩形へ制限する。この前提から上記の可視画素同値を推論し、実機で確かめる。

## 直接検証と再利用

通常Debug製品を正規tidy/ASan/UBSanでbuildし、変更sourceのformat/conformanceを確認する。原文の厳密な二挿入以外とsrc/tests/eng/CMake/flagsの不変を機械照合する。既存collector試験は変更されず、その成功を再利用する。

#362通常Releaseの成功した本文27＋本文IME4の31場面は同一文書bytes/設定/字体/テーマ/DPI/画面寸法/刺激/道具/比較領域を照合し、旧版画像を再利用する。新しい作業folderに伴う文書絶対pathの差は明示し、本文/statusへ出ないことをsourceとbasename一致から確認する。旧metadataを書き換えない。一覧IMEの旧empty場面には候補locationの絶対folderが表示されるため、その6場面は新共通pathで前後を採る。変更後通常ReleaseとDebugは実Rendererで制御置換/BMP/補助平面/結合文字/Tab/双方向文字/選択/検索/ブロック、本文IMEと横溢れする一覧IMEを確認する。旧条件が一致しない場面は再利用しない。

新しい直接境界として長いASCII制御置換行を640〜688の49幅へ1px刻みにresizeする固定sweepを通常Release前後とDebugで採り、実capture/client寸法を記録する。最終列への着色画素の到達と非到達を調べ、右端の部分clip/可視とtarget再作成を画素比較する。画面の完全外側の置換も同じ長い行に含むが、内部HitTestの厳密な端点等値や完全左外側の実到達を画像から測定済みとはしない。左側の横overflowは既存の実IME一覧とsourceの経路を確認し、完全左外側への到達を保証できなければ未測として残す。IME open statusを戻し、文書/設定不変と正常終了を記録する。

性能は#362採用済み通常Release9218124と新版通常Release。同じASCII1024/混在1024/短行2、検索/a<CR>、先頭aへのgg0/gg0l/gg0、暖機nN、計測20回n/N、ABBA×3/各120組を固定。前版は混在も短縮済みのため全ケース0.4秒間隔にし、#362の3秒条件の数字と直接比較しない。暖機と計測の各frameが次inputまでに来ること、全試料・画像往復・失敗を保存する。比較は一回で、成功までの再試行や試料選別をしない。

正式#360のsingle/burst200/burst200-16MiB/single-long-line成功はtint非到達と共有経路/道具/入力/flags不変を機械照合し、独立レビューして再利用する。この前提が失効した場合だけ直接境界を選び直す。全benchの再実行とは記さない。

独立レビュー/CI/実機同値/固定密疎比較の利益と代償を合わせて採否判断。利益なしや画素差があれば採用しない。任意文字/字体/DPI/IME、資源失敗、RSS/実確保回数は未測。waiver none。

## 2026-10-10 測定前の失敗と実験の継続

通常Releaseの85場面は差0だが、一覧IMEの03-other-compositionは6画素が一channel一段異なり、元の全画素比較は失敗した。本文入力のother色83、Space/commit/IME状態復帰は一致し、同sourceの通常Debug画像は旧版と完全同一だった。成功までの撮り直し、元失敗/画像の置換、閾値緩和、maskは行わない。

差の6点は入力行の下で、画面上のOS候補窓背景にある。原因を調べる固定一回の診断で、6点全部のrootがNeNeNib以外のApplicationFrameWindow/explorer.exeであることを実測した。診断時のbitmapと当初の失敗bitmapはSHAを含め完全同一。外部overlay領域の差という判断を独立レビューでも支持した。具体的なOS側の差の生成原因、旧exeでの揺れ、隠れた背後の全画素は未実証であり、全86場面完全一致とはしない。

この外部画素の問題を#365へ分離し、QLT-012に基づき性能は独立した固定実験として続ける。元の全画素条件の達成状態はfalseのままidentityへ固定し、速度の成功でGUI合格を代用しない。採否時に外部windowの所有境界と当初条件との整合を明示して判断する。正式gate/共通道具/製品/基準/除外/許容/waiverは変更しない。
