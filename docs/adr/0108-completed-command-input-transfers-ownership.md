# ADR 0108 — 完成したコマンド入力は既存の所有先へ移す

- 状態: 技術受理（2026-10-11 11:47 JST、統合・収載・整理は別記録）
- 日付: 2026-10-11
- Issue: #392
- 基準: main c203f0e7d6f22221f6f31e3db340838486e86998

## 問題と決定

SearchLine/CommandLine/CommandPaletteとapplication CommandInput/EditorControllerは、成功して以後使わない局所expected/InputTextをconst lvalueで既存の値渡し先へ送り、完成した文字列を再び複製する。検証・編集・再filter・state更新は今の経路のまま、次の8境界だけ所有を移す。

1. SearchLine::insertedのnextを非constにし、成功分岐で`SearchLine(std::move(next).value(), direction_)`へ渡す。
2. CommandLine::insertedのnextを非constにし、成功後にcaretを小さい値として先に退避し、`std::move(next).value().text`を既存constructorへ渡す。
3. CommandLine::editedの完成InputTextを非constにし、caretを先に退避してからtextをmoveする。補完分岐は変更しない。
4. CommandPalette::insertedとfilledは成功expectedを非constにし、`refiltered(std::move(next).value())`相当で既存の値渡し先へ送る（2境界）。
5. application inserted_commandのvisitorは成功alternativeをmoveして同じCommandInput variantを作る。
6. EditorController::accept(CommandText)とpalette fillは、成功expectedをmoveして同じwith_command_inputの値引数へ送る（2境界）。

失敗は今のearly returnより前に消費しない。元state/input/借用をmoveせず、呼び手が作った独立の完成値だけを一度消費する。InputText内部の防御copy、themesの共有参照copy、EditorState setterの旧状態copyは残す。borrowとconsumeを同じ引数式へ混ぜず、消費後に局所値を再読しない。既にprvalueで届くSearchLine::edited、CommandPalette::edited、edited_commandは変更しない。

製品はsrc/core/SearchLine.cpp、CommandLine.cpp、CommandPalette.cpp、src/application/CommandInput.cpp、EditorController.cppの5files。公開API・新factory・cache・入力上限256bytes・parser・候補保持/並び・補完・設定・本文・保存schemaは変えない。ADR0081/C2の未評価部分で、元26候補の新規件数へ数えない。

ARC-001/003/004/005/007/008/011、CPP-002/003/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0032/0041/0060/0062/0081/0082、waiver none。

## 固定入力と計測区間

OUTはD:/NeNeNib/outputs/20261011-command-input、WTはD:/NeNeNib/worktrees/392-command-input。実装前のfixed-inputs.json SHA256はeac377e57d999400fba4f5979ab0cb6217af3608f0d17cb7da8fc00bd11d7f74。全入力bytes/FNV/SHA/独立期待を保存する。新CommandInputWorkloadの閉enum12値を既存typed dispatch/TimingPort/metadataへ足し、旧98workloadの順/入力/本体/期待値、旧47selector/default/既存assertionsは保持する。登録総数だけ110へ更新する。

| workloadのcommand-input-以降 | 主入力（metadata） | 区間と独立期待 | checksum | 分類 |
| --- | --- | --- | ---: | --- |
| search-long-1024 / ex-long-1024 / palette-long-1024 | a×240（240bytes/FNV10285162318351609365） | inserted_command(source, 日🖋)を同じimmutable sourceから1024回生成/破棄。結果a×240+日🖋、247bytes/242code points/caret247 | 各505856 | benefit3 |
| search-short-1024 / ex-short-1024 / palette-short-1024 | a（1byte/FNV12638187200555641996） | 同上、結果a日🖋、8bytes/3code points/caret8 | 各16384 | cost3 |
| ex-left-long-1024 | a×240+🖋（244bytes/FNV14968357790484435179） | 同じCommandLineのedited(left)を1024回生成/破棄。text不変244bytes/241code points、caret240 | 495616 | cost |
| palette-fill-long-1024 | a×240（240bytes/FNV10285162318351609365） | 空entries/query oldの同じpaletteからfilled(input)を1024回生成/破棄。結果:a×240、241bytes/caret241、commands scope/count0/selected0 | 493568 | cost |
| controller-search-64 / controller-ex-64 / controller-palette-64 | a（1byte/同FNV） | 初期query aへCommandText(b)→EditCommand(backspace)を64cycle。128apply/delivery生成破棄、最後はquery a/caret1 | 各1152 | cost3 |
| controller-fill-64 | :fz（3bytes/FNV11941784898562763057） | Submit→Cancel→OpenPalette→CommandText(:fz)を64cycle。256apply/delivery生成破棄、最後は:fz/caret3/1 fill候補set fontsize=/selected0 | 2304 | cost |

最初8条件のchecksumは結果textのbytes+caret byteを加算する。3variant挿入のSearchはforward、Exはbuiltins、Paletteは空entries/files scope/count0/selected0。空候補を使うのは入力の所有移管を切り分けるためであり、5000件処理全体の利益には代用しない。caret/方向/variant/scope/候補/補完/失敗/全文/UTF-8/code pointsと元入力、区間前に保持した完成値・viewの不変性は区間外で照合する。入力生成とfactory、期待値生成は区間外。

Controller4条件はEditing足場、OpenDocument(C:\nib-probe\probe.txt)、本文a x a、Vim、visible1行、実body caret1,1、既定incsearch/hlsearch on。Searchは/、Exは:、PaletteはOpenCommandPaletteから主入力を入れる。bookmark/historyは空、ScriptedFoldersはbatchを返さない。各deliveryのtitle probe.txtの9bytesを加算する。timingの外で独立したframe/本文/保存状態/入力/prompt/補完/保持snapshot/設定writes0を照合する。Search query aは全matches[1,2),[5,6)、current[5,6)、Exは補完空。Palette query aはraw tab1/filtered0/selected0。fillでは区間外にSubmit一回の:set fontsize=/選択fillと未保存設定writes0も確認してから復元する。

controller-fillは再open/entries/filter/folder requestとScriptedFoldersのrequest vector成長を含む固定cycle全体の費用。fill単独の時間とは呼ばず、足場を特別に変更してその費用を隠さない。純粋filled条件とは分けて記録する。

直接の既存費用palette-append-narrow-5000-to-50を定義不変で追加する。input225009bytes/FNV2074654865412653394/SHA47a09f1d7bb9fa23f3564024f2cd42d470849348c44d6cd5beb7e6b95a4d178e。全13条件は3利益/10費用。同一harness前後・20iterations・ABBA3・各120対応組で一度取得する。利益は対応比中央値<1、全3cycle比<1、短縮>=90。費用は比<=1.10かつ差中央値<=50µs/固定区間。全oracle成功が前提。未達は不採用、欠測/外乱は保留。除外/補完/合格までの再取得をしない。局所率をUI全体へ換算しない。

## 差分に必要な検証

- 既存--ex-settings/--command-palette/--vim-search-incrementalは文字/途中caret/補完/拒否/Controller fill/検索previewと取消・方向・incsearch on/offという直接境界を通す。旧assertionを削らない。
- SearchLineの不足する入力契約だけ6checksを追加する。a×253に日を入れ256bytes受理し元値保持、最大値へのXはtoo_longで元値保持、不正UTF-8はinvalid_text、改行はinvalid_text、backwardのa日🖋zを2回leftしてcaret4へ界を入れa日界🖋z/caret7/方向と元値保持、空挿入はtext/caret/方向を保持。既存CommandLine/Paletteの同種契約を複製しない。
- new12+既存palette1をbefore Release/after Release/after Debug各1iterationでoracle/metadata/mark確認。新入力/不正名と旧登録のPython対象、新enum漏れcompiler拒否、旧98probe/47selector/default/既存assertion不変をsourceで確認する。
- 正規Release前後、Debug ASan/UBSan/no-recover、strict flags/tidy/差分整形・source規約/実CMake graph/core・application symbols/protected-diff。新beforeにもFile API queryを置き、native subprocess stdout/stderrを最初から明示captureする。
- 失敗原因を記録し、成功済みの関連src/tests/入力/依存/必要環境が不変なら工程/docs/SHAだけで再実行しない。全件回帰・coverage・Vim oracle再生成は対象外。

## GUIと正式な速さ

beforeは#388の通常Release（SHA76965ccf74da69d10066241576accfd5a4b62416e0885e2ab5a54b7297b2a418）をsource一致確認して再利用。afterは製品commitからeng/build-release.ps1の明示Refで作る。専用D:/NeNeNib/evidence/t392/probe.txtにUTF-8本文alpha 日界 omega<CR><LF>alpha 日界 lastを新規作成する。private profile/settings/font13.5/client1280×800、全client RGB無mask前後比較を使う。

先行固定sceneは次の9入力と最後の通常mode本文。各scene前にEsc/gg0で入力を閉じ、各capture後にEsc→:w専用別名で全本文照合→gg0とする。入力中に本文copyのEscを挟まない。

| scene | 操作 | 独立期待 |
| --- | --- | --- |
| ex-short | :set fontsize=（Enterなし） | storage set fontsize=/caret13、補完1件 |
| ex-unicode-mid | :a日界b<Left><Left>X<Delete> | a日Xb/caret5 |
| search-forward | /日界 | 日界/caret6、current matchは1行7列から |
| search-mid | /日界<Left>X<Delete> | 日X/caret4、matchなし |
| search-backward | ?alpha | alpha/caret5、後方折返しで2行1列から |
| palette-tab | :tabs<CR>probe | #probe/caret6、1件 |
| palette-fill | :tabs<CR><Home><Delete>:fz<CR> | :set fontsize=/caret14、1件/selected0、設定適用なし |
| palette-edit | 同fill後18<Left><Backspace> | :set fontsize=8/caret14、1件/selected0、適用なし |
| palette-unicode | :tabs<CR>日界 | #日界/caret7、0件 |

実body caretとstatusは全て1,1を保持する。入力中の本文caretは描かず、Searchのcurrent-match枠と入力caretを区別する。Ex/Search中はmode toggleを描かないので直前・Esc後の実RGBでVim modeを確認し、Palette/最後の通常captionではcaptionからもRGB照合する。scene/guard PNGを全て親が目視する。LastInput/foreground/title/client/closedIME/mode/2560点非被覆/元bytes・mtime/settings/正常closeを記録し、失敗原record/PNGを保持して停止する。

GUI鍵入力はBMPの日本語を使う。既存driverはcode pointをWM_CHARへpostするため、非BMPを正しいUTF-16入力として送ったとは扱わず、4byte入力はcore契約/probeで照合する。repo driverは本件で変更しない。全鍵列と本文copy path長を窓0の時点で解析し、詳細script/plan/input/exe SHAを初回操作前に固定する。

正式指標は直接関係するkey-to-frame-palette-5000の初回5試料だけ。既存計測器/基準/許容を変えず、各試料の5000候補PNGとraw/本文不変を確認する。他7指標は未測とする。Ctrl+Pは正規計測器の実chordを使うため、自動入力直前のLastInputと送信後tickを記録し、それ以外の区間に入力変化がないか監査する。自動chord区間内の完全な外部入力識別を主張しない。欠測/外乱は保留し補完しない。性能中は他build/test/GUI/hash/copyを止める。hideの今回の不在・自由な実機試験許可を適用する。

## 分担・結果・整理

hideの今回の明示分担に従い、Astraが設計/採否、SOLが実装と対象検証、別SOLがGUI準備/必要な独立レビュー、LUNAが読取調査を担当する。製品5filesとharness/testsを設計後に実装し、実験前後を別commitに保つ。GUI/性能は親の排他指示後だけ実行する。

#388はPR391/main c203f0eへ統合、wave4/snapshot-388の703records/111398935bytesを全SHAで恒久収載、10:35:05 JSTに追加WT/OUT/専用入力を監査整理済み。branch/commitは保持し、実証拠はwave4/acceptance-388。

本件も全試行/失敗/未測/代償を日報・引き継ぎ・qualityへ記録する。採用時はdraft PR/Ready/必須CI/eng/merge-pr.py plan→同expected-head execute/main同期/恒久収載後、絶対D path/取込/未保存/未追跡/ignored/唯一成果/稼働/linkを監査して追加物を整理する。branch/commit保持。C8入力parse保持・R8・IO6〜IO8は未実験、#365/#373は未解決。hideの停止まで改善を継続する。

## 実施結果

最終harness aa4763d / 製品c6ce1ce。3利益/10費用の固定条件、Debug851checks、39oracle、旧98/47/1853fixture保持、GUI10対0px/20本文/66PNG、正式palette5000初回5試料中央値2.159ms（基準2.630/上限4.630）が成立し技術受理。長いSearch/Ex/Paletteを1024回生成/破棄する局所区間は対応比.489933/.589109/.639037。短Searchは横ばい、Controller Exは+2µs、fill固定cycleは+5µsで事前費用内。局所率をUI全体に換算しない。

110登録によるconstexpr上限とoptional guardのbefore build失敗を原source/全native rawごと保存し、上限・gateを緩めず是正した。GUIはBMPのみ、正式入力の出所保証はcanonical chord区間外に限る。全コマンド・13条件・是正・限界・再利用は[gate-proofs 5-dv](../quality/gate-proofs.md#5-dv--完成したコマンド入力の所有を移すissue-392adr0108)。公開API/schema/waiver変更なし。統合・恒久収載・監査整理の完了証拠はwave4/acceptance-392へ残す。
