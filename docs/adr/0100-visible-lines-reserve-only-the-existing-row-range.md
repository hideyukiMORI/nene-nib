# ADR 0100 — 表示行一覧は既存の生成範囲ぶんを一度予約する

- 状態: 提案・固定比較前
- 日付: 2026-10-10
- Issue: #367
- 規則: ARC-001/002/003/004/007/008/011、CPP-003/004/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014

## 決定

1. EditorController::visible_linesの既存first/last計算を保ち、`std::vector<LineView> lines;`の直後にだけ、`if (first <= last) { lines.reserve(last - first + 1); }`を挿入する。既存loopの回数とline_view、検索/選択/表示値、move/返却を変えない。
2. TextBufferはnewline_count+1行を持つ。first/lastはそのtotalで上限が付くが、既存の加算を極端なVisibleLines値が通ることを理由に、first>lastの空loopへ新たな巨大予約を導入しない。既存rangeを変えず非空の場合だけ予約する。予約が要求する要素数を述べるもので、実capacity/RSS/確保回数の実測とはしない。
3. 新cache/state/API、別frame経路、allocator、flag、基準、許容、waiverを作らない。確保失敗のタイミング同値は保証せず、通常の妥当な有限入力を対象とする。C8のpattern所有や#364の画面外clipを混ぜない。

## 固定した閉じた比較

既存ADR0082のprobe dispatch/TimingPort/Win32TimingAdapter/compare-probesだけへ4workloadを末尾追加する。beforeとafterは完全に同じharnessを使う。beforeの製品はmain decd4f1（#362の受理済み内容で旧8252cb1と同一src tree）、#364候補をbeforeにしない。

unitはUTF-8 `a日\t🖋\x01 row`、区切りCRLF、tailはASCII `tail`。emptyは空bytes、shortはunit+CRLF+tail、30と120は(unit+CRLF)×119+tailで全120行。同一ordinary編集状態・caret1行1列・検索/選択/IMEなし。viewportと期待行数を次に固定する。

| workload | viewport指定 | total | frame行数 |
| --- | ---: | ---: | ---: |
| frame-rows-empty-64 | 0（既存acceptで1へ正規化） | 1 | 1 |
| frame-rows-short-64 | 30 | 2 | 2 |
| frame-rows-30-64 | 30 | 120 | 30 |
| frame-rows-120-64 | 120 | 120 | 120 |

1sampleは同じcontrollerのconst frame()を64回呼び、各返却値をその反復内で破棄する。そのループと、lines.size/first_visible/total_lines/caret.line/caret.columnの和の加算を区間に含む。準備・入力生成・固定期待値との全行照合は区間外。区間前後で全LineViewのnumber/text/display.text/display.starts/selection/matches/current_matchとglobal caret/first/total/mode/compositionを照合する。unitの表示は`a日\t🖋^A row`、startsは`0,1,2,3,4,6,7,8,9,10`、tailは0〜4、emptyは0。製品のdisplay_lineを呼んで期待値を作らない。入力bytes/FNV/SHA/build commit/flagsと結果checksumも既存比較器で照合する。

20iterations、ABBA×3、各120対応組を一度だけ。全試料/原marks/失敗を保存し、0usの補正、外れ値除外、試行数の事後変更、成功までの再試行をしない。測定単位は64回分の生成・破棄と固定checksumであり、UI応答/1frame純時間ではない。

30行と120行の双方が、対応比中央値<1、各ABBA cycleの対応比中央値<1、120組中90組以上短縮という条件を満たす場合だけ利益の候補とする。空/2行の費用を併記し、正式製品への費用・同値・レビューと合わせて採否を決める。固定条件未達なら製品を戻して記録する。

2026-10-10再開時・新probe取得前の追加固定: 空/2行も各120組を要求し、それぞれ対応差中央値(after−before)≤50µsかつ対応比中央値≤1.10を費用の上限とする。1sampleは64回生成/破棄なので1frameやUIの許容ではなく、今回だけの局所費用の採否条件。QLT-014の基準/許容は変更しない。

## dispatchの未決を解く設計（2026-10-10再開時・実装前）

旧ProbeWorkloadの53caseを列挙するchecked_indexは通常整形で60行となり、新4caseの直接追加はCPP-012に収まらない。ADR0082の「実装前に相談」に対する現在の設計判断として、既存53enum/各本体/引数/入力/区間/順/名前は保ち、多行生成4条件だけFrameRowsWorkloadという別の閉じたenumにする。数値・文字列・fake countへの置換、switchのdefault、抑制、閾値変更はしない。

- 外部の選択値ProbeSelectionは`variant<ProbeWorkload, FrameRowsWorkload>`。名前から選ぶ入口は従来どおり登録表一つを走査する。製品API/CLI/JSON schema/測定器は変えない。
- 登録表は57行の一つだけ。各行は`ProbeDispatchRow<型>`（その型の値、名前、同じ型を受けるinput factory、runner）をvariantへ保持する。型付き行により別familyのenumと関数pointerの混在はコンパイル不可。旧53行の4要素は変えず、新4行だけ末尾へ追加する。
- 旧checked_index(ProbeWorkload)の網羅switchは53caseのまま維持し、新checked_index(FrameRowsWorkload)は4caseを明記する。新enumの表indexは旧53行の後ろ。選択値へのvisitorはこの二つの型付きoverloadへだけ転送し、別名selection_indexに置く。ProbeSelectionを受けるchecked_index fallbackを作らず、新variant alternativeはoverload不足でコンパイル拒否する。
- constexprで各行のindex/型対応・名前重複/空・null関数・表sizeを検査する。run/inputは同じ選択indexから同じ登録行を訪問し、その行が所有する型付き値を渡す。測定する本体へ変更を入れず、variant訪問/表の選択は元から計測markの外。
- 固定四条件のDebug/Release correctnessに加え、この構造変更の直接証明として、旧53行の値/旧処理本体一致、両enumのcase網羅、variant alternative追加時の拒否、登録型不一致の拒否を限定した正例/反例で確認する。正規compiler/clang-tidy/formatを使用し、反例はDの専用出力へ置く。全旧53性能を測り直す理由にはしない。

これは新4条件を意味のある型へ分けて同じ表に載せる設計であり、計測器の並行した実装経路を作らない。before/afterへ完全に同じharnessを使う。必要な独立レビューは読取専用でこの型と計測区間の不変性を確認する。

## 直接検証と再利用

通常Debug/tidy/ASan/UBSanで変更sourceをbuildし、既存`--application`契約を一度実行する。frame/scroll/selection/所有を直接確認し、同selectorに含まれるframe-selectionを重複実行しない。新unit selectorやreserve/capacityを写すtestは不要。probe入力registryの追加は既存test_compare_probesの該当境界だけ、実probeの4条件はDebug一sampleとRelease同一exeの短い自己比較で道具の正しさを確認する。速度判断には自己比較を使わない。

通常製品Releaseをbuildする。frame生成は起動/編集/一覧の共通callerなので、以前のtint非到達による正式4成功の再利用根拠は使わない。親は直接sourceの到達境界を示して正式速度条件を選び直す。全gate/無関係検査は行わない。実機の値同値は空・短文書・多行viewport/EOFに限定した共通入力と前後画像で確認し、Renderer/IMEの実装不変に基づき#364の86場面を繰り返さない。

独立レビューで固定harness、正規path、全失敗、再利用根拠と利益/費用を確認する。source/道具/関連依存/環境が不変の成功は工程/SHAだけで再実行しない。全コマンド・成果・未測をgate-proofs/日報/引き継ぎへ残す。Dの追加作業木/出力は採否/必要統合/恒久収載後に監査して削除し、branch/commitを保持する。
