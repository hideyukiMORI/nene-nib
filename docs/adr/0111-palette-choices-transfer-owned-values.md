# ADR 0111 — パレット候補は所有値をコピーせず渡す

- 状態: 設計固定・実測前（2026-10-11）
- Issue: #398
- 基準: main4f6419ebd93c0b634692b6ccf311cd0ab783d966

## 決定

CommandChoice.cppだけの二箇所を変更する。palette_choicesのrange-forはex_command_candidatesが返す一時owning vectorを反復中保持するので、auto commandの要素copyをauto &commandへ変え、一致した要素を既存choice_of(std::move(command))へ渡す。要素をmoveした後に再読しない。choice_ofのconst auto labelをauto labelにし、検証済みDisplayTextをCommandChoiceへstd::move(label)で渡す。DisplayTextのstring/countは暗黙moveで所有が渡る。kindはcommandをmoveする前に決める。

候補列挙の正本、score/sort/同点順、DisplayText::parse、fill/execute、失敗custom名、公開API/返却型、input/catalog、設定/schemaは不変。新helper/cache/reserve/別列挙経路は作らない。direct/fallbackのinitializer_list返却copyを含む他のcopyは残し、全copyが消えると主張しない。二箇所以外のCommandChoice.cpp全文と他製品sourceの不変を証明する。

ARC-001/003/004/005/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0023/0032/0060/0061/0062/0066/0067/0075/0081/0082/0091/0109/0110。waiver none。C2の追加評価で元26候補の採否件数は増やさない。

## 実装前に固定する入力と採否

WT=D:/NeNeNib/worktrees/398-palette-choice、OUT=D:/NeNeNib/outputs/20261011-palette-choice。fixed-inputs.json SHA256 c2c929f74c89ab0d964cc4eea2c74f8b13af499bb04c707838dc909f73502d1f。旧#394の独立名前literalだけを再利用し、空queryのbyte長/辞書順から全36/292行を独立に確定した。製品結果や性能試料から期待値を作っていない。全文literal、期待kind/codepoints/他全fields、metadataUtf8/bytes/FNV/SHA/checksumはJSONに保持する。

| palette-choice-以降 | raw input / catalog | rounds | 全候補数 | metadata bytes / FNV1a64 | checksum | 分類 |
| --- | --- | ---: | ---: | --- | ---: | --- |
| builtin-all-512 | : / builtins | 512 | 36 | 140 / 5339322669749484422 | 677888 | benefit |
| custom-all-128 | : / custom256失敗 | 128 | 292 | 2714 / 18190280579141497665 | 2266624 | benefit |
| miss-1024 | :? / builtins | 1024 | 0 | 135 / 17264397353492223643 | 1024 | cost |
| short-1024 | :fz / builtins | 1024 | 1 | 137 / 4682680893133025120 | 41984 | cost |
| theme-512 | :DRAC / builtins | 512 | 1 | 138 / 2863211271999336340 | 30208 | cost |
| direct-fill-1024 | :set guifont= / builtins | 1024 | 1 | 153 / 11307889127795840306 | 38912 | cost |
| direct-long-1024 | :set guifont= + 日79個 + AB:h18 / builtins | 1024 | 1 | 396 / 16642808051819215475 | 623616 | cost |
| fallback-long-512 | :colorscheme + 空白 + 日81個 / builtins | 512 | 1 | 397 / 6745060132669727186 | 309760 | cost |
| custom-narrow-128 | :user-0123 / custom256失敗 | 128 | 1 | 2726 / 1215178052096014269 | 8320 | cost |

custom名はuser-0000〜0255、全recordがThemeFailure::invalid_color。builtins9名の後にcustom名256を持ち、systemは候補側に追加される。long rawは両方256UTF8bytes、colon除外後label/commandは255bytes、directは97codepoints/fallbackは93codepointsでkind execute。direct-fillはset guifont=/fill、fzはset fontsize=/fill、DRACはcolorscheme dracula/execute、custom-narrowはcolorscheme user-0123/execute。GUIで設定文字列をSubmitしない。

新PaletteChoiceWorkloadは閉enum9値、Sampleと必要な期待値型は1型1ファイルで既存typed dispatch/TimingPort/metadataへ接続する。旧129条件の定義/順/入力/期待、47selector/default/unit assertionsを保持し登録138とする。準備/insert/catalog構築/期待生成/oracleは区間外。区間内は同じconst CommandLineに対するpalette_choicesの固定回数の生成・破棄とchecksumだけ。checksumは各callの1+件数+全command bytes+全label bytes+label codepoint countを加算する。

独立oracleは全候補順、command/label/codepoint count/kind、detail/origin/operation無し/key空、元input/caret/index/catalogの全名・全失敗record、保持した別結果/vector/string_viewを区間外確認する。生成元のCommandLineとlocal catalogの寿命を終えた後も返却値が所有を保つことを区間外に確認する。期待順は製品関数で生成せず固定literalを使う。

旧command-input-controller-fill-64（入力「:fz」/3bytes、FNV11941784898562763057、checksum2304）の定義は一切変えず費用条件として新たな前後比較に含める。Submit/Cancel/OpenPalette/CommandText(:fz)のapply/deliveryとrequest-vector成長を64cycle含み、fill単独の時間とはしない。CommandText→refiltered→result_of→palette_choicesが区間内の直接caller。旧Ex frame3はprecomputed rowsを複製するだけで今回の関数を再生成しないため除外する。

計10条件=2利益/8費用。20iterations/ABBA3/各120対応組を初回一度だけ取得（120runs/2400durations/4800marks/1200pairs）。利益は対応比中央値<1・全3cycle比<1・短縮>=90、費用は比<=1.10かつ対応差中央値<=50µs/固定区間。全oracle成功が前提。未達不採用、欠測/外乱保留。除外/補完/合格までの再取得なし。異なるroundsを混同せず局所率をUI全体へ換算/加算しない。測定中は他build/test/GUI/hash/copyを止める。

## 変更に関係する検証

Debug ASan/UBSan/no-recoverで既存--command-palette495/--user-theme-selection97の592checksを各1回。順位/fill/入力・本文保持、直接設定、未知theme、custom成功/失敗名の直接契約を確認する。Ex単独は今回のcallerでないので--ex-settingsは対象外。新unit0、Release unit build/run0、全件回帰/coverage/Vim oracle再生成なし。

新9pure+旧fill1をbefore Release/after通常Release/Debug各1iteration/warmup1で30oracle確認（速度採否とは区別）。新Python登録/固定入力/不正名と旧登録、新enum漏れのcompiler拒否、差分format/source/CMake graph/core・application symbols/protected1853/来歴を確認する。厳格flags/tidy/生成flags、source archive/Gitとphysical SHA、全native configure/build/compiler/tidy/unit/probe stdout/stderr、File API query/repliesを初回から保存する。通常Releaseはeng/build-release.ps1の製品40hexRefで生成し同buildへprobeだけ追加する。

GUI beforeは#396通常Release（製品1ec1264、SHA2c1401d0f87a316c044fbaa87f77332557a4759f42e55cbcd2accf124ba482d7）をsnapshot-396から不変参照、基準mainとの製品/compile環境同一を確認する。private D:/NeNeNib/evidence/t398/probe.txt、本文alpha 日界 omega<CR><LF>alpha 日界 lastの37bytes、private profile/settings/13.5pt、client1280×800。全7sceneはcommands paletteのall/fz/miss/DRAC/direct-fill/direct-long/fallback-long。入力計画/キー列/helper/exe/input SHAを起動前freeze。全client RGB無mask、LastInput/foreground/title/closedIME/非被覆2560点、各caption前後guard、本文コピー/元bytes・mtime/設定不変/正常closeを確認し、全21対42PNGを親が目視する。captionを本文copy用Escより先に取得する。可視行/長文字列clipと全36/292順位・255bytes所有のcore oracleを区別する。BMPのみ、外部IME/4byteUTF8新検証とはしない。

## 正式記録・統合・整理

canonical files palette5000はlisted_positions側でpalette_choices/choice_ofを呼ばない。QLT-001/012とADR0021に従い、旧#392一指標（旧5試料中央値2.159ms/上限4.630ms）だけを関連source/caller/112compile commands/tool/input/host/power不変の証明により限定reuseする。旧c6からの製品差分はExResult.cpp/CommandLine.cpp/CommandChoice.cppに限定し、CommandLineとCommandChoiceの指定箇所以外全文、files側関数は不変と照合する。新正式測定/正式8全成功/commands UI速度と呼ばない。基準/閾値/観測器を変更しない。

hideの現在の明示分担に従いAstraが設計・採否・排他測定/GUI/統合、SOLが実装/対象検証と別席のGUI準備・レビュー、LUNAが独立読取。先行ADR→harness→before→製品を別commit。採用時はdraft PR/最終独立レビュー/Ready/必須CI/eng/merge-pr.py planと同head execute/main同期/恒久収載/絶対D path・取込・未保存/未追跡/無視・唯一成果・稼働/link監査整理まで完了する。branch/commit保持。全試行/費用/未測/限界を日報・引き継ぎへ残し、成功結果を工程/文書/SHAだけで再実行しない。元26候補、C8/R8/IO6〜IO8、#365/#373の処分不変。hideの停止まで継続する。
