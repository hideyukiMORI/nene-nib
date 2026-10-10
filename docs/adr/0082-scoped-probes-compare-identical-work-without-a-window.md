# ADR 0082 — 同じ固定処理を窓なしで前後比較する

- 状態: 受理（親設計サナの設計・2026-10-09）
- 日付: 2026-10-09
- Issue: #329
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-007 / CPP-005 / CPP-007 / CPP-012 / CPP-016 / QLT-001 / QLT-012 / QLT-013 / QLT-014

## 文脈

#320〜#324 の閉じた処理の効果は、窓と描画全体の測定だけでは分離しにくい。
hide は他の作業がある機械でも、規約の範囲で固定入力の対応前後を比較するよう指定した。
正式な速度のゲートと異なる意味を持つ比較器を、製品に未検証の生成口や時計を足さずに用意する。

## 決定

1. 開発用 target `nib_perf_probes` を `tests/performance/`、module `performance_tests` に置く。
   依存は core / application / adapters_win32 / unit_tests。既存 `Editing` と Scripted ports はヘッダだけを借用し、unit の実行 target を link したり cpp を再コンパイルしたりしない。
   `eng/architecture.json` と `docs/PROJECT_LAYOUT.md` にこの境界を記録する。製品の依存と既存 unit の純粋さは変えない。CTest の既定には登録しない。
2. 時刻と測定ファイルの出力は既存 `Win32TimingAdapter` だけを使う。
   `Milestone` に `probe_started` / `probe_finished` を足し、網羅する名前の分岐も更新する。
   既存の入力と描画の節目を別の意味に転用しない。出力先は argv から共通 `to_utf16` を通して bind に渡す。test / core / application は時計やファイルを読まない。
3. 最初の固定 workload は四つ: `controller-open-utf8-16mib`（200000 CRLF 行・16800000 bytes）、`buffer-from-utf8-16mib`（同じ本文）、`controller-insert-200`（通常の新規文書・VisibleLines 30・200回のInsertTextと戻りframe生成）、`display-line-long`（既存長行ベンチの日本語文の90反復を含む1行）。
   開く場面は毎回新しい Editing と ScriptedFiles を準備し、既存タブの no-op を測らない。現在 main と #320 / #321 で共通の API だけを呼ぶ。後続用だけの API は足さず、旧アルゴリズムもコピーしない。
4. 入力生成・ポート準備・期待値との照合・checksum は区間外。処理の戻り値を保持して必ず観測する。
   controller の全文確認は区間後の既存 SaveDocument と ScriptedFiles の記録で行う。
   反復数は CLI で事前確定（既定20）。warmup を bind 前に一回実行し集計から外す。各反復に開始/終了を対応させ、4096 marks の上限を越える入力を拒否する。誤った本文・行数・選択などは非0で終わる。
5. `eng/compare-probes.py` は before / after exe・ref・commit・workload・output・反復数を明示して使う。
   同じ設定の ABBA ブロックを既定3回（12 process・各内部20 sample）実行する。subprocess は窓を出さず、各 process の測定先は独立させる。
   全値と対応する ratio・中央値・range を保存する。途中で順や試行数を変えず、外れ値除外もしない。欠測・timeout・失敗終了・不正 JSON / marks・checksum 不一致は判定不能として非0で返す。
6. exe の SHA256、ソース ref / commit、build 時の compiler / configuration、入力 bytes と実際の入力の FNV-1a64、反復数・順序・warmup・出力 checksum を記録する。
   checksum の一致は性能の合格を意味しない。独自の基準値・自動調整・成功までの再試行は作らない。
7. 同じ harness を main と候補に載せて比較する。#329 は main 起点で完結し、候補への取り込みと長い実測は親が行う。
   開発 target の Debug / Release は固定 `eng/toolchain.ps1` と同じ CMake 警告・静的解析を使い、target だけ並列2で build する。
   製品 exe 専用の `eng/build-release.ps1` は使わず、この開発 target の CMake Release build を製品 Release と呼ばない。
8. #323 の前計測のため、親設計席の追加決定で `controller-insert-200-after-delete-1mib` / `controller-insert-200-after-delete-16mib` を同じ初回 harness に含める。
   1MiB は84 byte行を12483行（1048572 bytes・約1MiB）、16MiB は既存の200000行（16800000 bytes・約16MiB）を使い、実 bytes と入力 hash を必ず記録する。
   毎回新しい Editing で OpenDocument → SelectAll → DeleteText を区間外に行い、frame と保存で空本文を確かめる。履歴を残して200 InsertText のみを計測する。
   最終本文は200文字、Undoで空、もう一度Undoで元の全文、Redo二回で200文字へ戻ることを区間外の保存と hash で確認する。正式 GUI の新ベンチや基準値採用の代用にしない。
9. #330 の ASCII 検証候補を比較するため、最終追加として `utf8-validate-ascii-16mib` / `utf8-validate-japanese-6mib` を含め、初期 scope を八つで固定する。
   ASCII は既存の16800000bytesを再利用し、期待 codepoint 数も16800000。日本語は既存の長行6217bytesとCRLFを1000行（6219000bytes・2079000 codepoints）生成する。
   `validate_utf8` だけを計測し、期待countとchecksumの確認・生成・hashは区間外。非ASCIIへの悪化を直接見るための二本で、長い比較は親が行う。

## 初期8本の固定batch完了後の第2段階（Issue #323）

初期8本の定義・入力・区間・期待値・batch結果を固定したまま、Vimの登録本文と録画/ドット記録のコピーコストを見る4本を追加する。製品source/APIと所有実装には触れず、同じharness変更をbefore/candidate双方へ取り込む。

- `controller-vim-insert-200-register-1mib`: 入力はASCII `r`の1048576bytes。
- `controller-vim-insert-200-register-16mib`: 入力はASCII `r`の16777216bytes。
- `controller-vim-record-insert-200`: 入力はASCII `x`の200bytes。
- `controller-vim-record-insert-2000`: 入力はASCII `x`の2000bytes。

登録2本は各反復で新しいEditing、VisibleLines30、Vim選択、StoreVimRegisterでaへ文字単位入力を置き、空本文でiへ入る。区間内は200個のVimKeyPress{x}をprimary applyへ流し、最後の所有反映値を保持する。区間外で失敗無し・INSERT・1行・caret列201・保存本文x200を確認する。Esc→名指しa→pでx200+入力の全文を保存照合し、undoでx200へ戻ることも確認する。checksumは確認済みx200と貼付後の全文を含む。レジスタのLFのみの契約に従い、既存CRLF入力を転用しない。

録画2本は各反復で新しい空Editing、VisibleLines30、Vim選択、q a iを区間外に与える。区間内は入力byte数と同数のVimKeyPress{x}をprimary applyへ流す。区間外でINSERT・caret列count+1・保存全文を確認し、Esc→qで録画を止める。uで空に戻し、@aを同じ既存キー入口から再生して入力と同じ全文を照合する。checksumへ確認済み全文を含む。

1warmupは既存のまま。準備・照合・undo・録画停止・再生は区間外。閉じたenum/name/input/runとPythonの固定生成式を網羅更新する。新4本だけDebug/Release各warmup1+sample1で動作確認し、metadata/marks/固定input/hash/checksumを照合する。旧8本や失敗検出の期待値は変更しない。長い比較と性能受理は親が行い、短い実行を正式速度ゲートの合格と扱わない。

製品2dbd106と旧比較器a2ea9ceはDの保管先へコピーしSHAを確認してから新targetをbuildする。製品targetを再buildしない。harness変更をclean commitし、既存Debug/Release builddirへ明示configureしてbuild metadataを更新する。

## 検証と限界

道具の対象試験で、短い自己比較・壊れた marks・片側の失敗・checksum の違いを確認する。
変更 target の Debug / Release、対象の規約・依存・整形・シンボルを確認する。実機窓・正式 speed・全件回帰・coverage・oracle は実装席では実行しない。
この比較は UI / GPU / 実ファイル I/O の遅延を測らず、正式 QLT-014 を置き換えない。小さい差だけで採用を決めない。既存 perf-reference と許容は変えない。

## 第3段階 — buffer / palette / 入力変換の固定12本（Issue #337）

親設計席が2026-10-09の実装briefで受理した固定定義。旧12本の関数本体・入力・期待値を保持し、同じrun_workload・TimingPort・metadata・比較器に追加する。製品source・toolchain・flags・allowlist・fixture・正式基準値は変更しない。

### 新12固定処理（命名は下記固定）

1. buffer-line-text-scattered-crlf-4096: (a×78+CRLF)×4096。外でi=0..4095の80*i+1をbへ1byte置換。実piece_count、準備後本文、4097行を外で照合。計測は行1..30のline_textを一巡し事前用意array<string,30>へ格納。全戻り値の破棄/hash/検証は外。期待は各78bytes=a+b+a×76。
2. buffer-line-text-scattered-lf-4096: 同じLF版、stride79。孤立CRを混ぜず改行違いの退行区間として別名。
3. buffer-position-long-utf8-57344: a日本語🖋×4096。1piece準備。position_of(Offset{57344})を128回、事前用意array<TextPosition,128>へ。すべてline1/column20481。cp数を候補の位置計数関数で期待生成しない。
4. buffer-position-scattered-utf8-57344: 同じinputの14*i先頭aをbへ外で置換してfragmented snapshot。期待bytes/count/positionは上と同じ。準備後本文hashを外で確認し、入力hashは元inputと固定準備規則をmetadata/文書で対応づける。
5. palette-listed-name-5000: 5000 entries、label=ENTRY_<5桁i>_ABCDEFGHIJKLMNOPQRSTUV.txt、detail=D:\ZROOT\LONG_DIRECTORY_COMPONENT\GROUP_00、folder/open、query=entry、scope=files。listed_positions一回だけ計測、位置0..4999全件同順を外で検証。
6. palette-listed-location-5000: 上のquery=zroot。名前にZは無い。全5000を場所一致させ同じ期待順。
7. palette-append-narrow-5000-to-50: detail無しfolder/open 5000entries。i<50 QX_Y_<5桁i>_file.txt、i<500 QX_N_<5桁i>_file.txt、残りAA_N_<5桁i>_file.txt。opened(input=qx)で500件を外で準備。inserted(y)一回を計測、50件/位置0..49/input=qxy/selected0を全確認。
8. palette-caret-left-5000: 上と同じopened(qx).selected_at(7)。edited(left)一回を計測、500件同順/input=qx/caret byte1/selected0。shares_result_withを共通期待に使わない。
9. codepage-to-utf8-cp932-japanese-16mib: bytes93 FA 96 7B×4194304、16,777,216bytesを準備。実Win32CodePageAdapter::to_utf8一回のみ計測、期待UTF8日本×4194304（25,165,824bytes）を外で完全照合/hash。台本CodePagePortは不可。
10. utf16-to-utf8-japanese-8m-units: 日本×4194304（8,388,608units）を外で作りcore::to_utf8一回。結果は上の期待と同じ。
11. utf16-to-utf8-ascii-8m-units: L'a'×8,388,608units、期待a同数。
12. utf16-to-utf8-supplementary-8m-units: U+1F58Bのsurrogate pair×4,194,304（8,388,608units）、期待UTF8の🖋×同数（16,777,216bytes）。

UTF16 inputHashはWindowsUTF16LEの標準byte列をinput_ofから生成し、run側で明示unitへ構成する。host wchar_t memoryを雑にreinterpretしてhashしない。palette inputHashはquery/scopeと全entryのlabel/detail/識別値を含む固定直列化。タブ/改行を含まないこの固定fixtureなら明示delimiterで十分、run側は同じinput bytesからentriesを組み立てる。実際に使わない説明文だけをinputHashにしない。

すべて入力生成/編集準備/候補生成/結果容器準備は計測の前。結果値を外へ保持、完全照合/結果hash/破棄はprobe_finishedの後。計測中のstring/vector内の必須allocationはその処理の費用として含める。毎反復同じ準備状態から開始。既存のchecksum/2marks/metadataを共用。準備費用が大きくても既存harnessの区間を勝手に広げない。正しさsmoke時に全runの壁時計も参考として報告し、親が最終固定反復数を事前決定する。


paletteの直列化はUTF-8で、headerは `files<TAB>query<LF>`、各entryは `folder<TAB>open<TAB>probe-<5桁i><TAB>label<TAB>detail<LF>` の順。detail無しは空欄。実際のdetail表示値は `D:\ZROOT\LONG_DIRECTORY_COMPONENT\GROUP_00`（各区切り1 backslash）。runはこの直列化のqueryと全entryから値を構築する。識別値は0〜4999で一意、operation無し・key空を固定する。UTF16LEは明示したunitから下位byte・上位byteの順で生成し、runも2byteからunitを復元する。

準備後本文を元入力から独立した固定期待列と完全照合し、fragmented行入力は8193 pieces、fragmented UTF8入力は8192 pieces、連続UTF8入力は1 pieceを確認する。piece数は準備の観測で、結果checksumには含めない。計測前に戻り値の容器を準備し、終了markの後に全文・順序・位置とchecksumを確認する。

新12本のみDebug/Release各warmup1＋sample1で正しさを確認する。比較器の対象試験、File API依存、symbols core/application、差分整形・保護対象・旧12のGit一致を確認する。clean commit後の明示Release configureでmetadataを固定する。製品exeはbuild/起動せず、製品source baseはeef5aad。CLI全体の参考壁時計を残し、固定反復数・長い性能比較・採否・独立レビュー・統合は親が担当する。新しい正式速度ゲートや合否閾値を設けない。

## 第4段階 — 保存・erase・offset_of・検索の固定17本（Issue #346）

親設計席が2026-10-10の実装briefで受理した決定。既存24本は不変。同じmarks・metadata・warmup・完全照合と比較器を使用し、製品API/flags/allowlist/正式基準値を変更しない。

#### 固定17workload

#### 保存2本

- controller-save-utf8-16mib
- controller-save-utf8-bom-16mib

入力は既存controller-open-utf8-16mibと同じ16800000bytesの200000 CRLF行（BOM版もinput bytesは同じBOM無し本文）。各反復fresh EditingでVisibleLines30、ScriptedFiles.hold(input)、OpenDocumentを区間外に行う。区間内はSaveDocument{同じprobe_path,utf8又はutf8_bom}のprimary apply一回のみ。戻りEditorUpdateとwrittenは終了mark後まで保持。計測は内部encoded単体ではなくtext()/encoded/ScriptedFiles::writeのcopy/状態更新を含む保存経路、frame生成は区間外。外でfailureなし・encoding一致・dirtyでない・名前同じ・本文/改行不変を確認。writtenはUTF8=input、BOM=EF BB BF+inputの全byte一致を独立に確認。checksumは実written全文。変換失敗/実file原子性は製品unit/実機側で確認し、このprobeの速度区間へ混ぜない。

#### collect/erase5本

- buffer-erase-scattered-head-4096
- buffer-erase-scattered-middle-4096
- buffer-erase-scattered-tail-4096
- buffer-erase-scattered-all-4096
- buffer-erase-single-middle-4096

inputは既存crlf_buffer_input=(a*78+CRLF)*4096、327680bytes。scatteredはi=0..4095の80*i+1をbに1byte置換して8193pieceを準備。singleは1pieceの元inputをそのまま使う。prepared全文/piece_count/4097linesを区間外で照合。
位置: head=0、middle=163840、tail=327677（最後の行の末尾a）、all=[0,327680)。head/middle/tail/singleは指定位置の1byteだけ削除。区間前にvector<TextBuffer>をreserve(16)し、同じ不変なprepared sourceへeraseを16回、結果をemplaceして保持する区間だけを計時。全16結果のtext/size/line_count/line_endingと旧snapshot全文不変は終了mark後に確認・checksum化・破棄する。1byte削除期待はpreparedのstd::stringを指定位置でeraseしたもの（製品eraseを期待値生成に使わない）。allの期待は空/1line/CRLF、ほかは4097lines/CRLF。metadataのinputHashは元input、準備規則とpreparedHash/piece数をstderrへ出す。input生成/準備/期待/vector reserveは区間外。

#### offset_of4本

- buffer-offset-long-head-57344
- buffer-offset-long-middle-57344
- buffer-offset-long-end-57344
- buffer-offset-scattered-middle-57344

input=(a日本語🖋)*4096、57344bytes/20480codepoints/1line。scatteredだけ既存の14*i先頭a→b置換で8192pieces、期待本文=(b日本語🖋)*4096。head column2→byte1、middle column10241→byte28672、end column20481→byte57344。scatteredはmiddleと同じ期待。各反復でfresh準備し区間前にarray<Offset,128>を用意、同じTextPosition{line1,固定column}へのoffset_ofを128回実行して格納。完全一致とchecksum、準備の本文/piece数/line_countは外で確認。column0/1/範囲外/UTF8途中pieceは製品unitが担当。

#### 検索6本

- search-forward-head-many-4096
- search-forward-middle-many-4096
- search-forward-tail-many-4096
- search-backward-head-many-4096
- search-backward-middle-many-4096
- search-backward-tail-many-4096

input=(a日本語🖋 + 半角space)*4096、61440bytes/24576codepoints/1line、1piece。patternはaをforward separatorでparseし区間外に保持。要求のdirectionは名前どおり、count=1、from line1、columnはhead1/middle12289/tail24571（0始まり一致i=0/2048/4095）。区間前array<expected<VimSearchHit,VimSearchNoticeKind>,16>を用意（構築が難しければreserve済vectorへemplaceでよい）、vim_find_match16回を区間内で保持。外の期待: forward head7/no wrap、middle12295/no wrap、tail1/wrap。backward head24571/wrap、middle12283/no wrap、tail24565/no wrap。全戻り値/旧本文不変を外で確認し位置とwrappedをchecksumへ含める。準備/parse/結果照合は外。count/greedy/empty等は製品対象unitで守る。


新17本だけDebug/Release各warmup1+sample1で検証する。長いABBA・GUI・製品Releaseは親設計席が担当する。結果容器の破棄は終了mark後、0usは補正せず正しさsmokeと比較の分解能判定を区別する。Waivers: none。
## 第5段階 — VimPattern::matchedの固定7本とdispatchの正本表（Issue #350）

親設計席が2026-10-10のbriefで受理した決定。#346のref/exe/SHA/rawを固定保管してから別branchで実装する。

### 固定入力・区間・期待

旧41本の入力/処理本体/引数/区間/期待値/結果を変更せず、次の7本を追加。製品source/flags/基準値/抑制/allowlistは変更しない。VimPattern::parseは区間外、patternを保持。fromはすべて0。区間前array<optional<VimPatternMatch>,16>を用意し、16回matched(input,0)を格納する区間のみ計時。全16戻り値を区間外でpresence/begin/end完全照合。checksumはchecksum_of(input)へ各結果のpresence+begin+endを加算（一致無しは0を加算）、unsigned既存型。pattern文字列の独立FNVはstderrへ出す。inputHashは実本文のbyte列。

| name | input | pattern | expected |
| --- | --- | --- | --- |
| pattern-star-miss-1024 | ASCII a×1024 | a*b | 不一致 |
| pattern-star-miss-2048 | ASCII a×2048 | a*b | 不一致 |
| pattern-star-miss-4096 | ASCII a×4096 | a*b | 不一致 |
| pattern-multistar-miss-32 | ASCII a×32 | a*a*a*a*b | 不一致 |
| pattern-greedy-hit-4096 | ASCII a×4096 | a.*a | begin0/end4096 |
| pattern-literal-tail-4102 | ASCII x×4096 + needle | needle | begin4096/end4102 |
| pattern-literal-long-4096 | ASCII a×4096 | ASCII a×4096 | begin0/end4096 |

入力生成/parse/array準備/全照合/checksum/破棄は外。prepared stateを再利用するのは同じmatched呼出し内16回だけ、workload毎反復はfresh parse。新7本Debug/Release各warmup1+sample1で完全照合。比較器の固定定義Python対象試験と旧41静的対応一致を記録。新しい基準値/合否閾値を作らない。長いABBA/GUIは親。

### dispatchの構造

CPP012上限により型付き正本1表へ移す案を採用。ProbeDispatch.hppの1aggregateにenum/name/input factory/runnerを持ち、名前も同じrowへ集約。factory (ProbeWorkload)、runner (ProbeWorkload,const string&,TimingPort&)で統一。既存signatureの薄いprivate wrapperだけ追加し、本体/引数不変。constexpr検査でrow index==enum underlyingと重複/欠落、表sizeを検査。

**閉じたenumのcompiler網羅性は残す。** 配列size/最終enumのstatic_assertだけではenum末尾追加を検出できない。private checked_index(ProbeWorkload)に48個のcaseを全部列挙し、全caseを1つのreturn static_cast<size_t>(workload)へ集約、switch外std::unreachable、default/else無し。input/runはこの1本を経て表へアクセスする。48case+1returnなら通常整形で60行内に収まる見込み。規約の抑制や型のfake count enumeratorは足さない。実際に上限を越える場合は実装前に相談。

## 行内spanの固定比較（2026-10-10・#356、製品#355より先に固定）

main163c314の照合器を前後で共通に使い、`EditorController::frame()`を一回だけ`probe_started/finished`で囲む。入力生成、ScriptedFilesからのopen、VisibleLines{1}、mode/検索/選択の準備、全戻り値照合と破棄は区間外。公開applyを使い準備のframe生成を省く。反復数/ABBA順/timeoutは短い正しさsmokeの費用を見た後、比較実行より先にplanへ固定する。harnessの入力/区間/期待はこの時点で固定し、候補結果へ合わせない。

| 固定名 | UTF8本文 | 一致と選択（1始まり原文桁） |
| --- | --- | --- |
| frame-search-dense-ascii-8192 | `a x `を8192回、32768bytes | aの8192面、i番目[4i+1,4i+2)、caret1、current[1,2)、選択absent |
| frame-search-dense-mixed-4096 | `日a\t🖋\x01 `を4096回、45056bytes | aの4096面、[6i+2,6i+3)、caret2、current[2,3)、選択absent |
| frame-search-sparse-tail-32768 | xを32767回+a、32768bytes | 末尾の1面[32768,32769)、caret32768、current同じ、選択absent |
| frame-selection-ascii-32768 | 1本目と同じ | 通常mode、PlaceCaret8193 collapse→24577 extend、選択[8193,24577)、caret24577、matches/currentなし |
| frame-search-visual-ascii-8192 | 1本目と同じ | aの8192面、検索確定後caret8193、v→16384l、VISUAL選択[8193,24578)、caret24577、current[24577,24578) |

検索はSelectEditMode(vim)→VimKeyPress(VimSearchPattern{"a",forward,nullopt})で確定し、PlaceCaret collapseで指定caretへ置く。VISUALだけはその後にvと数字16384とlを通常のVimKeyPressで送り、inclusive終端を確認する。windowやfixture入力行parserは使わない。全条件line.number/first_visible/total_lines/lines.sizeは1、command_line/compositionはなし。mode/visualとcaret位置、原本文、全SelectionSpan、matchesの順と全端点、currentの有無/全端点を完全照合する。

ASCII displayは原本文と同じ、startsは0..32768。混合displayは`日a\t🖋^A `を4096回（49152bytes/28672codepoints）、startsは各unitの7i+[0,1,2,3,4,6]と最後28672（24577要素）。Tab/emojiの原文桁は各1、制御文字だけ表示幅2となる。display本文/全startsも独立期待と照合し、期待をframe自身や候補helperから生成しない。

checksumはuint64の剰余和でFNV(input)+FNV(display)+全startsの和+全match(begin+end)の和+present選択/現在面の両端の和+4（line.number/first_visible/total_lines/lines.size）+caret.line+caret.column+matches.size+選択presence(0/1)+current有無(0/1)。上から`4762729787238942292`、`18084650234477590103`、`13493547337114145917`、`4762729786970564179`、`4762729787239048792`。独立Pythonの入力/期待算術をDの事前計画へ保存。完全照合が正本でchecksumだけを正しさの根拠にしない。

旧48workloadとregistry/input/factory/runnerの意味を保持し、5本だけ追加する。新5のDebug/通常Release各1iteration+warmup1の短い完全照合を行う。通常flags/TimingPort/metadata/checker不変。全初回失敗/未観測を保持し、正式速度やGUIの利益へ直接転用しない。規則ARC-001/007/011、CPP-002/005/012/016、QLT-001/012/014。
