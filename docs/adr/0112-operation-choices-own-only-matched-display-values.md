# ADR 0112 — 操作候補の表示値は一致した行だけ所有する

- 状態: 技術受理（2026-10-11 16:07 JST、統合・収載・整理は別記録）
- Issue: #400
- 基準: main72e68633badafc69b03d84456a85068001793e30

## 決定

CommandChoice.cppのoperation_choice_of、operation_score、operation_choicesの三つのprivate/public関数本体範囲だけを変える。operation_choicesのoperation_available判定の後でshown_chordとkey_chord_labelを一度だけ評価し、同じローカル所有keyを照合と返却へ用いる。operation_scoreはquery/const OperationText&/string_view keyを受け、text.name/text.reading/keyの三fieldを既存match_scoreへ渡す。一致した行だけoperation_choice_of(text,std::move(key))を呼ぶ。operation_choice_ofはkeyをby-valueで受け、従来のDisplayText::parse(name/description).value()で表示値を所有し、同じkeyをCommandChoiceへmoveする。private helper二つのsignature以外にAPIを変えない。

空queryのscore0、operation_available、available.size()の同点位置、in_score_order、名前/読み/鍵の最小score、全返却fieldsと所有、description/short_nameの照合除外、mode別鍵を保つ。OperationTextのviewは不変の静的表、key viewは同期照合中だけで保持しない。候補vectorとscoredは局所値、途中例外で部分結果を公開しない。表/lookup/表示文言/scorekernel/sort/公開signature/キャッシュ/reserve/CommandPaletteの防御copyは不変。三範囲以外の同file全文と他製品sourceが不変であることを照合する。

DisplayTextは有効UTF8を検証後そのまま所有し正規化しない。現在の固定16行は有効で、借用照合は同じbytesを見る。将来不正なname/descriptionを表へ入れた場合、非一致行のparse例外は従来より遅れる。これは現在の有効データの意味変更ではないが限界として保持し、既存--operationsのempty ordinary全16行parseと新独立両mode全fields oracleで不正表を失敗させる。第二のUTF8/表示validatorやcacheを導入してこの差を隠さない。key生成/parseの割当失敗順も不変とは主張せず、外部副作用無しの局所値破棄を守る。

ARC-001/003/004/005/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0023/0032/0060/0061/0062/0078/0079/0081/0082/0091/0100/0111。waiver none。元26候補の採否件数は増やさない。

## 実装前に固定する入力と採否

WT=D:/NeNeNib/worktrees/400-operation-choice、OUT=D:/NeNeNib/outputs/20261011-operation-choice。fixed-inputs.json SHA256 698da4b0ebeafadb6bf3621f3a0bb9b575b8ff0b7aefeb31d699ce060c611b47。親が手書きした16行の全表示/reading/mode別keyと期待順を、製品を呼ばないPython Unicode scalar参照で照合した。製品結果や速度試料から期待を生成していない。表literalのJSON UTF8 SHA256は55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98。metadataUtf8/bytes/FNV/SHA、label/detail codepoints、全fields、全順位、checksumをJSONで固定する。

| operation-choice-以降 | query | 候補数 | metadata bytes / FNV1a64 | checksum | 分類 |
| --- | --- | ---: | --- | ---: | --- |
| reading-ordinary-1024 | ほぞん | 2 | 126 / 9827213030368951609 | 186368 | benefit |
| miss-ordinary-1024 | qqq | 0 | 117 / 10923022879623698261 | 1024 | benefit |
| all-ordinary-256 | (empty) | 16 | 112 / 11167900429985717533 | 441088 | cost |
| all-vim-256 | (empty) | 14 | 102 / 6653010295187654273 | 396544 | cost |
| space-ordinary-256 | (space) | 16 | 115 / 6929459950631445612 | 441088 | cost |
| key-vim-1024 | cTrL+s | 4 | 109 / 16597966209003581918 | 486400 | cost |
| name-ordinary-1024 | タブ | 5 | 120 / 1663071639732138461 | 569344 | cost |
| close-key-vim-1024 | CTRL+F4 | 1 | 116 / 15629853484372669928 | 79872 | cost |
| description-miss-1024 | 上書き | 0 | 126 / 12690603971269174588 | 1024 | cost |
| short-name-miss-1024 | ヘルプ | 0 | 125 / 15438258473254498258 | 1024 | cost |

readingはsave/save_as。key-vimはsave/save_as/toggle_bookmark/recent_tab_back（中二つが同点で表順）。name-ordinaryのタブはclose_tab/new_tab/recent_tab/list_files/recent_tab_back。close-key-vimはclose_tabだけ。space-ordinaryは空queryとは異なり、key無しtoggle_mode→F1→6bytes鍵群の表順→Ctrl+Tab→Ctrl+Shift+S→Ctrl+Shift+Tabとなる。ordinary16/Vim14、Vim undo/redo無し、全fields/コードポイントを区間外に固定literal比較する。

OperationChoiceWorkload閉enum10値を既存typed dispatch/TimingPort/metadataへ追加し、SampleとExpectedは必要なら一型一file。旧138条件の順/定義/入力/期待、47selector/default/unit assertionsを保持し登録148。既存別probe型の用途を広げず、product型へ計測口を作らない。区間内はconst query/modeへのoperation_choicesの固定回数生成・破棄とchecksumだけ。checksumは各callの1+件数に、全行label bytes+label codepoints+detail bytes+detail codepoints+key bytes+operation ordinal+1を足す。空候補でも1を返す。入力/期待生成、全field oracle、保持vectorとlabel/detail/keyのviewの確認、query所有元破棄後の返却所有確認は区間外。入力query/modeも不変を確認する。

計10条件=2利益/8費用。20iterations/warmup1/ABBA3/各120対応組の初回一度だけ（120runs/2400durations/4800marks/1200pairs）。利益は対応比中央値<1・全3cycle比<1・短縮>=90、費用は比<=1.10かつ対応差中央値<=50µs/block。全oracle成功が前提。未達不採用、欠測/外乱保留。除外/補完/合格まで再取得なし。rounds256/1024の局所率を合算/UI全体へ換算しない。controller全体の速度を今回測るとは主張しない。測定中は他build/test/GUI/hash/copyを止める。

## 変更に関係する検証

Debug ASan/UBSan no-recoverの既存--operations（操作表/照合/key/empty全行parse）、--command-palette（唯一の製品callerとoperation_request）、--guide（short_nameを照合しない既存契約）を各一回。実checks数は出力後に記録し事前推定を実結果にしない。新unit0、Release unit build/run0。all/default/無関係なbackground/Ex/Vim oracle/coverageを実行しない。before Release/after正規通常Release/Debug各10oracleを1iteration/warmup1で確認し、性能採否の試料と混同しない。

関連Python登録/不正名/固定metadata/旧138登録保存と、新enumの実compiler missing-case拒否、変更C++format/source-CMake graph/core・application symbols/protected1853/実compile flags・physical source/Git来歴を確認。厳格flags/tidy/no-recover、actual cache/compile_commands/Ninja/File API、全native configure/build/compiler/tidy/test/probe stdout/stderrを初回から保存する。before harnessはclean commit、製品commitは後。通常Releaseはeng/build-release.ps1 -Ref <40hex製品commit>で作り、同buildへprobeだけ追加する。失敗は原版を残し是正の影響が無い成功結果を再利用する。

GUIはfixed6scene: ordinary全件/ほぞん/qqq/空白、Vim全件/cTrL+s。F1入口、mode別key/件数/選択/名前/説明/no-keyを確認する。beforeは#398通常Release製品5cc9727、snapshot-398のexe SHA05090cc9bebf7989c525c4e1795b5b0bbeb4e7ed308d204fe89c16613fe2a8f7を不変参照し、基準mainとの差分が結果文書のみであることを確認する。private D:/NeNeNib/evidence/t400/probe.txtはalpha 日界 omega<CR><LF>alpha 日界 lastの37bytes、private profile/settings/13.5pt/client1280×800/DPI120。helper/plan/inputs/exe SHAを起動前freeze。各caption+前後guardで18対36PNG/36mode/12本文copy/2normalclose、全client RGB無mask、LastInput/foreground/title/closedIME/非被覆grid2560/本文/元bytes・mtime/settings不変を確認し親が全36原PNGを目視する。実checkpoint数は準備に従い固定。caption前に本文copy用Escを入れず、操作をSubmitしない。BMPのみ/可視行の限界と全行literal oracleを分離する。

## 正式記録・統合・整理

canonical files palette5000はlisted_positions側でoperation_choicesを呼ばない。旧#392の一指標（旧5試料中央値2.159ms/上限4.630ms）だけをsource/caller/112compile commands/tool/input/host/power不変で限定reuseする。同一TU layoutの新実行性能は未測であり、新正式測定/正式8全成功/操作UI速度とは呼ばない。CommandChoice.cppの今回三範囲に加え旧#398二箇所を明示して逆照合し、files関連関数と他本文が不変であることを示す。旧CommandLine/ExResultの限定証明も維持する。

hideの現行分担に沿いAstraが設計/採否/排他計測GUI/統合、SOLが実装対象検証と独立別席レビューGUI準備、LUNAが読取レビュー。先行ADR→harness→before→製品を別commit。採用なら日報5〜8行/引き継ぎ/全コマンド/費用/未測/失敗原記録を収載、draft/最終レビュー/Ready/必須CI/正規merge-pr.py plan+同head execute/main clean同期/恒久収載/絶対D targetとdirty/untracked/ignored/唯一成果物/稼働参照/link監査/整理まで行う。branch/commitを保持する。工程/文書/SHAだけで成功検証を繰り返さない。#398はPR399/main72e6863・snapshot398/acceptance398・15:17:16JST監査整理完了。#365/#373/元26候補は不変。hideの停止まで継続する。

## 実施結果

#400 / ADR0112。operation_choicesは利用可能な操作のkeyを一度生成し、静的name/readingとそのkeyを既存scorekernelへ借用する。一致した行だけ従来のDisplayText::parseでname/descriptionを所有し、同じkeyを返却値へmoveする。CommandChoice.cppの三関数以外と他製品source525filesは不変。空query、mode別鍵、全fields、同点順、返却所有を維持し、description/short_nameは従来どおり照合しない。公開API/settings/schema/table/cache/防御copy/正本経路の変更なし、waiver none。

先行設計5cedb47d70d9684aa907740e16f33bc90c636ba3、harness333b4de6a439107a3a5277b8e838a7ffbbccbbf9、製品f8cbda10cc806f4b1f2420fc4c8405bac8688db7。初回2利益/8費用、Debug既存1530checks/30oracle、旧138probe/47selector/default/assertions/1853fixtures保持、GUI18対0px/36PNG・12本文copy・264checkpoint・36mode・2正常close・親全画像目視で2026-10-11 16:07 JSTに技術受理。

queryほぞん（返却2候補）の1024回生成・破棄区間で約51.79%、qqq（返却0候補）の同1024回で約63.17%短縮。全件を含む費用8条件は[gate-proofs 5-dz](../quality/history/5-dz-matched-operation-candidates-own.md)。局所率を合算/UI全体へ換算しない。新正式取得0、旧#392 files palette5000だけをsource/caller/112compile flags/tool/input/host/power不変で限定reuseし、新exeの正式速度/全8成功とはしない。

実統合/収載/整理はwave4/acceptance-400。
