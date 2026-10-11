# ADR 0110 — 補完prefixは同期呼び出しの間だけ借用する

- 状態: 技術受理（2026-10-11 13:57 JST、統合・収載・整理は別記録）
- Issue: #396
- 基準: main062f973041afb02f4551f7fcb36d1e7f5012d222

## 決定

CommandLine::completionsのcompletion_seed_.value_or(text_)はconst& overloadでもstd::stringを値で返す。既存command_completions(string_view, themes)はprefixを保持せず、同期的に所有vector/stringを返す。そのcallまでreceiverのmember寿命が続くので、has_value()?value():text_のconst lvalueをconst参照に束縛して渡す。新API/helper/cache/列挙経路は作らず、製品変更はCommandLine.cppのこの関数だけとする。completed()のnext.completion_seed_への別value_or代入は所有のため維持する。

空のengaged seedは非engagedと異なる。Tabでtextがcolorschemeになってもseed空なら通常26候補を返す。text/caret/index/catalog、返却結果の所有、Tab順/逆順/編集reset/失敗時保持、入力上限、設定/schema、ExResultの正本列挙は変えない。optionalはhas_value/valueで読む。抑制/例外経路/閉集合fallbackを増やさない。

ARC-001/003/004/005/007/008/011、CPP-002/004/005/007/008/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0022/0023/0032/0060/0062/0075/0082/0108/0109、waiver none。既存C2の追加評価で、元26候補の採否件数は増やさない。

## 先に固定する入力と採否

OUT=D:/NeNeNib/outputs/20261011-completion-prefix、WT=D:/NeNeNib/worktrees/396-completion-prefix。fixed-inputs.json SHA256 73b368c2ef29da2f63ebc2c3cb1147c5a087837fef6fc483c6bfc63b2c29b3a4。独立literalの全通常26候補とbuiltin9名、前件から値を変えず引き継ぐ3frame条件を含む。入力の算数を固定前に確認した調査記録もOUTへ残す。

新CompletionPrefixWorkloadは閉enum7値で、既存typed dispatch/TimingPort/metadataへ接続する。既存122条件の定義・順序・入力・期待、47selector/default/全旧assertionsを保持し登録総数だけ129とする。新pure7条件はCommandLine::empty(catalog).inserted(input)と必要ならedited(complete_next)を区間外で準備し、同じconst CommandLineのcompletionsを4096回生成・破棄する。区間内には準備/insert/Tab/期待生成を含めない。

| workloadのcompletion-prefix-以降 | 準備入力 / 操作 | 公開text bytes / caret / index | metadata bytes / FNV1a64 | checksum | 分類 |
| --- | --- | --- | --- | ---: | --- |
| long-miss-4096 | edit C:\nib-probe\ + 日79個 + Z / raw | 256 / 256 / なし | 414 / 17899993968014254022 | 4096 | benefit |
| theme-raw-4096 | colorscheme drac / raw | 16 / 16 / なし | 174 / 6355724148303210051 | 86016 | cost |
| theme-seeded-4096 | colorscheme drac / Tab1 | 19 / 19 / 0 | 179 / 9157927312824126276 | 86016 | cost |
| short-raw-4096 | set f / raw | 5 / 5 / なし | 163 / 5146725973529891191 | 61440 | cost |
| short-seeded-4096 | set f / Tab1 | 13 / 13 / 0 | 168 / 4940635072069788562 | 61440 | cost |
| empty-raw-4096 | 空 / raw | 0 / 0 / なし | 158 / 7231983662249126709 | 925696 | cost |
| empty-seeded-4096 | 空 / Tab1 | 11 / 11 / 0 | 163 / 16657912587326077742 | 925696 | cost |

longは98コードポイント・256UTF8bytesのBMPだけの有効入力。候補はlongが空、themeがcolorscheme dracula一件、shortがset fontsize=一件、emptyが通常26件/199bytes。checksumは各結果の1+件数+全string bytesの合計を4096回加算する。metadataUtf8はmode、raw input、rounds、catalog種、全名を改行で直列化し、JSONのbytes/FNV/SHAへnativeとPythonを照合する。元raw/prepared text/caret/index/catalog、全候補全文順、保持したvectorとstring_viewが後続callで不変であることを区間外で検査する。private seedを読んだと主張せず、操作と公開状態を区別する。

ADR0109のex-completion-frame-ex-option-64/empty-64/palette-64を費用に加える。fixture/区間/checksumは同じ（1856/15040/832）。application::view_of(CommandLine)とcommands-scope paletteのcompletions_ofがこの関数を呼ぶ。旧controller-fill64の計測中はapply/deliveryだけでcompletionsはoracleのframe内なので、今回の費用へ追加しない。

計10条件=1利益/9費用。同一harnessのbefore/afterで20iterations/ABBA3/各120対応組を初回一度だけ取得する（120runs/2400durations/4800marks/1200pairs）。利益は対応比中央値<1・全3cycle比<1・短縮>=90、費用は比<=1.10かつ差中央値<=50µs/固定区間。全oracle成功が前提。未達は不採用、欠測/外乱は保留。除外/補完/合格までの再取得なし。pure4096とframe64の値を混同せず、局所効果をUI全体へ換算/合算しない。測定中は他build/test/GUI/hash/copyを停止する。

## 差分に関係する確認

既存--ex-settings225/--command-palette495/--user-theme-selection97の817checksをDebug ASan/UBSan/no-recoverで一度実行する。既存cycle/逆順/reset/失敗保持と、直接palette/使用catalogの境界を通す。空seedと返却保持の不足分は新probeの独立oracleで覆い、重複unitを追加しない。Release unit再実行/全件回帰/coverage/Vim oracle再生成はしない。

新7pure+既存3frameをbefore Release/after Release/after Debug各1iterationでoracle/metadata/mark確認する（計30、速度試料ではない）。新登録/入力/不正名と旧122をPython対象で確認し、新enum漏れをcompilerが拒否することを確認する。差分strict flags/tidy/format/source graph、core/application symbols、旧selector/default/assertions/1853fixturesのprotected差分を記録する。全configure/build/compiler/unit/probe/native stdout/stderrとFile API queryを最初から保存する。after通常Releaseはeng/build-release.ps1の製品40hex Refで作り同buildへprobeを追加する。

GUI beforeは#394通常Release（製品4d9aaef、SHA239b624debca978767756a739dfd0772fda09b5073e6ba0757041cc31cfe194c）をsnapshot-394から再利用し、mainとの製品/compile環境不変を確認する。専用D:/NeNeNib/evidence/t396/probe.txt、UTF8本文alpha 日界 omega<CR><LF>alpha 日界 last、private profile/settings/13.5pt/client1280×800を用いる。7sceneはlong入力、set f、set f<Tab>、colorscheme drac、同Tab、空からTab1（colorscheme/seed空/index0）、commands palette raw:fz。設定をSubmitしない。全client RGB無mask比較とLastInput/foreground/title/client/closedIME/mode/非被覆2560点/全本文copy/元bytes・mtime/正常closeを記録し、全caption/guard PNGを親が目視する。caption後のEscと専用別名:wで本文確認しgg0へ戻す。Ex中はbody caret/mode toggleを描かないので直前/Esc後に確認する。描画の候補clipと全候補oracleを区別し、逆順は既存unitの結果とする。GUI script/plan/input/exe SHA/全鍵列は起動前に固定する。

## 正式記録・統合・整理

QLT001/012とADR0021を優先し、無関係な正式8指標を再取得しない。files scopeはcompletions_ofのguardから空を返し今回のcompletionsを呼ばない。#392 key-to-frame-palette-5000初回成功（5試料中央値2.159ms/上限4.630ms・71PNG目視済み）を関連source/caller/input/tool/112 production compile flags/host不変の証拠とともに一指標だけ限定再利用する。旧c6ce1ceからの製品差分はExResult.cppとCommandLine.cppで、files scopeのinsert/parse/filter等は不変と確認する。新exeの正式測定・正式8全成功・Ex UI速度と呼ばない。新基準/閾値/観測経路を足さず、範囲を越える差分が必要になれば測定前に再評価する。

hideの現在の分担指示に基づきAstraが設計/採否、SOLがharness/製品/対象検証、別SOLがGUI準備と独立レビュー、LUNAが読取照合を担当する。先行ADR→harness→before→製品を別commitで保持する。採用時はdraft PR/独立レビュー/Ready/必須CI/eng/merge-pr.py planと同head execute/main同期/恒久収載/絶対D path・取込・未保存/未追跡/無視ファイル・唯一成果・稼働・リンクの監査整理まで完了する。branch/commit保持、成功検証は工程/文書/SHAだけで再実行しない。全試行/費用/未測/制約を日報・引き継ぎ・qualityへ残す。#365/#373・C8・R8・IO6〜IO8の処分は変えずhideの停止まで継続する。

## 実施結果

#396はADR0110/harness1917db4/製品1ec1264。CommandLine::completionsだけでoptionalの一時string copyをconst参照へ置換し、同期APIへ渡す。completed()の所有seed、空engaged seed、返却所有と他製品全文は不変。初回1利益/9費用、817既存checks/30oracle、旧122条件/47selector/1853fixtures保持、GUI21対0px/14本文copy/232checkpoint/30mode/2正常close/親全42PNG目視で2026-10-11 13:57 JSTに技術受理。

256bytesの長prefix候補なしを4096回生成・破棄する局所条件で約38.03%短縮。全10行の費用も保持し、UI全体の改善率へ換算しない。新正式取得0。files scopeが今回関数を呼ばず、関連source/112compile flags/tool/input/host等が不変な旧#392 files palette5000（中央値2.159ms/上限4.630ms）だけを限定reuse。新exeの正式測定や全8成功ではない。全試行/コマンド/費用/限界は[gate-proofs 5-dx](../quality/gate-proofs.md#5-dx--補完prefixを同期呼出しの間だけ借用するissue-396adr0110)。API/settings/schema変更なし、waiver none。

統合/恒久収載/監査整理の実完了はwave4/acceptance-396。
