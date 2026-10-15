# 5-dc — 行取得・候補照合・文字コード変換の統合受理（Issue #341・PR #342・D41）

出典: docs/quality/gate-proofs.md の節 5-dc（2026-10-16 に D50 で移動・本文は移動時のまま）

製品source `2993bb482e7cf117aefe269226eac606d0e72b1c` は5-dbの4部品の和。独立レビュー `D:/NeNeNib/outputs/341-review/review-341.md` は各作業木673fileを照合し、製品と機能testの計14fileの変更が各成功時と一致すること、本文byte位置と候補indexを混ぜないこと、CP932→UTF8→TextBufferの防御境界が残ることを確認した。最終ログ追補を含めP0/P1/P2なし。部品scopeをもう一度走らせる根拠となる新しい結合差分はなく、`out/341-component-reuse-proof.json` と意味境界レビューを根拠に5-dbの成功結果を再利用した。

**正規Releaseと静的境界。** `CMAKE_BUILD_PARALLEL_LEVEL=2; pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` は固定clang-cl19.1.5/Releaseで成功。`build/release-2993bb4/NeNeNib.exe` は1447424bytes、SHA256 `CDF2433522FA5F672EAC2DB7B3747ED56F7337EDC0EB71160352DCBB20C30F8B`。`out/release/2993bb4.json` のconfigure2.959/build322.701/計325.66秒。最初の呼出しは出力先outが無くshellのredirectだけが失敗し、build自体は開始していない。out作成後の一回の実buildが上記であり、初回失敗を成功へ読み替えない。

最終CMake構成と規約境界を確かめるため、正規File API queryを置いた同じRelease構成に対して `python -B eng/conformance.py --build-dir build/release-2993bb4` は0、`python -B eng/symbols.py --build-dir build/release-2993bb4 --require core application` は2libraries/0。`python -B eng/protected-diff.py --base eef5aad --head 2993bb4` はfixture1853→1853、metadata0/deleted0/changed0/added0、保護files changed none。scopes43のうち新規buffer-rangeと旧42のruntime件数比較は未測で、同数成功とは記載しない。各logは `out/341-{release-file-api-configure,conformance,symbols,protected}.log`、機械記録は `out/protected/2993bb4.json`。

**設計席による実機の対象確認。** TextBufferと入力文字変換の接点、一覧の部分絞込と失効が直接影響するため、`python -B -X utf8 D:/NeNeNib/scripts/341-boundary-visual-20261009.py --executable <前後Release> --name <before|after>` と同runnerの `--compare` を選択。前は第一陣a64a5d4、後は2993bb4、共通の固定文書/設定v2/120DPI。同じCRLF/LF本文の断片化編集・行端・スクロール、一覧到着・名前/場所・末尾追加/途中編集/削除・caret・scope・操作一覧の25場面が本文/ステータス0画素差、刺激到達・設定v2・正常終了0も確認。`D:/NeNeNib/outputs/341-boundary-visual-20261009/comparison.json` と全PNGを保存し、代表画面を目視した。既存35場面とWIC2280はrenderer/UI不変なので再実行していない。

実FilePortから変換→本文→保存までを覆うため、`python -B -X utf8 D:/NeNeNib/scripts/341-read-save-20261009.py --executable <2993bb4 Release> --output D:/NeNeNib/outputs/341-read-save-20261010` を実行。UTF8、UTF8 BOM/CRLF、CP932/LFの3通りで編集後の全byte一致、未保存印解消、正常終了0。完了状態を最大20秒観測し、1.2秒固定待ちへ戻していない。結果は同outputのresults.jsonと全撮影/設定。今回のbeforeは同じ期待byteと前版で成功した5-daを再利用。

**正式速度の選択理由と結果。** core::to_utf8の確保変更はLocalSettingsPath/AbsolutePathの起動、EditorWindowの通常入力、FolderListingの候補、CP932に直接波及する。TextBufferは空/大文書/長行のframeと入力に、一覧は5000件絞込に直接使われるので、この異なる直接callerを覆う正式8本を一回選んだ。全件契約/coverage/oracleは実行しない。taskのbuild/static/GUI/copy停止とprocess inventory空を `out/341-formal-environment-before.json` に記録し、00:08:26 JSTに `python -B -X utf8 eng/measure-speed.py --check --executable D:/NeNeNib/worktrees/341-speed-integration/build/release-2993bb4/NeNeNib.exe` を開始。終了0、**8 checked / 0 regression / 0 unmeasurable**。

`out/speed/2026-10-09T15-10-44Z.json`、指紋bc8a356f37c68491（i9-10850K/RTX3090/120DPI）、各5sample。palette撮影に1/5000の候補表示を目視確認した。正式道具のsamplesと最終marksを保存するが、全試行のrawを保持する局所harnessとは区別する。

| 正式bench | 中央値ms | min〜max ms |
| --- | --- | --- |
| startup-first-frame | 221.150 | 211.118〜239.068 |
| startup-window-shown | 40.537 | 36.082〜45.818 |
| key-to-frame-single | 0.589 | 0.479〜0.672 |
| key-to-frame-burst-200 | 1.742 | 1.593〜2.115 |
| open-large-file-16mib | 249.044 | 237.587〜311.457 |
| key-to-frame-burst-200-16mib | 1.817 | 1.742〜2.163 |
| key-to-frame-palette-5000 | 2.470 | 2.040〜2.755 |
| key-to-frame-single-long-line | 4.631 | 4.548〜4.810 |

正式8本は既存基準に対する退行判定で、前の正式一回との差を因果的な改善量としない。外部案件の機械負荷は管理していない。旧版での#331は別記録として保持し、許容や基準を動かしていない。

全候補の未実験、caretの分解能未満、B ASCII/A移管の利益未確認、一覧入口copyの区間外、B/Cの要求容量増を5-dbと採否台帳から継承する。peak memory、物理DPI遷移、device-loss/COM失敗注入は未測。独立レビュー・実機対象確認・正式8本を満たし、必須CIを確認してsquash mergeする。以後文書だけなら関連source/test/toolの一致を確認して成功証拠を再利用する。規則ARC-001/003/004/005/007/012、CPP-003/005/007/008/012/016、QLT-001/012/013/014、GIT-003/004、D41。waiver none。

恒久証拠は `D:/NeNeNib/evidence/speed-optimizations-20261009/append-20261010-wave2/`。元path/commit/全file SHAのmanifestと最終main/PR対応、削除前監査を保存する。main統合・収載・必要反映の完了後、追加作業木と一時出力を整理しbranch/commitを保持する。

**統合・収載の実結果。** [PR #342](https://github.com/hideyukiMORI/nene-nib/pull/342)のhead310163cで必須checkが[run37950695621](https://github.com/hideyukiMORI/nene-nib/actions/runs/37950695621)に成功し、00:18:50 JSTにsquash mergeした。mainは `b79c9e6aa9b6444295725441db5e850db2c5bc85`、本体ff-only同期後clean。`git diff --quiet HEAD 2993bb4 -- src tests eng CMakeLists.txt` は0で、実測済み製品入力の一致を確認。文書だけの310163cと統合SHAを理由に製品検証を再実行していない。#337〜#341はclosed。

第二陣archiveの本manifest5637file/96,820,501bytesは全元/先SHA一致、manifest SHA256 `ea3032adebf8994fa375be6cbcfee63850181b3d1f17a6d2d47405f6e5a0bbc4`。最終reviewの1fileは `supplement-341-review/`、確定docs8file/報告/PR本文とmain対応は `supplement-341-final/` に独立追補する。元のmain manifestを上書きして原結果を消さない。cleanupの詳細は同恒久先の監査へ残し、branch/commitは保持する。

00:23:38 JSTまでに#337〜#340のD作業木4件と、完全収載済みのreview4件/batch4原出力を削除。`D:/NeNeNib/outputs/cleanup-speed-wave2-20261010/cleanup-result.json` は全remove exit0、4枝/commit保持、本体clean、live/links/未保存/唯一未収載なしを記録する。最終#341作業木と今回の他の重複出力は文書PR #343の統合後までに整理し、終了時の監査とmain SHAは恒久archiveのcloseoutへ追記する。
