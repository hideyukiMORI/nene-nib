# 宣言サイズまで読み込む契約 — #463 / ADR0124

## 変更と対象

`FilePort::read` → `Win32FileAdapter::read_all` → adapter内の `complete_file_read` を唯一の完了判定にする。最初に受け入れた宣言サイズを全量取得するまで所有stringを返さない。短い成功は続行し、0進捗・OS失敗・要求を超える返却長は `unreadable`。空、上限、NUL、文字コード判定、保存形式、通常の完全readのOS要求回数を維持する。

影響: ARC-001/003/007/008/010/012、CPP-003/005/007/008/012/016、QLT-001/002/004/008/012/013。Waivers: none。

## 検証の選択

| 確認 | この差分で起こり得る退行 |
| --- | --- |
| 固定toolchainで `nib_adapter_tests` のDebug build | 新しい同期読込操作と `resize_and_overwrite` の型・noexcept・span範囲が誤る。通常のclang-tidy・ASan/UBSanを適用する |
| 同targetの `--files` | 完全/短い/空のreadで要求回数・取得位置・NUL・成功長が誤る。0進捗や部分取得後の失敗を成功にする。DWORD境界で切り詰める。既存実ファイルの上限と往復を壊す |
| 変更C++のclang-format、conformance | adapter内の低層操作の配置と公開型・依存・禁止記法が正典から外れる |
| 差分と直接callerの確認 | 既存7callerの型付き失敗経路を迂回し、未完bytesを採用する別口を作る |

低層の完了判定を修正し、controller・codec・TextBuffer・保存のアルゴリズムは変更しない。通常の完全readは一回、空は無呼出しという操作列を小入力で確認する。この単位で速度改善や2GiB成立を主張せず、GUI・性能・全件アプリ試験を追加しない。#452のUTF-8性能は別の未受理事項である。

## 実行記録

起点はmain `7794d6cc89599aaf69e003a36aba94ccae77da89`。製品変更は `CompleteFileRead.hpp` と `Win32FileAdapter.cpp`、FilePortは契約コメントのみ。追加試験は既存 `NibAdapterTests.cpp` に接続し、CMake/flags/ゲート設定を変更していない。

作業木 `D:/NeNeNib/worktrees/463-complete-file-read`、実行原本 `D:/NeNeNib/outputs/complete-file-read-20261013`。恒久収載先は `D:/NeNeNib/evidence/complete-file-read-463`。原コマンドと初回の失敗も収載し、統合・収載SHA照合・整理の実結果は `closeout.json` を参照する。

| コマンド・条件 | 結果・証拠 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | 固定clang-cl 19.1.5で成功。`configure.log`。FileAPI queryを作成した通常再configureも成功、`configure-file-api.log` |
| `cmake --build build --target nib_adapter_tests --parallel 2` | 初回はCPP-012の入れ子4 > 3で失敗。反復を `complete_file_read_into` へ抽出し、第二回は試験側Windows min/maxマクロ衝突で失敗。局所の括弧記法へ直し、最後の増分4 stepsが成功。原ログは `build.log`、`build-nesting-fixed.log`、`build-win32-macros-fixed.log` |
| `build/nib_adapter_tests.exe --files`（cwd=build） | **139 checks、0 failures**。追加40契約を含む。`files.log`。DebugのASan/UBSan有効、ASAN_OPTIONS/UBSAN_OPTIONSのoverrideなし。対象実行は修正後の一回だけ |
| `clang-format --dry-run --Werror` + 変更C++4ファイル | 成功。実行順と対象は `check-win32-macros-fixed.ps1`、終了値は `result.json` に記録。無出力なので架空のformatログを参照しない |
| `python eng/conformance.py --build-dir build` | **0 violations**。`conformance.log`。FileAPI返信を含む境界検査成功 |
| `git diff --check`、製品差分・直接7caller・独立レビュー | 差分空白成功。実装と抽出後の所有境界に残る指摘なし。試験の実呼出し数をガード前で数える修正を含む。`review.json` |

最終の差分/ソースSHA・exe/構成・原ログは収載manifestに対応づける。通常readの要求回数を変える設定や、lint抑制・waiver・性能閾値の変更は加えていない。

## 再利用と限界

この修正の対象検証は初回。成功後は製品・試験・関連依存・環境が同じなら、文書追記・commit・push・review・mergeのために再実行しない。未実行の確認を成功扱いにしない。

宣言長を超える追加や同サイズ変更の検知、外部ファイルシステムのsnapshot、割当失敗の回復、取消、退避資源、非同期読込、64MiB超は対象外。旧ADR0080の短いread成功は意図して変更し、途中EOFは `unreadable`。各caller既存の失敗処理を維持し、失敗時の全UI状態不変までは保証しない。
