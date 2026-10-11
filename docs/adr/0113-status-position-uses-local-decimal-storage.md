# ADR 0113 — ステータス位置の数値は局所領域で組み立てる

- 状態: 技術受理（2026-10-11 17:16 JST、統合・収載・整理は別記録）
- 日付: 2026-10-11
- Issue: #402
- 基準: main b13a04b41e708fb198a6b61bdbf84a31f2499278

## 問題と正規経路

StatusItems.cppのcaret_positionはto_stringの所有値を連結し、fixed→DisplayText::parseで最終所有する。ADR0105が対象外とした数値側の中間所有を除く。整数変換を自作せずstd::to_charsの符号なし十進変換を用い、最終のUTF-8/control/256byte/コードポイント検証と所有は既存DisplayText::parseの一経路を保つ。

製品変更はStatusItems.cpp内のprivate caret_position・局所decimal helper・必要includeとstatus_items_forの一呼出しに限定する。caret_positionがDisplayTextを返し、status_items_forはそれと既存encoding/endingのfixed結果を同じ3項目arrayに並べる。公開型/API/入力値/文字列/表示順/label関数/formatter/validatorは変えない。stack viewを返さず、mutable/static cacheと新factoryは置かない。

## 容量・失敗・規範

局所配列はprefix「行 」4bytes、separator「, 桁 」6bytes、2*(numeric_limits<size_t>::digits10+1)で容量を導く。Windows x64では50bytes、最大20桁×2をNULなしで含む。各to_charsの範囲を最大桁数内へ限定し、ec==std::errc{}を確認したptrだけを後段へ渡す。literal copyはこの容量証明の範囲内。終端は書き込まれたptrとの差で求め、未使用byteをparseへ渡さない。0も現公開wrapperが保持できるので拒否しない。

最大桁数を確保したunsigned十進変換の容量不足はプログラム不変条件の破れ。private decimal_endはstd::expected<char *, std::errc>を返し、ec失敗時はstd::unexpected(result.ec)、成功時だけresult.ptrを返す。呼出側は容量を証明した固定extentでのみ使い、既存fixedのparseと同じvalue()で不変条件を確認する。標準のchecked accessによって失敗を表し、明示throw・未定義動作・部分出力・黙った代替書式・結果型の公開追加をしない（CPP-005）。期待される入力失敗を無視する経路ではない。原則到達不能でもec分岐を消さず、Debug ASan/UBSanと最大/0/桁境界を検証する。既存symbol gateを緩めず、新しい外部依存を求めるなら候補を止めて設計へ戻る。

CPP-014のstd::format一般文には、受理済ADR0022のcore/application locale拒否が優先する。今回の位置表示はこのADRのto_chars→既存DisplayTextという一経路へ置換し、旧caret_positionのto_string経路を同時に除く。CPP-014にその狭い関係を明記する。他の既存数値callerを一括移行せず、formatのcore導入・allowlist/抑制/警告/機械強制状態/閾値変更はしない。plannedはplannedのまま。これは新gateで通らない実装を許す変更ではない。

実MSVC14.44.35207の整数to_stringも局所配列を使う。利益仮説は中間string所有と連結の除去に限り、locale改善やallocation回数を実測したとはしない。SSO15byteは固定SDK xstringの根拠で、他STLへの一般化なし。割当失敗の時機は同一と主張しない。

## 製品前に固定する計測

既存6StatusItemsWorkloadとframe-document-short-saved-256の全source/登録/入力/oracle/timing区間を不変で再利用する。新probe/enum/runnerは追加しない。固定JSONはD:/NeNeNib/outputs/20261011-status-number/fixed-inputs.json、SHA256 b657467f8fb57db028d24e23972b637d66cc47c372007bf847e7fd7056145a7c。

利益3はstatus-items-sjis-crlf-1024(123/456)、status-items-max-utf8-lf-1024(max/max)、status-items-large-bom-crlf-1024(123456/654321)。費用4は残りutf8-crlf/bom-lf/sjis-lfの3短値とframe-document-short-saved-256。6pureの1024回生成・破棄とbytes+codepoints checksum、callerの256framesを混同せず、UI全体率へ換算しない。期待値は既存独立literalと親scalar metadataを照合する。

同一harness前後、20iterations/warmup1/ABBA3/各120対応組を初回1度取得。利益は対応比中央値<1、全3cycle比<1、短縮>=90/120。費用は対応比<=1.10かつ差中央値<=50us/各固定区間。1条件でも未達は不採用、欠測・外乱は保留、試料除外/穴埋め/合格までの再取得なし。全7条件の全rawと費用を保存する。

## 関連検証

既存--status-itemsの8assertions/既定verify_look入口/47selectorを変えず、0/0、10/100、99/999、100/999のliteral/text bytes/codepointsと先行返却値・viewの保持を追加する。新境界テストを別harness commitに先行し製品前後で共有する。入力は15/16bytesの対象STL境界をまたぐ。期待値を製品helperで作らない。既存148probe/1853fixtures/他selector/assertionは保持する。

before Release、製品通常Release、製品Debug ASan/UBSan/no-recoverで既存7probeを各1iteration、Debug --status-itemsだけを実行。全encoding/ending・max・所有保持は6pure oracle、frameの全fieldsはcaller oracleが直接境界を担うため無関係なunit scope/全件回帰は実行しない。変更C++のformat/tidy/規約、実CMake graph/core/application symbols、protected-diffを確認。通常Releaseはeng/build-release.ps1の40桁Ref/cleanから作る。構成別flagsは原captureへ保持し同一と主張しない。

GUIは既存driverと私有profile、同一bodyの6encoding×ending場面で1/1、9/24、10/100、99/999、100/999、123/456を事前固定する。本文は124行×1000codepoints、先頭「日界」+998個x、改行は場面encoding/endingどおり。本物のキーでVim count gg/0/count l後、指定ordinary/Vimへ移る。captionをcopy前に取り、copy用:wは私有コピーだけ。前後全client無mask、mode/ステータス位置/元bytesmtime/本文copy/settings/LastInput/正常終了を確認し親が全原PNGを見る。本文の水平scrollは現行UIに無く長桁caretはclipされるため、GUIで確認するのはステータスの行桁literalであり、画面外の本文caretが可視だったとは主張しない。内部位置の全域と0/size_t最大はcore型契約で確認し、GUIへ不可能値を注入しない。captionは同位置・同mode・入力面なしのbeforeguardを兼ね、copy後に同位置へ復帰したafterguardを別に取る（各side12画像、前後12対/24原PNG、24mode/12本文copy/2正常close）。操作計画をafter取得前に固定し、順序・欠測を保持する。

正式は共通status callerのkey-to-frame-singleを既存measure-speed.py --benchで初回5試料新取得し、元基準に照合。他7正式指標は未測。旧files結果はこの変更へreuseしない。純粋比較・正式・GUI取得中は他agent/build/test/hash/copyを停止する。

## 記録・規則・整理

16:41 JSTの最初の製品42fed2957cd1e7f20a44752ced87aae27ea39727は、std::logic_errorの明示throwが通常Releaseの既存clang-tidyにcannot use 'throw' with exceptions disabledとして拒否され、40/117工程で停止した。Release成功・後続probe・Debug・性能・GUIの結果は無い。flags/allowlist/抑制を変えず、失敗source/原command/exit/stdout/stderr/部分build/FileAPI/無exeをimplementation/after-release-failed-product-v1へ保存した。上のexpected/checked accessはこの失敗後の設計改訂である。先行harness3433f1fのbefore buildと7oracleは入力が不変なので再利用する。比較前にharnessから修正版までの全差分を監査し、製品差分はStatusItems.cpp、その他はこのADRの改訂だけとする。初回固定7条件・判定・計測区間は変更しない。失敗buildを成功buildとして収載せず、最終証拠と一緒に保持する。

ARC-001/003/004/007/008/010/011/012、CPP-001/002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0022/0070/0105。API/settings/schema変更なし、waiver none。hideの明示分担に従い親が設計採否、SOLが実装/対象検証、必要な独立読取レビューを行う。

D:/NeNeNib/worktrees/402-status-numberとoutputs/20261011-status-numberを使う。全試行/失敗/原証拠と前件#400の統合・収載・整理を日報/引き継ぎへ残す。技術受理後は通常draftPR/Ready/CI/squash/main同期/恒久収載。取込/dirty/untracked/ignored/唯一成果/稼働/links/絶対D対象を監査して追加領域を整理しbranch/commitを保持する。旧成功は入力不変の限り工程/SHAだけで再実行しない。他数値caller/公開factory/cache/#365/#373/全件回帰は対象外。

## 実施結果

#402 / ADR0113。StatusItems.cppの行・桁表示だけでto_string所有値の連結を除き、局所50byte範囲に固定literalと2個のunsigned十進数をstd::to_charsで書く。private helperは実ecをexpectedへ保持し、最大桁数を保証したcallerでvalue()を使う。書込済み範囲だけを既存fixed→DisplayText::parseで検証・所有し、stack viewは返さない。他製品525files/公開API/入力値/表示文言・順/encoding/ending/validator/settings/schema/flags/allowlistは不変、waiver none。

先行設計e2ca247ffbe05ca9823a55490b096a6fd544a104、harness3433f1f5e92af5ef337a5a860b4ee26c91493d8c、失敗製品42fed2957cd1e7f20a44752ced87aae27ea39727、失敗後設計00847fd8a729093402980e79ffb412b5bd1dedca、修正版製品cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f。初回3利益/4費用、Debug --status-items 24checks（旧8+境界16）/3構成21oracle、既存148probe/47selector/default/1853fixtures保持、GUI12対0px/24PNG・12本文copy・272checkpoint・24mode・2正常close・親全画像目視で2026-10-11 17:16 JSTに技術受理。

3項目の生成・破棄1024回という各局所区間は行123/桁456で約30.48%、size_t最大値同士で約41.69%、行123456/桁654321で約29.29%短縮。encoding/endingも各入力の一部であり、数値長だけの単因子比較ではない。費用4条件と全数値は[gate-proofs 5-ea](../quality/gate-proofs.md#5-ea--ステータス位置の数値を局所領域から所有するissue-402adr0113)。局所率の加算/UI全体への換算はしない。共通status callerの正式key-to-frame-singleを初回5試料[0.609, 0.673, 0.551, 0.519, 0.652]msで新取得し、中央値0.609ms/基準0.906ms/上限2.906ms、欠測0。元基準内の確認であり変更前後のUI改善率ではない。他7正式指標は未測。

全失敗・費用・原証拠はgate-proofs 5-ea。実統合/収載/整理はwave4/acceptance-402。
