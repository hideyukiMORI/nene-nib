# ADR 0106 — ファイル名の探索は末尾の区切りで止める

- 状態: 技術受理（2026-10-11・固定3利益/6費用、同値性、正式1指標、取得復旧の独立レビュー成立）
- 日付: 2026-10-10
- Issue: #386
- 基準: main 0c1d399e2e830ac9653e609e377bc0704fb17619

## 問題と決定案

FilePath::file_nameのprivate name_startは経路の先頭から最後まで走査する。名前が短くても親経路全体を読む。既存コメントの「後ろから前へ」は実装と食い違っている。name_startのみをtext.size()から0より大きい間、index-1のbyteを見る後方探索へ変える。最初の\\または/でindexを返し、見つからなければ0を返す。末尾区切りはsizeで空名になる。

検証済みUTF-8のASCII区切りは継続byteに現れず、返却は従来どおり最後の区切りの直後。size_tを0より大きい間だけ減らしunderflowを避ける。公開API/parse/所有/正規化/noexcept/borrowed view寿命は不変。cacheや第二解析器/STL検索を増やさない。製品はFilePath.cppのこの関数だけ。TabTitleの一時copy案は未実験で見送る。

## 規則・影響範囲

ARC-001/004/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0010/0021/0057/0082、waiver none。直接callerはTabTitleの題名/場所、EditorControllerのcopy名/フォルダ候補、EditorWindowの確認名、ThemeCodecのfile名取得。callerのformat/表示/拡張子判定/保存経路は変えない。

## 固定入力・区間・条件

新FileNameWorkloadの7閉enumを既存typed dispatchの旧82の後へ追加する。inputは実経路のUTF-8 bytes、区間前にFilePath::parseで所有する。期待名は独立定数（bare-longだけaを1024個）で後方探索をoracleに再実装しない。

| 条件 | 入力生成 | bytes | FNV1a64 | 名前offset | checksum | 分類 |
| --- | --- | ---: | --- | ---: | ---: | --- |
| file-name-windows-4096 | `C:\Users\hide\Documents\NeNeNib\projects\editor\note.txt` | 56 | 7860286326920136938 | 48 | 958464 | benefit |
| file-name-deep-ascii-4096 | `C:\ + segment\ ×128 + note.txt` | 1035 | 14257691521225177198 | 1027 | 958464 | benefit |
| file-name-mixed-utf8-4096 | `C:/ + 資料\階層/ ×64 + 日誌🖋.txt` | 913 | 3805312051846199945 | 899 | 1474560 | benefit |
| file-name-bare-short-4096 | `note.txt` | 8 | 1116535845637733659 | 0 | 958464 | cost |
| file-name-bare-long-4096 | `a×1024` | 1024 | 18395275555644700453 | 0 | 4988928 | cost |
| file-name-trailing-4096 | `C:\ + segment\ ×128` | 1027 | 2593822827728123500 | 1027 | 0 | cost |
| file-name-root-4096 | `/` | 1 | 12638123428881205758 | 1 | 0 | cost |

名前期待値は順にnote.txt / note.txt / 日誌🖋.txt / note.txt / a×1024 / 空 / 空。bytesは8/8/14/8/1024/0/0。前後に全文・名前内容・長さ・data==path.text().data()+固定offsetを照合し、区間前に取得したviewも後で再確認する。区間内はfile_name取得4096回と、名前bytes数+空でない場合の先頭/末尾unsigned byte値のchecksum。parse/準備/oracleは区間外。本文のコピーや時間読取をcoreへ追加しない。TimingPortの既存mark二つを使う。

既存frame-document-short-saved-256とlong-saved-256を直接callerの費用として含め、既存入力/期待値/回数/marksを維持する。全9条件を同一harnessのbefore/after、20iterations/ABBA3/各120対応組で一度取得する。3利益は対応比中央値<1、全3cycle比<1、短縮>=90/120。6費用は比<=1.10かつ差中央値<=50µs/各固定区間。全oracle成功が前提。未達は不採用、外乱/欠測は保留、除外/穴埋め/合格までの再試行はしない。UI全体/単発呼出しへ倍率を読み替えない。

## 対象検証・再利用

- 既存verify_file_path/verify_tab_titlesはverify_text_and_caret経由で既定実行される。assertionをそのまま呼ぶverify_file_path_scopeと--file-pathを追加する。既定呼出し/旧46selector/既存assertionsは変えず、重複試験を増やさない。新7probeが混在区切り/非ASCIIと補助平面/長い裸名/末尾/根の内容と借用位置を直接確認する。
- tabs/command-palette scopeはfile名/場所を使う候補の表示・切替の直接callerとして一度確認する。adapter/theme/UIの分岐は不変であり、新しい名前oracleとGUIにより同一の入力名を確認し、無関係なテーマ/IO全件は回さない。
- 新入力/不正名と旧status/frame登録のPython対象試験、旧82input/function/typed表不変証明、新enum欠落の実compiler拒否。新enum以外のdispatch設計は変えない。
- 正規Release before/afterとDebug/tidy/ASan/UBSan/no-recover、対象3scopeと新7/旧2callerの各1iteration。変更source整形/規約、実CMake依存/core・application symbols、protected-diff。baseline/実flags/toolsを記録する。
- 通常Releaseはeng/build-release.ps1。GUIは短いASCII/長い日本語親経路のsaved/modified/一覧/選択/undoの場面を操作前に別固定planへ確定し、私有profile/同input/前後exe hash/全client無mask/全文copy/全PNG目視/LastInput・foreground・title・mode・IME・client guards/正常終了を要求する。正式key-to-frame-single-long-lineだけ初回5試料、他7未測。名前のある文書のframeが直接callerであり、既存正式指標の該当する単発入力を使う。本文/検索の全件回帰は行わない。
- build/test/hash/copy/GUIを性能系列と並走しない。成功した実装/入力/依存/環境が不変なら工程/文書/SHA変更だけで再実行しない。

## 採否・記録・整理

単体実行、判断に不確実性がある場合だけ必要な独立読取レビュー。D:/NeNeNib/worktrees/386-file-name、D:/NeNeNib/outputs/20261010-file-name。通常PR/必須CI、eng/merge-pr.pyのplan確認→同expected-headでexecute、main/恒久収載後に絶対D path/取込/未保存/ignored/唯一成果/稼働/linkを監査整理しbranch/commitを保持する。

全実験/失敗/判断は日報/引継ぎへ残す。前件#384の実tool plan/execute成功・Issue384の正subject・CI38060262470/main0c1d399・snapshot-384/23:38:23の整理も追記する。#365/#373は未解決。schema/設定/許容/抑制/allowlist/正規化/TabTitleコピー/公開factory/全件回帰は対象外。

## 23:52〜23:54 JST・採取前の訂正

初版の正式singleはkeys_trial(document=None)が無題でname_ofはfile_nameを呼ばず、今回の直接経路を通らない。sourceを確認し、固定SHAの名前ある文書を渡す既存long-lineだけへ訂正した。性能試料はまだ0、局所9条件/閾値/GUIは不変。初版wrapper/auditはOUT/raw/formal-plan-initialへ保存。

before初回buildは新name_matchesの5引数がCPP-012（閾値4）で拒否された。入力全文の前後照合をrunnerへ移し、name_matchesは名前内容/借用位置の4引数にする。初版b8273cc/全build logs/flags/新cppを保持し、入力/区間/oracle要件は変えない。製品未変更、exeなし、性能試料0。Python対象4tests/old82証明/dispatch正例とcase欠落拒否は関連入力不変で再利用。

## 2026-10-11の結果と採用

同一harness a1630ab / 製品9388207で全9条件各120組を一度取得、3利益/6費用成立。局所4096取得の通常/深いASCII/混在UTF8は比.166667/.009488/.016147、裸長は.998854でほぼ横ばい。全108run/4320marks/1080pairsをrawから再計算一致。--file-path/tabs/command-paletteの806checksと新7+旧2probe、正規build/sanitizer/symbolsが成立。

GUI初回before5場面後に未送信<Up>notationを拒否した原記録を保持。同PID/HWND/LastInput/全RGB一致を要求し、別planで未実行最後1場面だけ再開、未着手after6は初回取得。6対0px/本文12/100guards/4mode/2正常close、親16PNG目視。取得復旧の独立読取レビューでP0/P1/P2なし、閉じたIMEの固定6場面の同値性として受理。初回一括成功とはしない。

正式long-line初回5試料中央値5.079ms、基準5.431/上限7.431、欠測/退行0。他7未測。局所率をUI全体へ換算せず、裸長の横ばいも残して採用する。詳細・全失敗・再利用はgate-proofs 5-dt。API/schema不変、waiver none。通常PR/必須CI/main/恒久収載/監査整理後もhideの停止まで改善継続。
