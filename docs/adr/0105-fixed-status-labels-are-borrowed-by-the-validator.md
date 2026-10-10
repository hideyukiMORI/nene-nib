# ADR 0105 — 固定のステータスラベルは検証器へ直接借用する

- 状態: 技術受理（固定3利益/4費用・対象契約・GUI・正式single成立）
- 日付: 2026-10-10
- Issue: #382
- 基準: main a866f710a6ca108398da882ca3fbfb4e4b17ceec

## 問題と決定案

status_items_forはencoding_label/line_ending_labelが返す固定string_viewを一時std::stringへコピーしてから、private fixed(const std::string&)経由でDisplayText::parse(string_view)へ渡す。fixedをstd::string_viewへ替え、2つの一時std::stringを省く。検証と所有化はDisplayText::parse一経路を保つ。

caret_positionの一時std::stringは同期callの間だけ借り、parseが所有するため寿命を越えない。数値書式/std::to_string/3値array/表示順/公開APIは不変。std::formatはADR0022でcoreへのlocale参照が拒否されており、今回の案には数値書式の作り替えを含めない。製品はsrc/core/StatusItems.cppのprivate引数と2呼出し、string_view includeだけ。

## 規則・範囲

ARC-001/004/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0010/0021/0022/0070/0082。waiver none。公開API/保存schema/設定/基準/許容/型の検証経路/依存許可を変えない。

## 固定入力と区間

新StatusItemsWorkloadの閉enum6条件を旧76の後に同じtyped dispatch/TimingPortへ追加。inputは`encoding表示名|ending表示名|line|column`のASCII bytesで、区間中にparseしない。数値/encoding/endingは型付きの固定descriptorから渡す。期待値は製品のformat/label関数を使わない独立literalで、3表示string/各code point数を前後に照合する。

| 条件 | line / column | input bytes | FNV1a64 | checksum |
| --- | --- | ---: | --- | ---: |
| status-items-utf8-crlf-1024 | 1 / 1 | 14 | 2425243683614802670 | 38912 |
| status-items-bom-lf-1024 | 9 / 24 | 17 | 1768796593162922628 | 45056 |
| status-items-sjis-crlf-1024 | 123 / 456 | 22 | 8885631928267894942 | 55296 |
| status-items-max-utf8-lf-1024 | 18446744073709551615 / 18446744073709551615 | 50 | 13581726349480759485 | 112640 |
| status-items-large-bom-crlf-1024 | 123456 / 654321 | 28 | 7783354242304457080 | 67584 |
| status-items-sjis-lf-1024 | 1 / 1 | 16 | 1943232094207065852 | 43008 |

utf8-crlf、bom-lf、sjis-crlfを3利益とする。残り3を費用にする。caret文字列は順に「行 1, 桁 1」「行 9, 桁 24」「行 123, 桁 456」「行 18446744073709551615, 桁 18446744073709551615」「行 123456, 桁 654321」「行 1, 桁 1」。UTF-8/UTF-8 BOM/Shift_JISとCRLF/LFをliteral照合、code point数はそれぞれ[8,5,4] / [9,9,2] / [12,9,4] / [46,5,2] / [18,9,4] / [8,9,2]。最大値はWindows x64のsize_t64bitを計測器のstatic_assertで固定する。

区間内はconst status_items_for結果の生成・破棄1024回と、3項目のUTF-8 bytes数+code point数のchecksum。準備・入力照合・前後oracleは区間外。初回arrayを保持して区間後にも照合し、借用先の一時値に依存していないことを確認する。既存frame-document-short-saved-256も直接callerの費用として同じ系列へ含め、入力31bytes/FNV3165114504819470625/checksum14592/既存oracleを変えない。

全7条件を同一harnessのbefore/after、20iterations/ABBA3/各120対応組で一度取得する。3利益は対応比中央値<1・全3cycle比<1・短縮>=90/120、4費用は比<=1.10かつ差中央値<=50µs/各固定区間。条件未達は不採用、外乱/欠測は保留、除外/穴埋め/合格までの再取得なし。全rawと失敗を保持し、局所区間をUI全体や1回単独の改善率にしない。

## 対象検証と再利用

新6のPython入力/不正名と旧frame-document登録、旧76入力/関数/typed selector/template不変証明、新enum case欠落の実compiler拒否。変更sourceの整形/規約、実CMake依存/core・application symbols、protected-diff。正規Release前後とDebug/tidy/ASan/UBSan/no-recover、新6と既存frame-document短を各1iteration。

既存CoreTests::verify_status_itemsはverify_look経由の既定実行だけであり、45selectorに直接入口がない。既存assertionを呼ぶverify_status_items_scope wrapperと--status-itemsを追加し、限定実行する。既定のverify_lookと全既存assertion/selectorの意味を変えず、重複assertionを増やさない。新6probeが全encoding/ending・大きな数値・保持値の独立oracleを持つ。

通常Releaseは正規eng/build-release.ps1で作る。GUIは3encoding×2endingの固定6場面、同一本文/私有profile/前後exe hash/実mode/LastInput guards/全client無mask/全本文copy/全画像目視/正常終了を操作前に固定して確認する。数値のformatは変更しない。正式はkey-to-frame-singleだけを初回5試料取得、同じstatus共通callerの普通入力を代表とする。他7指標は今回未測と明記する。実機計測中はbuild/test/GUI/hash/copyを並走しない。

## 採否・記録・整理

単体で設計/実装/検証/自己レビューし、判断の不確実性がある場合だけ必要な独立読取レビューを使う。D:/NeNeNib/worktrees/382-status-labelsとoutputs/20261010-status-labelsを使う。通常PR/必須CI/main統合/恒久収載後、絶対D path/取込/未保存/ignored/唯一成果/稼働/linkを監査して追加物を整理しbranch/commitを保持する。全実験/失敗/判断と前件#380のarchive長path失敗/復旧/整理も日報/引き継ぎへ記す。関連入力不変の成功は工程/文書/SHA変更だけで再実行しない。

数値format、DisplayText公開factory、TabTitleのclip/所有経路、永続cache、C8入力parse保持、#365/#373、全件回帰は対象外。

## 2026-10-10 23:13 JSTの採否

harness c2c7cdc/製品9c6031aで全7条件を一度取得し成立。3利益の対応比.962963/.972727/.981911、短縮110/104/104組。最大行桁1024回+3µs、frame256回+4µsの費用を許容する。既存8checks/新6+旧caller probe、GUI6対0px・全16PNG目視・12本文一致、正式single.626ms/5有効/基準内。単体で差分/寿命/oracle/全rawを自己レビューして技術受理。公開API/schema不変、waiver none。通常PR/CI/main/収載/整理へ進む。

Debug初回native呼出しだけ配列bindingで本体前に失敗、原転記を残して同一PowerShell内の直接呼出しで未開始buildを成功させた。前件#380のarchive長path失敗/復旧も含む全記録と限界は[gate-proofs 5-dr](../quality/gate-proofs.md#5-dr--固定ステータスラベルの一時コピーを省くissue-382adr0105)。取得前の条件は上記から変更しない。
