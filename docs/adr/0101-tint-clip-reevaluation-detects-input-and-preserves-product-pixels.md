# ADR 0101 — 画面外clipの再評価では入力混入と製品全画素を先に検証する

- 状態: 保留・固定実験終了（撮影不成立と入力外乱、製品採用なし）
- 日付: 2026-10-10
- Issue: #372（旧実験 #364、撮影問題 #365、再開指示 #370）
- 規則: ARC-001 / ARC-004 / ARC-007 / ARC-008 / ARC-011 / ARC-012 / CPP-003 / CPP-008 / CPP-012 / CPP-016 / QLT-001 / QLT-007 / QLT-010 / QLT-012 / QLT-013 / QLT-014 / D41

## 根拠と正典

旧#364はASCII旧版08-Aで予定外の先頭oと未保存印、終了待ちTimeout、marks欠落を残した。hideの入力混入申告と整合するが、入力元は未記録で断定しない。一覧IMEの6画素差は別試行・別問題である。旧ADR0099・不採用・全画像・全試料は書き換えず、新実験だけを本ADRで判断する。ADR0100は#367で使用中。

製品案はADR0099と同じtint_runsのtarget幅取得とstrict左右完全外側continueの二挿入だけ。beforeは受理済み#362の9218124、afterは旧候補0c91c99。保存済みexe/ソース/実flags/build logの指紋と現在mainの関連source/test/engを照合して再利用する。別修正・#367・new cache/API/flagsを混ぜない。二挿入を再現する全文照合で同一性を証明する。通常finite metrics・context96dpi/恒等変換の前提と、資源失敗/NaN/任意字体等の未測は維持する。

## 入力と本文状態の検査

1. `eng/window_driver.py`と製品`--measure`を共用する。製品に計測専用の口を足さない。開始後・暖機前・終了前に、窓titleが期待した保存済みbasenameで未保存印なし、foreground/寸法/DPI/IME NORMAL状態が同じことを記録する。
2. 各checkpointで既存Exの`:w <新しい検査用path>`による名前付き文書の別名コピーをDに保存し、全bytesを固定入力と照合する。保存前にも未保存印を検査する。`SaveIdentity::retain_document`により原文書の名前/未保存状態は変えない。初回は検索前、暖機前は検索/anchor準備後、最後は計測後の画像取得後。書込み後1.5秒待つ。暖機前のコピー後は同じanchorを再送して通知とcaretを揃える。コピー・検査・撮影・再anchor・待機は計測値に含めないが、暖機状態への影響は両版で同じになる新しい実験条件として明記する。
3. OSの`GetLastInputInfo`を窓操作開始前とcheckpointで読む。性能台本のclick/鍵はPostMessageだけなので、この期間のtickの変化は外部入力としてそのtrialを無効にする。値は単調増加を仮定せず等値だけを比較する。別sessionの入力、同tickを意図した偽装を網羅するセキュリティ監査ではない。補助として全予定送信数と原input marks数、正常終了、全body copy、title、画像の一致を独立に確認する。
4. 各phaseのpost鍵数、Returnに対応するKEYDOWN/CHAR二markを記録し、準備・暖機・測定・終了後検査の区間を明示する。各暖機2入力と計測20入力は、次のinputより前にframeがあることを原marksで要求する。0us補正、outlier除外、不足補完、後続frameの流用はしない。
   さらに窓作成前の`timing.bind`を確認し、`driver.start`直後のQPCを製品originの上界として保存する。warm n撮影・暖機後検査・計測後検査の各開始QPCより、`origin上界 + ceil((frame相対µs+1)×frequency/1e6)`が小さいことを要求する。µs切捨てを保守的に包み、描画と観測が重ならないと証明できない試行は欠測にする。
5. 旧版の正しいsmoke ready画像と新readyの本文/statusを照合し、ASCII/短行は1行1桁のa、混在は1行2桁のaが開始点であることを目視でも確認する。warm nで変化しnN往復で戻ることと、前後全trialの同じcheckpoint画像が完全一致することを確認する。新しい共通D絶対pathは本文/statusへ表示しない。名前と保存済み状態をtitleで別に検査する。
6. trial中に混入/dirty/遮蔽を検出したら計測を有効値にしない。元record/画像/既存marks/台本を残し、未保存確認を無視して正常終了扱いにしない。想定外の未保存内容が残る窓は即座に破棄せず、その状態を保全して系列を停止する。既知の合成入力だけのnegative proofは専用profileで清掃できるが、その終了は正常trialへ数えない。

変更した検査器のcorrectnessとして、旧exeでASCIIの短いnNを一度実行し、別の専用runで起動後の普通モードへoを故意にpostしたときdirty/body検査が拒否することを一度確認する。LastInput判定と予定mark/frame対応には固定recordの正例/反例を与える。smokeは性能利益の証拠にせず、初回失敗も保存する。道具の欠陥を直す場合は原版と失敗を保存し、訂正版の意味と再実行範囲を先に記録する。正式系列開始後はharnessを変えない。

## 外部IME候補窓を含む撮影条件

computer-useのWGC取得はnative pipe unavailableで使用不能だった。新規capture実装を足さず、既存`window_driver.capture`の画面合成を使う。次の方法を取得前に独立レビューし、対象exeの性能比較へ撮影操作を混ぜない。

- 一覧の共通入力は旧#364と同じ`日🖋 text\r\n`、`BEGIN_`＋x160＋`_END`、Cascadia Code 13.5pt、system/guide on、1280×800、位置(80,80)、実日本語IMEのnihongo→Space→Enter→Esc。01空/02横溢れ/03other/04target/05確定/06閉じた、の6場面を保存する。
- before A1、同じexe対照A2、after Bの順に、同じ新共通文書pathとprofileの初期値で各一度行う。完全画素の条件はclientの本文/status全域（従来と同じtitle帯を除く矩形）で、隠れた箇所を切り抜くmaskは置かない。title・保存済み状態は別途検査する。
- 各場面で元の合成画像を必ず保存する。03/04で対象窓の上に重なる可視root窓を記録し、class/PID/実行物/矩形/targetとの関係を確認する。移動できるのは実入力後に出現した、旧#365と同じApplicationFrameWindow・explorer.exe所有の候補窓一つだけ。他の窓や不明な所有ならその条件は保留。
- 候補窓を`SetWindowPos(SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER)`で製品矩形の右側へ一時移し、十分な画面内余白があり全矩形が非交差であることを確認する。製品窓は移動/resize/再作成しない。IMEをcancel/commitせず、foreground/IME open/title/client寸法/入力欄の未遮蔽画素が同一であることを確認する。移した後にも遮蔽が無いことを前後二度検査し、全clientを撮る。終了時に元の矩形へ復元し、復元結果を記録する。失敗してもfinallyで復元を試みる。
- 取得前後にroot窓を辿り、製品より上にある可視窓の矩形とclientが交差しないことを確かめる。root/窓の消失・順序変化・不明な透明窓があれば合格へ救済せず保留。DWMの影が製品に掛からないよう候補窓との余白を64px以上確保する。合成の原画像と移動後の全画像、幾何/所有/復元記録を保存する。
- A1/A2の全製品画素が一致し、移動前に見えていた入力欄の画素が不変、Bの全製品画素も一致して初めてこの境界を同値とする。OS候補の原bitmap自体の差は別に報告し、その差を製品差と断定しない。隠れた製品画素を取得できない、移動がcompositionを変える、所有や遮蔽を証明できない場合は保留。結果後の範囲/閾値変更は禁止。
- IME色/target色と場面到達は保存したproduct画像から検査し、復元後の別captureで代用しない。想定外の未保存状態・確認dialogでは性能trialと同じく窓とPIDを保全して系列を停止する。

右端clipの旧49幅・本文/本文IME31場面・一覧の成功5場面は同一source/exe/test/依存/必要環境を照合して成功を再利用する。新取得は撮影問題の6場面×A1/A2/Bと、入力監視を追加した性能trial内画像に限定する。旧86場面の全再実行やDebugの工程理由の再buildを行わない。

Microsoft一次資料: [GetLastInputInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlastinputinfo)、[SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)、[GetWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow)。tick比較・候補窓の位置だけを変えるフラグと所有/幾何の事後検査はこれらの契約から設計した。画素不変は契約だけで証明せず上記の実対照で検証する。

## 固定した性能条件と処分

入力は`a\x01 `×1024＋CRLF、`日a🖋\u200b `×1024＋CRLF、`a\x01 `×2＋CRLF。検索`/a<CR>`、anchor `gg0`/`gg0l`/`gg0`、暖機nN、計測nN×10、間隔0.4秒。条件順はASCII/混在/短行、各ABBA×3（12trial）、120対応組。対応はcycle内A1対B1、A2対B2の同ordinal。同じ前後製品・共通新台本で全36trialを一系列として一度だけ実行する。計測前後にexe/道具/入力/環境指紋を保存し、背景build・GUI・重いhash/copyを並行しない。起動・準備・コピー・検査の時間は製品input→frame値へ含めない。

採用候補とする必要条件を次に固定する。

1. 3条件全部で有効な120組があり、本文bytes/画素同値/入力対応/正常終了が成立する。上記IME全製品画素と直接境界も成立する。
2. ASCII1024と混在1024のそれぞれで、対応比中央値<1、3cycleそれぞれの対応比中央値<1、120組中90組以上が短縮する。両方を満たさなければ今回の製品案は利益不足として不採用。
3. 短行2は、対応差(after−before)中央値≤50µsかつ対応比中央値≤1.10を許容する。これは独立実験の微小な代償の上限で、QLT-014の基準値/許容を変えるものではない。費用が0とはせず全値・遅い組を報告する。上限超過は不採用。
4. #360正式編集4成功は、tintへの非到達と共通実装/道具/入力/flags/基準・実機環境の不変を再確認して再利用する。受理済みsourceとの同値も別に示す。実機確認・独立レビュー・通常PR/必須CIを経て統合する。

有効な製品差・利益不足・費用超過は不採用。外乱/欠測/撮影不成立/原因不明は保留であり製品不良・改善なしとは断定しない。無効trialだけ差し替えない。旧約7msと新値は別系列として報告する。どの処分でも全失敗と原記録を残し、依存しない#367へ進む。未解決は#365または具体的な別Issueへ残す。採用時は#367の基準製品を揃える。

## 保存と限界

全raw/台本/指紋/レビュー/失敗/採否はD専用出力へ、要約は日報・引き継ぎ・current、詳細はgate-proofsへ残す。main統合/必要反映/恒久収載の後、絶対path・未保存/未追跡/ignored・唯一成果・稼働参照・link・取込を監査し作業木と不要出力を整理する。branch/commit/恒久証拠は保持する。waiver none。

通常有限metricsの固定実機条件だけを検証する。任意のIME候補・外部窓・フォント/DPI、資源失敗、RSS/確保数、光るまでの遅延は未測。旧実験の失敗は新結果でも覆らない。

## 固定実験の結果（2026-10-10 19:03 JST）

短smoke・dirty負例・固定record3testsは成立。IME A1では03の候補面がproduct画像に残り、窓列挙は空だった。撮影器内passedを採用根拠から除き、04target未到達の初回失敗も保持した。A2/B・候補移動/復元は未実行。今回旧exeの03画像は前回候補版と同一で、前回旧版から同じ6pxが変化した。製品差の帰属はできないが、全製品画素は取得できていない。

固定性能系列は最初のASCII00-Aでsession入力tick変化を検出し停止。有効trial/対応組0、残り35trial未取得、最終copy/marks/正常終了なし。入力元は未知。条件緩和・同系列再試行・欠測差替えをしない。利益/製品同値の必要条件が判定不能なため保留とし、製品は受理済み#362を維持する。旧#364の不採用は不変。詳細はgate-proofs 5-dm、未解決#365/#373。依存しない#367へ進む。
