# ADR 0107 — タブ表示は省略後の範囲だけを一時所有する

- 状態: 技術受理（固定利益・費用・正しさ・表示・正式指標を確認）
- 日付: 2026-10-11
- Issue: #388
- 基準: main 6536c9681a5b911a1cf8e69ab6cfe193721fa23e

## 問題と決定

TabTitle.cppは題名を全連結し、場所も全コピーしてから256bytesへ省略する。その後DisplayText::parseが検証して所有する。表示しないsuffixのコピーと、prefixなしで収まる値の一時所有を省く。

private clippedを`DisplayText clipped(std::string_view text, std::string_view prefix)`へ置換する。prefixは既存mark_ofの空または「● 」の4bytesだけで、任意の外部入力を受けるAPIへ一般化しない。

1. `text.size() <= DisplayText::maximum_bytes - prefix.size()`で収容可否を決める。FilePathに長さ上限はないため、全長の加算で判定しない。
2. prefixが空で収まれば同じDisplayText::parseへtextを借用して渡す。parseが所有するため借用は戻り値へ残らない。
3. 省略時は元textの位置`maximum_bytes - ellipsis.size() - prefix.size()`を既存is_boundary/previous_code_pointで戻す。prefixは完全なUTF-8固定値なので、旧連結後の位置からprefix長を引いた位置と同じになる。
4. 組み立てる場合はprefix・残す本文・必要なellipsisの合計（最大256bytes）だけreserveし連結し、同じparseで検証・所有する。短いmodifiedにも同じ組立経路を使う。
5. tab_title_forとtab_folder_forをこの一つのhelperへ移し、旧owned clippingを同時に除く。name_of/mark_of/folder切出し/optionalの意味は維持する。

製品変更はsrc/core/TabTitle.cppだけ。公開API・DisplayText/Utf8/FilePath/SaveState・本文・設定・保存schema・cache・正規化・表示内容は変更しない。所有検証を省くfactoryや第二の解析器を増やさない。

## 規則と直接の影響

ARC-001/004/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0010/0021/0057/0082、waiver none。直接callerは文書の表示値・タブ帯・一覧の題名と場所。ファイル操作や検索の意味は対象外。

## 固定入力と採否

停止前の入力草案を再開時に原本/保全先の全SHAで照合した。`D:/NeNeNib/outputs/20261011-tab-display/fixed-inputs.json`のSHA256は24e3b2b7d9f301f8d82651e6495e15bdf6f4918757b851961c51790e2b10fbc4。以下を性能取得前に固定し、既存比較器へ追加する。

| workloadのtab-display-以降 | 入力 | 期待title | 期待folder | checksum | 分類 |
| --- | --- | --- | --- | ---: | --- |
| ordinary-saved-1024 | `C:\Users\hide\Documents\NeNeNib\projects\editor\` + a×64 + `.txt` | a×64 + `.txt` | `C:\Users\hide\Documents\NeNeNib\projects\editor` | 236544 | benefit |
| deep-saved-1024 | `C:\` + `segment\`×128 + `note.txt` | `note.txt` | `C:\` + `segment\`×31 + `se…` | 539648 | benefit |
| wide-saved-1024 | `C:\work\` + `日本語`×64 + `.txt` | `日本語`×28 + `…` | `C:\work` | 363520 | benefit |
| short-saved-1024 | `C:\n.txt` | `n.txt` | `C:` | 15360 | cost |
| exact-saved-1024 | a×256 | 入力と同じ | 無し | 524288 | cost |
| modified-boundary-1024 | a×247 + `🖋.txt`、modified | `● ` + a×247 + `…` | 無し | 516096 | cost |
| folder-wide-1024 | `C:/` + `資料/`×64 + `note.txt` | `note.txt` | `C:/` + `資料/`×35 + `資…` | 390144 | cost |
| root-saved-1024 | `C:\` | `C:\` | `C:` | 11264 | cost |
| untitled-modified-1024 | 経路無し、modified | `● 無題` | 無し | 14336 | cost |

新TabDisplayWorkloadは閉enum9値。旧89workloadの順/入力/本体/期待値を維持し、同じtyped dispatch/TimingPort/metadataへ追加する。既存登録総数の検査は追加分だけ更新し、旧sliceと期待値は保持する。

区間内はtab_title_for+tab_folder_forの生成・破棄1024回と、title/folderのbytes数+code point数+folder有無のchecksum。入力生成/parse/独立literal期待/全bytes/code point/optional/元path照合は区間外。区間前の所有結果を保持し、後でも値が変わらないことを確認する。

既存frame-document-short-saved-256 / long-saved-256 / long-failed-256 / untitled-256を直接callerの費用4条件として含め、定義を変更しない。全13条件を同一harnessのbefore/after、20iterations、ABBA3、各120対応組で一度取得する。利益3条件は対応比中央値<1・全3cycle比<1・短縮>=90/120。費用10条件は比<=1.10かつ差中央値<=50µs/固定区間。全oracle成功が前提。未達は不採用、外乱/欠測は保留。除外・補完・合格までの再取得は行わず、全rawを保存する。局所率をUI全体の率へ換算しない。

## 差分に必要な検証

- 既存--file-path/tabs/command-paletteの806checksは省略文字列・候補表示・タブ切替の直接境界として一度確認する。既存assertion/47selector/defaultは維持する。
- 収容境界の変更を守るためCoreTestsのタブ題名検証へ追加する。modifiedのASCII252/253bytes、savedの257bytes、savedのa×250+🖋+.txt、modifiedのa×248+🖋+.txt、savedのa×250+日+.txtについて独立した完全期待文字列とcode point数を照合する。これは新9性能入力を増減しない。
- 新9と既存frame4probeはbefore Releaseとafter Debug/Release各1iterationでoracle・metadata・markを確認する。新入力/不正名/旧登録のPython対象、新enum漏れのcompiler拒否、旧89probe/旧47selector/defaultの不変性を確認する。
- 正規toolchain/flags/tidyのRelease前後とDebug ASan/UBSan/no-recover、変更source整形/規約、実CMake依存、core/application symbols、protected-diff。今回変更しない製品の全件回帰・coverage・Vim oracle再生成は行わない。
- 成功した検証の関連入力/依存/必要環境が不変なら、工程・担当・文書・commit SHAの変化だけで再実行しない。

## GUIと正式な速さ

通常Releaseはeng/build-release.ps1の明示Refで作る。beforeは製品sourceが基準mainと同じ#386の通常Releaseを再利用し、source treeとexe SHAを照合する。afterは本件製品commitから作る。

3つの専用入力をD:/NeNeNib/evidence/t388へ新規作成する。short.txt、`日`×84+`.txt`（名256UTF-8bytes）、`資料`×40の親と`日本語`×40+`.txt`（名364UTF-8bytes）。全本文は`a日<TAB>end<CR><LF>last`。長い親は絶対経路で256UTF-8bytesを超え、各componentは255UTF-16units以下、全pathは240文字未満を取得前に検査する。

固定9場面はshort saved、exact saved/modified/undo、long saved/modified/undo、:tabs一覧、Up2/Enterでshort選択。通常/Vimの切替も実mode RGBで確認する。expected titleは固定入力からの独立期待値で、製品helperを呼んで生成しない。<Up>はSDKのVK_UP=0x26を取得前からscript内に固定し、既存driver経路で送る。全鍵列のnotationを窓を作る前に検証する。

私有profile/固定settings/全client1280×800の無mask前後比較/各場面の全本文copy照合/全PNG親目視/正常終了を要求する。LastInput・foreground・title・client・closedIME・実mode・覆い/gridを記録し、外乱や失敗は元record/PNGを保持して止める。詳細scriptとplanのSHAを実操作前に保存する。試用や画面比較を未実施のまま成功と扱わない。

正式指標は名前のある文書を通るkey-to-frame-single-long-lineを初回5試料だけ取得する。他7指標は未測。既存基準値/許容/計測器を変更しない。性能取得中は他席のbuild/test/GUI/hash/copyを止め、負荷を事前観測する。hideの今回の不在・自由な試験許可を実機操作の承認として適用する。

## 分担・記録・整理

今回のhide明示指示に従い設計サナが設計と採否、SOL実装サナがharness/製品/対象検証、別SOLがGUI取得準備、LUNAが読取調査を担当する。性能・GUIは親の排他指示後に一席だけで実行する。必要な独立レビューを採否に用いる。過去の停止記載は今回の再開指示で解除された履歴として保持する。

D:/NeNeNib/worktrees/388-tab-display、D:/NeNeNib/outputs/20261011-tab-displayを用い、本体mainへ直接書かない。全試行・失敗・判断・コマンド・結果を記録し、日報/引き継ぎ/qualityへ収載する。技術受理後は通常draft PR/Ready/必須CI、eng/merge-pr.pyのplan→同expected-headでexecute、main同期を行う。恒久収載後に絶対D path/取込/未保存/未追跡/ignored/唯一成果/稼働/linkを監査して追加物を整理する。branch/commitは保持する。未完了物は理由とpathを引き継ぐ。

旧#365/#373の未解決、C8入力中parse保持、R8、IO6〜IO8、splitの未採用状態は本件で変更しない。

## 実験結果と採用（2026-10-11）

同一harness3d599b7→製品346796aの一度の固定比較で3利益/10費用が成立し技術受理。1024表示生成・破棄の通常/深いASCII/日本語対応比.724293/.957055/.979378、短縮116/111/107組。failed frameは横ばい。812checks/39oracle/strict build/tidy/sanitizer/既存保護/独立レビューが成立。

GUI原9対は0pxだが長題名がfolder欄を隠すため、同親f.txtの別1場面を先に固定して補足し0px。全26PNG目視/20本文copy/4正常close。folder全文の画面表示は主張せず独立core oracleで照合する。正式long-line5試料中央値5.939ms、基準5.431/上限7.431内。UI速度向上の証拠とはしない。他7指標未測。

全build native stdoutの保存不足、証明scriptの初回誤り、補足監査の計数訂正を残す。成功済みbuild/GUI/性能は再取得しない。コマンド・数値・raw所在・制限・再利用は[gate-proofs 5-du](../quality/history/5-du-tab-display-truncated-range.md)。
