# ADR 0113 — ステータス位置の数値は局所領域で組み立てる

- 状態: 設計承認（実験branchの実装を許可。初回比較・関連検証の前であり、製品の技術受理・main統合は未実施）
- 日付: 2026-10-11
- Issue: #402
- 基準: main b13a04b41e708fb198a6b61bdbf84a31f2499278

## 問題と正規経路

StatusItems.cppのcaret_positionはto_stringの所有値を連結し、fixed→DisplayText::parseで最終所有する。ADR0105が対象外とした数値側の中間所有を除く。整数変換を自作せずstd::to_charsの符号なし十進変換を用い、最終のUTF-8/control/256byte/コードポイント検証と所有は既存DisplayText::parseの一経路を保つ。

製品変更はStatusItems.cpp内のprivate caret_position・局所decimal helper・必要includeとstatus_items_forの一呼出しに限定する。caret_positionがDisplayTextを返し、status_items_forはそれと既存encoding/endingのfixed結果を同じ3項目arrayに並べる。公開型/API/入力値/文字列/表示順/label関数/formatter/validatorは変えない。stack viewを返さず、mutable/static cacheと新factoryは置かない。

## 容量・失敗・規範

局所配列はprefix「行 」4bytes、separator「, 桁 」6bytes、2*(numeric_limits<size_t>::digits10+1)で容量を導く。Windows x64では50bytes、最大20桁×2をNULなしで含む。各to_charsの範囲を最大桁数内へ限定し、ec==std::errc{}を確認したptrだけを後段へ渡す。literal copyはこの容量証明の範囲内。終端は書き込まれたptrとの差で求め、未使用byteをparseへ渡さない。0も現公開wrapperが保持できるので拒否しない。

最大桁数を確保したunsigned十進変換の容量不足はプログラム不変条件の破れ。private helperはec失敗時にstd::logic_errorを投げ、部分出力・黙った代替書式・結果型の公開追加をしない（CPP-005）。原則到達不能だが分岐を消さず、Debug ASan/UBSanと最大/0/桁境界を検証する。既存symbol gateを緩めず、新しい外部依存を求めるなら候補を止めて設計へ戻る。

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

ARC-001/003/004/007/008/010/011/012、CPP-001/002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0022/0070/0105。API/settings/schema変更なし、waiver none。hideの明示分担に従い親が設計採否、SOLが実装/対象検証、必要な独立読取レビューを行う。

D:/NeNeNib/worktrees/402-status-numberとoutputs/20261011-status-numberを使う。全試行/失敗/原証拠と前件#400の統合・収載・整理を日報/引き継ぎへ残す。技術受理後は通常draftPR/Ready/CI/squash/main同期/恒久収載。取込/dirty/untracked/ignored/唯一成果/稼働/links/絶対D対象を監査して追加領域を整理しbranch/commitを保持する。旧成功は入力不変の限り工程/SHAだけで再実行しない。他数値caller/公開factory/cache/#365/#373/全件回帰は対象外。
