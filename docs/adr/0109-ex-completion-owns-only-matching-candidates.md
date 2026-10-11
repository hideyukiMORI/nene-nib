# ADR 0109 — Ex補完の候補は一致を確かめてから所有する

- 状態: 設計受理・実験前（性能・製品採用は未判定）
- 日付: 2026-10-11
- Issue: #394
- 基準: main 5d22ceb376396aba360c14d24b35fd4fe170655c

## 問題と一つの列挙経路

command_completionsはex_command_candidatesが全owning stringを作ってから不一致をeraseする。通常prefixや不一致でもtheme名のvectorと動的なcolorscheme文字列を作る。製品変更はExResult.cppだけとし、private matching_candidates(const ThemeCatalog&, optional<string_view> prefix)を唯一の候補列挙にする。公開ex_command_candidatesはnullopt、command_completionsはengaged prefixで同じhelperを呼ぶ。空prefixとnulloptは別の意味で、空prefixは通常26候補、nulloptはbuiltin時36候補。

1. 既存14候補をprivate constexpr string_view配列一箇所に置き、結果vectorはこの配列の件数14をreserveする。既存initializerの初期割当数を維持し、別の予約量最適化を混ぜない。返却vector/stringの所有を維持し、capacityを契約に追加しない。
2. stem/suffixを受けるprivate prefix照合で、nulloptなら一致、prefix長がstem以下ならstem.starts_with(prefix)、長ければprefix.starts_with(stem)かつsuffix.starts_with(prefix.substr(stem.size()))とする。長さ加算・一時連結なし。raw/case-sensitiveで、trim/colon除去を加えない。
3. 固定14名→palette_names5名→document_names6名とquit直後のquit!→themes.names()→colorscheme systemの順を保つ。通常名は一致後にemplace_back(view)、quit!は一致後に既存のstd::string(name)+"!"、themeは一致後に既存の"colorscheme "+nameで作る。組立式の別最適化は混ぜない。
4. quit!の一致判定をquit本体が一致するifの内側へ入れない。prefix quit!ではquit本体が不一致でもquit!だけが必要である。
5. themes.names()を呼ぶのはnullopt、またはprefixが"colorscheme "から始まる場合だけ。systemも同じbranchに置く。ThemeCatalogの既存names()を再実装せず、失敗したcustom recordの名前を含むbuiltins→custom順を保つ。

CommandChoice::palette_choicesはcase-insensitiveの部分列照合とrankを使うので、従来どおり全候補を受ける。返却所有/順序/Tab cycle/補完seedとindex/失敗時保持、公開API、parser、入力上限、設定、本文、ThemeCatalog、schemaは変えない。cache/new factory/第二の候補表・評価経路は増やさない。閉集合のdefault/else・抑制・gate緩和なし。helperはCPP012（60行・4引数・入れ子3）内に分ける。

規則: ARC-001/003/004/005/007/008/011、CPP-002/003/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0016/0021/0032/0060/0062/0066/0067/0075/0081/0082。waiver none。C2の追加評価であり元26候補の採否件数を増やさない。

## 固定入力と区間

OUT=D:/NeNeNib/outputs/20261011-ex-completion、WT=D:/NeNeNib/worktrees/394-ex-completion。実装前fixed-inputs.json SHA256 ec844c86220934dd7a639b38770ff2cfbd4d1c262a3670fabe0bcbd25f1050e4。新ExCompletionWorkloadは閉enum12値、既存typed dispatch/TimingPort/metadataへ接続する。旧110条件の順/入力/本体/期待、47selectors/default/全旧assertionsは保持。登録総数だけ122へ更新する。整数やfallback分岐で閉集合の網羅性を隠さない。

| workloadのex-completion-以降 | mode / prefix | metadata bytes / FNV1a64 | 結果件数 / 文字列合計bytes | checksum | 分類 |
| --- | --- | --- | ---: | ---: | --- |
| option-1024 | prefix / 'set f' | 155 / 4958441773274844346 | 1 / 13 | 15360 | benefit |
| miss-1024 | prefix / 'unknown' | 157 / 6699480406065360290 | 0 / 0 | 1024 | benefit |
| custom-narrow-128 | prefix / 'colorscheme user-0123' | 2747 / 7858797942040090021 | 1 / 21 | 2944 | benefit |
| empty-1024 | prefix / (empty) | 150 / 18242349949620908404 | 26 / 199 | 231424 | cost |
| set-wide-1024 | prefix / 'set ' | 154 / 9553897328609134628 | 8 / 99 | 110592 | cost |
| theme-wide-1024 | prefix / 'colorscheme ' | 162 / 1449422857361388192 | 10 / 230 | 246784 | cost |
| all-builtins-1024 | all / (empty) | 147 / 9382821741934878839 | 36 / 429 | 477184 | cost |
| custom-wide-128 | prefix / 'colorscheme ' | 2738 / 9480571665409447711 | 266 / 5606 | 751744 | cost |
| all-custom-128 | all / (empty) | 2723 / 7981154346645823048 | 292 / 5805 | 780544 | cost |
| frame-ex-option-64 | frame-ex / 'set f' | 222 / 1915732327603549999 | 1 / 13 | 1856 | cost |
| frame-ex-empty-64 | frame-ex / (empty) | 217 / 14254370962528029111 | 26 / 199 | 15040 | cost |
| frame-palette-64 | frame-palette / ':fz' | 225 / 4426202880856066747 | 0 / 0 | 832 | cost |

metadataはfixed JSONのmetadataUtf8そのもの。mode/prefix/catalog種と全catalog名を改行で直列化し、frameにはbody/path/visible/editor-modeも加える。prefixとcatalog生成・hash・期待値構築は区間外。native/Pythonのmetadata bytes/FNV/SHAを独立JSONへ照合し、実際に使うcatalog namesとrecord状態も区間外で検証する。

builtins名はubuntu-aubergine/neutral-light/solarized-dark/solarized-light/monokai/dracula/one-dark/night-owl/night-owl-lightの9個を独立literalとして固定。通常26候補の全literalもJSONへ固定した。customはuser-0000〜user-0255の256records、全valueはunexpected(ThemeFailure::invalid_color)。names()が失敗recordも補完する契約のstress入力であり、成功/失敗混在はunitで別に照合する。custom narrowのprefixと結果はcolorscheme user-0123だけ。customの計測は1区間128回、builtinのpureは1024回とし、case名/metadata/期待値へ明示する。

pure9条件は同じimmutable catalog/prefixからcommand_completions（mode=allだけex_command_candidates）を繰り返し生成・破棄する。checksumは各結果ごとに1+候補数+全stringのbytes合計。前後の全内容/順序、元prefix/catalog、保持した結果vectorとそのstring_viewの不変性を区間外で照合する。空vectorでは保持したempty自体を確認し、前件と同じ実装関数から期待を生成しない。

frame3条件はEditing足場、OpenDocument(C:\nib-probe\probe.txt)、body a x a、Vim NORMAL、visible1、実caret/status1,1、incsearch/hlsearch on、history/bookmarks空、folder batchなし。ExはVimCharacter ':'で開き、optionだけCommandText(set f)を入れる。emptyはCommandTextを送らない。PaletteはOpenCommandPalette→CommandText(:fz)でcommands scope・1 fill候補set fontsize=/selected0を持つが、raw colon prefixのcompletionsは空である。

区間は同じ準備済みcontroller.frame()の64生成/破棄だけ。apply/delivery生成は入れない。checksumはcommand_line.completionsの上記pure値にtitle9bytes+input text bytesを加算する。全frameの独立期待（本文/表示starts/選択・検索なし/実caret/status1,1/保存済みUTF8文書/入力text・caret・prompt・候補・completion_indexなし/Palette row/設定writes0）、元controllerと保持snapshot/view不変を区間外で照合する。frame内optionalの成功を推測してunsafeに読む書き方はしない。

既存command-input-controller-fill-64を直接のall-catalog caller費用として定義不変で足す。input :fz（3bytes/FNV11941784898562763057/SHA d58613416642baf2b9a49d9ee7107e85ac886af97e0086632f0892ef85fc331b）、checksum2304。Submit/Cancel/OpenPalette/CommandText(:fz)を64cycle、再open/filter/folder requestと足場request vector成長を含む。旧ADR0108の定義/期待を変更しない。

計13条件（3利益/10費用）、同一harness前後の20iterations/ABBA3/各120対応組を一度だけ取得する。利益は対応比中央値<1・全3cycle比<1・短縮>=90。費用は比<=1.10かつ差中央値<=50µs/固定区間。全oracle成功を前提とし、未達は不採用、欠測/外乱は保留。除外/補完/合格までの再取得なし。custom128/builtin1024/frame64を混同せず、局所率をUI全体へ換算・他改善と加算しない。

## 差分に必要な正しさ

既存--ex-settings/--command-palette/--user-theme-selectionをDebug ASan/UBSan/no-recoverで各一度実行する。補完cycle/編集リセット、palette全列挙→部分列/rank/fill/実行、custom成功/失敗/選択という直接境界を通す。他の本文編集/検索/保存全件を追加しない。

不足する独立期待だけ19checksを追加する。ExSettingsに16: all builtin36全文順、empty26全文順、col、colorscheme（空白なし）、colorscheme空白（10全文順）、colorscheme d、dracula完全名、draculax過長、Colorscheme大文字、先頭空白、raw colon、q（quit/quit!順）、quit!、quit!!、system完全名、systemx。UserThemeSelectionに3: 既存user_catalog（broken失敗/my-theme成功/z-theme成功）の全39順、colorscheme空白の13順、colorscheme brokenの失敗名一件。旧assertionsは変更せず、既存entryから新契約を呼ぶ。

new12+既存fill1はbefore Release/after Release/after Debug各1iterationでoracle/metadata/markを確認する（計39、速度の試料ではない）。新入力/不正名と旧110登録のPython対象、新enum漏れcompiler拒否、旧selector/default/assertions/fixturesをsourceで確認。厳格flags/tidy/差分format、変更source/CMake graph、core/application symbolsを選ぶ。beforeにもFile API queryを置き、全build/configure/compiler/unit/probe/native stdout/stderrを最初から明示captureする。通常after Releaseはeng/build-release.ps1の製品40hex Refで作り、同じbuildへprobeを追加する。Release unitの重複実行、全件回帰、coverage、Vim oracle再生成はしない。

## GUIと正式指標の範囲

beforeは#392通常Release（SHA be474374731a5a56c26d82ab82a66754c69c61b25f054bdb578a87c97a2e7247）をwave4/snapshot-392から再利用する。製品c6ce1ce→基準5d22cebはdocs8filesだけで、製品/build不変を確認する。afterは新製品commitの通常Release。専用D:/NeNeNib/evidence/t394/probe.txt、UTF-8本文alpha 日界 omega<CR><LF>alpha 日界 last、private profile/settings/13.5pt/client1280×800を使う。全client RGB無mask前後比較、LastInput/foreground/title/client/closedIME/mode/2560点非被覆/元bytesとmtime/全本文copy/正常closeを記録する。

7sceneを先行固定する: (1):set f（補完fontsize=1件）、(2):set f<Tab>（set fontsize=/seed set f/index0）、(3):の後Tab26回（saveas/caret6/seed空/index25、26補完）、(4):colorscheme drac（dracula1件）、(5)同じprefixの後Tab（colorscheme dracula/caret19/index0）、(6):colorscheme空白の後Tab10回（colorscheme system/caret18/index9、10補完）、(7):tabs<CR><Home><Delete>:fz（raw:fz/caret3、1 fill候補set fontsize=、completions空）。どれもSubmitで設定を適用しない。

captionの後にEsc/:w専用別名で全本文確認→gg0で戻し、入力中にbodycopyのEscを挟まない。実body caret/statusは全て1,1、Ex中はbody caretとmode toggleを描かないので直前/Esc後にVim modeを確認する。描画は6行へclipされ得るため全26/10候補の全文順はcore/frame oracleと区別し、表示された範囲と選択候補を目視する。全caption/guard PNGを親が目視する。既存driverはShift-Tabのpost表記を持たず実modifier入力が要るため、GUI末尾はforward Tab反復で到達する。逆順cycleは既存unitで確認し、GUIで実行したとは書かない。GUI script/plan/input/exe SHAと全鍵列は窓0で固定し、失敗原recordを残す。

canonical正式8指標はEx/commands scopeの候補生成を刺激しない。QUALITY_GATES冒頭/QLT001/012/ADR0021が旧頻度規定を置き換え、退行根拠の無い検証を禁じる。ADR0075決定2の検査基準は保つが毎回無関係8本を測り直さない。決定4は条件内比較か該当正式benchでの判定を要求し、未実装benchの新造を命じない。本件は固定core/直接frame/all-catalog callerの利益と費用を検証し、基準/許容/toolに変更を加えない。

#392 key-to-frame-palette-5000の初回成功（5試料中央値2.159ms/上限4.630ms、71PNG目視完了）は、files-scopeのsource/caller/input/toolchain/host必要条件が不変と確認した一指標の限定記録として再利用する。次のexeを新しく正式測定した、正式8本が全部通った、Ex UIの速度を測ったという主張はしない。新QLT014指標/基準を追加せず、無関係な正式7本も実行しない。実装がこの範囲を越えた場合は測定前に設計を再評価する。性能中は他build/test/GUI/hash/copyを止め親が排他取得する。

## 分担と完了

hideの今回の明示指示によりAstraが設計/採否、SOLがharness/製品/対象検証、別SOLがGUI準備と必要な独立レビュー、LUNAが独立読取照合を担当する。製品実装の前に本ADRをcommitし、harness→before→製品を別commitで保持する。全試行/失敗/未測/費用は日報・引き継ぎ・qualityへ記録する。

#392はPR393/main5d22cebに統合、wave4/snapshot-392の6295records/155241400bytesを全SHA収載し、12:04:12 JSTに追加WT/OUT/t392を監査整理済み。branch/commitを保持し、実結果はacceptance-392。次候補調査/規範の独立再照合と訂正は同snapshot内に原文ごと保存した。

採用時は通常draft PR→必要レビュー/Ready/必須CI→eng/merge-pr.py plan/同expected-head execute→main同期→恒久収載→絶対D path/取込/dirty・untracked・ignored/唯一成果/稼働/linkを監査して追加WT/OUT/t394を整理する。branch/commitは保持。成功済みの実装/入力/依存/必要環境が不変なら工程/文書/commit SHAだけで再試験しない。#365/#373・C8入力parse保持・R8・IO6〜IO8の状態は変えず、hideの停止まで改善を継続する。
