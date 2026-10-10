# ADR 0104 — 一時の文書表示値はframeへ所有を移す

- 状態: proposed（取得前の固定計画）
- 日付: 2026-10-10
- Issue: #380
- 基準: main6195a1547db60ee4aa0f96b5d7c84957dc559398

## 問題と決定案

EditorController::frameはdelivery()から作った所有DocumentViewをtab_viewsへ読み取らせた後、EditorFrame.documentへlvalueコピーする。DisplayTextのtitleとoptional<FilePath>の文字列が余分に複製される。

tab_viewsの既存順を保ち、その後でdocument.encodingのenumだけをconst localへ退避する。集約のdocumentはstd::move(delivered.document)、status_items_forのencodingは退避値を使い、移動後objectを読まない。共通値の生成はADR0084のdelivery一か所、stateから第二経路で作らない。製品はEditorController.cppのこの変換だけ。

## 規則・範囲

ARC-001/004/005/007/008/011、CPP-002/004/006/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0010/0056/0059/0075/0082/0084。waiver none。公開API/保存schema/鍵/設定/基準/許容/所有寿命/検索や本文のalgorithmは変えない。

## 取得前の4固定条件

FrameDocumentWorkloadの閉enum4条件を旧72条件の後へ同じtyped表/TimingPortで追加する。input metadataはpath UTF-8 bytesで、本文は以下の別固定literalである。名前とinput/FNVは次のとおり。

| 名前 | path bytes | FNV1a64 |
| --- | ---: | --- |
| frame-document-short-saved-256 | 31 | 3165114504819470625 |
| frame-document-long-saved-256 | 241 | 3259355916674798828 |
| frame-document-long-failed-256 | 241 | 3259355916674798828 |
| frame-document-untitled-256 | 0 | 14695981039346656037 |

- short-saved: `C:\nib-probe\frame-document.txt`。UTF-8/LF、本文 `a日\t🖋\x01\nend` 14bytes、title `frame-document.txt` 18bytes、保存済み。
- long-saved: `C:\nib-probe\` + `segment\` ×20 + `a` ×64 + `.txt`。UTF-8 BOM/CRLFを読む。本文は前項LFをCRLFへ替えた15bytes、物理readはBOM込み18bytes。titleはa×64+.txtの68bytes、保存済み。
- long-failed: 同じ長path/本文を開き先頭へxを一回挿入、caretを1/1へ戻してから、同path/UTF-8 BOMのSaveDocumentをScriptedFilesのaccess_deniedで拒否する。本文16bytes、titleは`● `付き72bytes、modified/last_failure=access_deniedを保持。
- untitled: 空のpathなし/本文0bytes/1行/UTF-8/CRLF/保存済み文書、title `無題` 6bytes。

全条件ordinary、viewport2、caret1/1 bar、検索/選択/IME/commandなし、tab1/active0。例外としてfailedのfile failure表示は保持する。区間外で全文（2行または空1行）/display/map/selection/matches/current、document title/path/encoding/save/failure、status3項目、tabs/delivery/documentsの同じ所有値を独立literalへ照合する。製品のtab_title_for/encoding_label/line変換を期待値生成に使わない。

区間はconst frameの生成・破棄256回とchecksumのみ。checksumは各frameのtitle bytes+path bytes（なし0）+statusのencoding bytes+tabs.size+lines.sizeの和、short14592/long82176/failed83200/untitled3328。準備・前後oracle・入力生成は区間外。同一harnessをbefore/afterへ使う。

20iterations・ABBA3・各120対応組を一度だけ取得する。short/long savedの2利益は対応比中央値<1、全cycle比<1、短縮>=90/120。failed/untitledの2費用は対応比<=1.10かつ対応差中央値<=50µs/256frames。条件未達は不採用、外乱/欠測は保留、除外・穴埋め・通るまで再取得しない。全rawと失敗を保存する。局所値を1回単独やUI全体の改善率としない。

## 変更に関係する検証

新登録Python対象2と既存末尾7登録、旧72入力/関数/selector/区間不変証明、新enum case欠落の実compiler負例。正規Release前後/Debug+tidy+ASan/UBSan、新4probe各1iteration。--applicationはdelivery/frame共通のdocument・encoding/save/失敗/保有snapshotとframe直接callerを、--tabsは帯への先行copyと切替を確認する。新unitは既存契約とprobeの独立oracleを重複して増やさない。製品差分/source/実依存/symbol、protected-diff、通常Releaseを確認する。

GUIはshort/longのsaved・modified・保存復帰とuntitled/tabの範囲に限定し、台本/固定本文/実mode/前後exe hashを操作前に固定して全clientと本文を比較、全画像を目視する。正式速度はkey-to-frame-singleだけを一度5試料取得。変更はbody量/検索によらない文書表示値の移管で、普通入力のframeを代表callerとする。他7指標は今回未測と明記し、前回成功を今回実測へ置換しない。実機計測中はbuild/test/他操作を重ねない。

## 採否・整理

単体実装、必要な独立レビューだけ読取専用。D:/NeNeNib/worktrees/380-frame-document、outputs/20261010-frame-documentで作業する。全実験/失敗/判断を日報/引き継ぎへ記録する。PR/必須CI/main統合/恒久収載後、未保存/ignored/唯一成果/稼働参照/link/絶対pathを監査整理、branch/commitを保持する。関連入力が不変の成功結果は工程/文書/SHA変更だけで再実行しない。

永続cache、tab表示API、本文/検索、C8入力parse保持、#365/#373、他候補の再測定、全件回帰は対象外。
