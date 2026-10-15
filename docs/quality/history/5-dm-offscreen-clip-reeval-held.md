# 5-dm — 画面外clipの独立再評価は撮影不成立と入力外乱で保留（Issue #372・ADR0101）

出典: docs/quality/gate-proofs.md の節 5-dm（2026-10-16 に D50 で移動・本文は移動時のまま）

hideの再開指示により旧#364を別系列で再評価した。設計commit685ab5d/df7c278、製品はbefore9218124/after0c91c99の保存済み通常Release、二挿入だけであることを全文照合。main25ff09cはbeforeのsrc/tests/eng/flagsと同一。今回のrepository変更はADR・報告・引き継ぎ等の文書だけで、候補の製品コードを新作業木へ適用していない。処分は保留であり、製品不良や利益不足とは判断しない。旧ADR0099/失敗/不採用は不変。

規則QLT-001/007/010/012/013/014、ARC-001/004/007/008/011/012、CNF-006、GIT-001〜004、D41。schema/API/基準/許容/mask/抑制変更なし、waiver none。以下OUTは実験時D:/NeNeNib/outputs/20261010-tint-reevaluation、恒久先はD:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-372/files/outputs/20261010-tint-reevaluation。後続文書/CI/main/監査整理はacceptance-372。元rawのpathを改変しない。

### 選んだ検証・再利用と結果

| コマンド / 証拠 | 直接の退行・判定対象 | 初回結果 |
| --- | --- | --- |
| `python OUT/provenance.py` / provenance.json | 保存済みexe/正規build実flags/source、二挿入、mainとbefore、旧証拠の同一性 | exit0。before SHA ce0277729b358d2d66f2dda65d592b62e3a159eecb7dbe3e9b85a744d0a88f77、after209a38fb976946745859d0b6854d8a11bf73dbc472d439a68a5316c548e8550d、Debug a741d8b653ce787a7be25d76ed499223d3e1e51bce746c370e894aed4cf954a1 |
| `python OUT/test-trial.py` | session入力/dirty/foreground/IME/予定marks/次input前frameの拒否、QPC観測境界 | 初稿2tests成功。最終inputを後検査frameで救済する反例追加で2tests成功。QPC上界反例追加で3tests成功。変更した検査入力にだけ再実行 |
| `python OUT/run-harness-proof.py smoke` | 既存Exコピー/全本文/ready画素/予定mark/観測前frame完了の実到達 | exit0。旧exe ASCII、body3コピー一致、ready旧画像差0、warm変化/final復帰、正常終了。67.664/68.346msはcorrectness用で利益へ算入しない |
| `python OUT/run-harness-proof.py dirty` | 旧失敗と同じ予定外oを測定前に拒否すること | exit0。ordinaryへ合成o、dirtyをcopy/測定前に拒否。failed原record/PNGを保持し、既知合成入力の専用窓だけ停止 |
| `pwsh -NoProfile -File OUT/environment.ps1 -Label before/after` | hardware/電源/競合処理・必要環境 | 各exit0、i9-10850K/RTX3090/HP推奨、同OS起動/driver、競合対象なし。snapshotであり連続監視ではない。各GUIはDPI120/1280×800/Cascadia Code13.5/system/guide on |
| `python OUT/ime-comparison.py --label A1 --executable <before>` | 候補背後を含む全製品画素、保存画像上のIME到達 | exit1、04target到達不成立。03の候補窓を列挙できず全画素取得も不成立。A2/B未実行 |
| `python OUT/run-series.py freeze` / plan.json | 全6script/製品/ABBA順/全条件の固定 | exit0、plan SHA b8225e401c53b7bece90c0c3ff96181de28835e3477f3e66610273f879dffb69 |
| `python OUT/run-series.py run` | ASCII/混在/短行、20nN入力、ABBA×3各120組 | exit1、ASCII00-Aでsession入力変化を検出。1attempt/有効0、残35未取得、全条件有効対応0。改善率算出なし |
| `python OUT/audit-held.py` / held-audit.json | 凍結指紋と製品不変・原失敗保持・保留判断 | exit0/auditStatus complete、productAccepted false。採用の成功判定ではない |

全本文の:w別pathコピーはretain_documentを使い、原文書名/未保存状態を変更しない。計測入力はPostMessage、ReturnのKEYDOWN/CHAR2markも全数へ含める。製品bindはwindow作成前なのでstart直後QPCをorigin上界とし、frame相対µsの切捨てを+1とceilで包んで3観測開始より厳密に前と要求した。独立レビューの監視重なり・dirty窓破棄・別captureからのIME判定の3阻害を修正、修正前scriptはinspector-before-reviewに保存。修正後静的阻害なしだったが、実機で撮影網羅性が否定され評価を更新した。

正式4編集条件の#360成功と旧GUI成功は同じexe/source/tests/tools/flags/環境の記録を照合。tint非到達・他の描画経路不変という従前の再利用根拠も保存したが、今回新しい必須条件を満たした代用にはしない。新build/全unit/全gate/正式8条件/旧86場面は再実行していない。最後は文書だけの差分なので製品テストを追加しない。

### IMEの誤判定と新しい6画素の根拠

03-productには候補面が明瞭に写るのにaboveBefore/aboveForCapture/aboveAfterCaptureは[]、moves=[]。空配列のall検査と既存5点のassert_uncoveredでは候補を検出しなかった。raw/product同じbitmapのinput差0も全製品画素の露出証明にはならない。原capture status passedを書き換えず、held-auditでvisual evidenceにより無効と明記する。候補移動/復元は行われていない。窓chain外/visible・cloakフィルタ/合成関係のどれが原因かはフィルタ前記録がなく未特定。04はIME色83→0・accent42→27、PNGからも日本語composition/候補が消えておりtarget未到達。製品差ではない。#365に残す。

今回旧exeの03 PNGはSHA08a7b0d94e7b920127aaecc13313fbd8804fbef1e9bbee8b92d03a08dbeb12b2で旧候補版03と完全同一。旧exeの前回03（SHA32c9aa2b0e75410174e5656f257630d70513a3dc06d45e9fe7c1a046a6096cc0）との差は既知の同6座標だけ。**同じ旧exeでも6画素差が出た**ことが新しく分かった。入力欄と製品の着色コードへ差を帰属しない。OS内部の生成原因は未確定であり、この証拠でも隠れた製品画素の同値を救済しない。

### 性能系列の中止と未測

ASCII00-Aは開始〜暖機後のLastInput tick36908687が測定後36953703へ変化。title保存済み、foreground true、IME0、DPI/寸法は維持。入力元は未取得なのでhide/他process/OS/マウス/鍵のどれかは断定しない。最終copy/正常終了/marks flushへ進まなかったためraw marksなし。保存済み状態のowned processだけ終了、unexpected dirty窓を捨てたものではない。before2全本文copy/ready/warm/failure画像/予定238mark台本/全checkpointは保存。

条件は緩めず無効trialを差し替えず、欠測を旧試料で埋めない。残る35trialとIME A2/Bは未取得。今回の性能利益・短行費用・全製品同値・候補移動/復元は未測。入力監視の未知は#373へ分離し、依存しない#367へ進む。#367の7未保存ファイルは再開時に停止snapshotと一致、preserved-367へ追加保全済み。製品採用なしなので基準を#362のまま保つ。

全実験/初稿/ツール読取失敗/差分レビュー/保留判断はjournalとreview-planに保存。computer-use native pipe unavailable2回は入力なし、別のUI実行成功とは数えない。rawの推測path/glob失敗は実在pathの再発見で訂正、正式比較の失敗を再試行してはいない。恒久収載/取込・未保存・ignored・唯一成果・稼働参照・link・D絶対pathの監査後に#372の作業木と不要OUTだけ整理し、branch/commit/原証拠と未完了#367を保持する。
