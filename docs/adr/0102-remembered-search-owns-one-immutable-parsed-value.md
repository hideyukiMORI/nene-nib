# ADR 0102 — 確定した検索は文字列・方向・解析結果を一つの不変値として共有する

- 状態: 提案・実装と固定比較前
- 日付: 2026-10-10
- Issue: #376（C8の確定済み検索を独立評価）
- 規則: ARC-001/002/003/004/005/007/008/011、CPP-002/003/004/005/006/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004
- 前提: ADR0032/0037/0041/0043/0088/0082/0100。#367を統合したmain bfa1b32を基準とする。

## 問題と意図

C8の確定済み検索を独立実験にする。VimState.last_searchは公開DTOの文字列を毎状態コピーで複製し、EditorController::search_patternとVimStep::search_fromは同じ文字列・方向をframeやn/NのたびにVimPattern::parseする。既存parser/評価器/走査を一つのまま、不変の文字列・方向・解析成功または失敗を一度所有し再利用する。

## 設計範囲

新core型VimSearchSnapshotはprivateな共有const Storageへ防御コピーした文字列、方向、既存VimPattern::parseのexpected結果を持つ。生成口from(string_view,direction)だけ、既定ctor/shared_ptr入力/可変viewなし。copyを明示して暗黙moveで元をnullにしない。text/parsedの借用はconst&だけ、const&&はdelete。VimState.last_searchのoptionalをこの型にする。VimSearchPatternはfrom起点を運ぶ既存command-key DTOのまま、録画・dot・マクロの形式は変えない。

検索確定・語検索だけが新snapshotへ置換する。未対応構文も失敗値として覚える。空の確定は前のraw文字列を使い、新しい方向で必ず解析する（区切り解釈が変わるため）。n/Nと描画は保持結果を借りる。本文一致結果やcaretは保存しない。

入力中SearchLineの解析と所有は維持する。controller内のprivateな閉じたvariantは、入力中の所有VimPatternまたは確定snapshotからのconst参照を表す。visible_linesの同期呼出し内でだけ選択・借用し、各行は同じVimPatternを読む。型別overloadのvisitorで閉集合を保ち、default/曖昧なfallbackは作らない。const frame中にstateは置換しない。renderer/IME/search evaluator/本文走査は変更しない。

## 規則と直接検証

ARC-001/002/003/004/005/007/008/011、CPP-002/003/004/005/006/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。ADR0102を実装前に固定。ownership試験は検索契約へ統合し、raw mutable alias、copy/rvalue後の元と先、成功/全失敗種別/方向、借用のrvalue拒否、二枝、空再利用を確認。既存search/highlight/incremental/tabsの直接scopeと固定search fixturesを実行し、期待値は変えない。型移行の既存testはアクセスだけ更新する。正規Debug/tidy/ASan/UBSan、source/graph/symbol、CMake/型付きprobe dispatchの対象検証。無関係全gateは行わない。

## 取得前の固定比較

旧57workloadを保ち、新しい閉enumの8条件をADR0082の同じ表/TimingPort/比較器へ追加する。before/after同一harness、main bfa1b32の製品がbefore。入力hashは実際の検索pattern bytes（a/256個のa/未検索の空bytes）、本文・操作・oracleも同一harnessの固定literalとする。

| 条件名 | 固定処理 |
| --- | --- |
| search-snapshot-frame-short-64 | 本文a xのCRLF3行、pattern aを確定、caret1/1、3行viewport、const frame生成破棄64回 |
| search-snapshot-frame-long-64 | 本文x、pattern a×256を確定し報せをhで消す、1行viewport、const frame生成破棄64回 |
| search-snapshot-repeat-long-64 | 本文a×256 + 空白x空白 + a×256、同長pattern、caret1/1、nNを32組（64keys） |
| search-snapshot-retained-long-200 | 空本文で長patternを覚え、通常モードでInsertText{x}200回 |
| search-snapshot-retained-short-200 | 上と同じ、短pattern a |
| search-snapshot-unsearched-200 | 空本文/検索履歴なし、通常InsertText{x}200回 |
| search-snapshot-commit-short-64 | 本文a x a、検索履歴なし/caret1/1、確定query aを64回、最後caret1/1 |
| search-snapshot-typing-frame-short-64 | CRLF3行、/aの入力途中、未確定、3行viewport、const frame生成破棄64回 |

frame条件は64回の破棄と既定少数field checksumも区間内、準備/全文・行値・表示map・span・caret・mode・last_search/message/保存oracleは前後の区間外。操作条件は上記applyだけを区間内、最後のEditorDeliveryを保持し照合と破棄は区間外。期待値はsnapshot新API/製品parserから生成せず、beforeで同じharnessを動かす。

各20iterations、ABBA3、各120対応組を一度だけ、全試料/失敗保持、outlier除外/0補正/欠測補完/成功まで再試行なし。long frame/repeat long/retained longの3条件はそれぞれ対応比中央値<1、全cycle<1、短縮>=90/120を要求。残5条件は対応比中央値<=1.10、対応差中央値<=50us（64frame/64確定）、<=100us（200入力）を費用上限とする。全必須条件成立後のみ利益候補。新規解析結果は検索寿命まで保持し、短検索でも共有制御領域を一つ確保する代償を明記する。原子/集合範囲O(A+R)の保持であり実capacity/RSSは未測として扱う。

通常Releaseで検索確定/方向/失敗/空再利用/hlsearch/通常復帰/入力中/タブの限定GUIを取得前台本で固定し全製品画素比較。共通VimStateとframe選択の直接callerに必要な正式速度を選び、基準/許容は変更しない。独立読取レビューで利益と代償を判断し、未達は不採用・外乱欠測は保留。新規検索構文・入力中解析保持・#365/#373は別件。

## 配置と完了

追加worktreeはD:/NeNeNib/worktrees/<issue>-search-snapshot、OUTはD:/NeNeNib/outputs/20261010-search-snapshot。単体実行と必要な独立読取レビューだけ。採否/PR/必須CI/main同期/恒久収載後、絶対path・未保存/ignored・唯一成果・稼働参照/linkを監査して作業物を整理し、branch/commit/原記録を保持する。hideが停止するまで継続する改善依頼の次の独立実験。

## 実装前の所有と借用の具体化

VimSearchSnapshotのStorageは同classのprivate nested structとし、外部が構築/変異できる名前付きaggregateを増やさない。fromはraw string_viewを一度std::stringへ複製し、その所有文字列からVimPattern::parseを一度呼ぶ。成功/失敗を同じprivate Storageへ移し、make_shared<const Storage>で所有する。nullは生成しない。明示default copy constructor/copy assignmentによりrvalueからも共有copyとなり、元/先の借用が有効であり続ける。private constructorだけが内部shared_ptrを受ける。想定内の不正構文はsnapshot生成失敗ではなく、覚えた解析失敗の値として保持する。

公開のtext() const&はstring_view、parsed() const&はconst expected<VimPattern,VimPatternFailure>&、direction()は値を返す。text/parsedのconst&& overloadをdeleteし一時値から借用できなくする。shared_ptr/Storageそのもの、mutable view、raw所有move口、既定constructorを公開しない。本文や検索の一致/起点は所有しない。coreの型付きAPIはlast_searchと読取型が変わるが、ユーザーの鍵・保存schema・parserのAPI/解釈は変わらない。

EditorControllerのprivate alias SearchPatternはvariant<VimPattern,reference_wrapper<const VimPattern>>。search_pattern()は入力中なら従来typed_patternの所有値、確定済みならsnapshotの解析成功値へのcrefを返す。失敗は描画用の選択値なしへ写し、入力中の失敗を古い確定結果で塗らない。visible_linesはoptional variantをlocal所有し、型別frame_pattern overloadを呼ぶvisitorでconst VimPattern*を一度取り、行loopへ渡す。nullptrは私有引数で「描くpatternがない」だけを表す。loop終了までlocal所有とstateのsnapshotがともに生存し、line_viewは借用を保存せず値だけ返す。公開nullable pointer APIや別parserは追加しない。

VimStepのsearch_fromはlast_searchをconst参照し、stored expectedの失敗を従来のnoticeへ、成功を同じvim_find_matchへ渡す。searched_keyは空ならprevious.text()、非空ならkey.patternをfromへ渡し、key.directionで新値を作る。from起点は元の一回の検索requestだけに使う。word_searchも同factoryを通る。n/Nの向き/回数/折返し/オペレータ、検索失敗の記憶、highlight三値、タブの所有は既存契約どおり。

新probeはSnapshot APIを一切使わず、既存controller intentとframe/saveの公開値だけで準備・計測・照合する。旧57登録/関数本体/入力/区間は保持し、新SearchSnapshotWorkloadの8caseを一つの65行表へ末尾追加する。新enumの網羅switchを加え、旧型のindex範囲は変えない。名前/型/関数/sizeをconstexprで強制し、新case欠落のcompiler反例だけを追加する。既存の型不一致/未知alternative拒否はテンプレート/visitor本体不変の証拠を照合して#367成功を再利用する。

固定採否はこのADRの表と条件で凍結する。局所の3利益条件と5費用条件、通常製品の同値/必要な正式速度/独立レビューが成立したときだけ採用する。未達を後から緩めず、欠測なら保留。保持量はraw文字列に加え解析した原子列/集合範囲/共有制御領域が検索寿命まで残る。payloadのO(A+R)という構造の説明とallocatorの実capacity/RSSを区別する。

## 取得前レビューによる入力の修正（20:19 JST）

最初の設計commit 41fae51はraw a×4096を利益条件にしたが、独立読取レビューで通常SearchLineのInputTextはDisplayText::maximum_bytes=256に制限されると確認した。まだbuild/速度取得前である。3利益条件のpatternをa×256、repeat本文を同256文字の2語へ変更し、全8条件・反復・採否の数値は維持する。最初の設計はcommitへ残す。共通harnessは各long条件の準備前に / → CommandText{256文字} → SubmitCommandという正規入力の到達を区間外で検証する。候補APIは参照せず、記憶の詳細は型固有unit/公開検索動作で確かめる。

4096文字語の * / # は正規経路で上限を通らず生成できるため、長い所有と検索意味の契約試験に残すが、今回の速度利益には数えない。空再利用は a? のforward成功→backward失敗→forward成功、入力中無効patternは古い確定値へ戻らないことを守る。raw/atoms/ranges/共有制御領域は通常モード・タブ移動でも残り、最後の共有所有者の解放まで保持する。
