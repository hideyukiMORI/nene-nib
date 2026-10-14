# キャレット追従の遅延解決 — Issue #474（ADR 0128）

## 変更と確認する退行

`follow_caret` / `follow_position` は追従したい位置を `pending_follow_` に置くだけにし、解決は controller がスクロールを読む直前の `settled_scroll()`（`settle_follow()`）と `frame()` の先頭で 1 回だけ行う。`state_.scroll()` の直読み 15 か所（controller 12・EditorView 3）を置き換え、残るのは `settle_follow` と `settled_scroll` の中だけ。`frame()` `visual_view()` `visible_lines()` `line_view()` `measured_vim_step()` は非 const。タブを離れる `leave_document()`・無題に開く道・`EndSession` の前に保留を解決して捨てない。`EditorState`・core・ui・adapters・保存 schema は不変。

確認する退行: 追従の結果が即時の実装と変わること（折り返しの画面行・incsearch の origin・VisibleLines の前後・タブ切替・Vim の画面移動）、const の `frame()` を呼ぶ試験と probe のコンパイル、連打以外の速さの悪化。

## 根拠の実測（main 89c582d・変更前）

正典 Release（SHA-256 `584911AD…C505`）の 8 本は連打の 2 本だけ基準超過（空 11.266 ms・16 MiB 14.661 ms）。実験ブランチ `experiment/460-burst-breakdown`（cb18638）に節目を足して 16 MiB の連打を測ると、1 打鍵の `apply` は中央値 65 µs、うち `BodyTextLayouts::make_layout` が毎打鍵 1 回・50 µs（200 打鍵で 10.0 ms）、窓側 5 µs。空の文書も同じ形（apply 56 µs・layout 38 µs）。窓なしの `controller-insert-200` は 0.38 ms なので、本文の編集ではなく追従の layout が原因。報告は `D:/NeNeNib/worktrees/460-burst-breakdown/out/reports/probe-460.md`。

## 対象検証（worktree `D:/NeNeNib/worktrees/474-settled-follow`・commit 2179dc3）

| 対象 | 退行 | コマンド・結果 |
|---|---|---|
| application / ui の直接利用 target の Debug | 型・所有・複雑度の違反、範囲外、未定義動作 | `cmake --build build --target nib_tests NeNeNib nib_perf_probes --parallel 8`（clang-cl 19.1.5・tidy・ASan/UBSan）成功 |
| 追従とスクロール | 200 打鍵後の 1 枚の frame が 1 打鍵ごとの frame と同じ所を見せる・打鍵だけでは layout を要求しない・incsearch の origin が解決済み | `build/nib_tests.exe --application`（Vim fixture 全件の再生を含む・16,029 checks）成功。契約 2 本を追加 |
| 折り返し・検索・画面移動・タブ・一覧 | 画面行の位置解決、incsearch、`H M L` と Ctrl-d/u、タブ切替の保留、面の行 | `--wrapped-view --vim-wrapped --vim-search --vim-search-incremental --vim-line-jumps --vim-open-lines --tabs --session --background-work --command-palette --frame-selection` すべて exit 0 |
| 境界・規約・整形 | 外部依存、規約逸脱、空白 | `python eng/symbols.py --build-dir build --require core application` 2 libraries / 0 違反、`python eng/conformance.py --build-dir build` 0 違反、変更 C++ の `clang-format --dry-run --Werror` 0、`git diff --check` 0 |

ログは worktree の `out/474-*.log`、席の報告は `out/reports/done-474-implement.md`。

## 正式な速さのゲート（QLT-014・変更後 9bff5a3）

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` で Release（`build/release-9bff5a3/NeNeNib.exe`）を作り、負荷 3〜6% を確かめてから `python eng/measure-speed.py --check --executable build/release-9bff5a3/NeNeNib.exe --repetitions 5` を 1 回。機械 bc8a356f37c68491・120 dpi・各 5 試行・欠測 0・退行 0。

| bench | 変更前 89c582d | 変更後 9bff5a3 | 基準（上限） |
|---|---:|---:|---:|
| startup-first-frame | 192.252 | 189.735 | 191.488（239.4） |
| startup-window-shown | 31.394 | 30.252 | 34.933（43.7） |
| key-to-frame-single | 0.644 | 0.637 | 0.906（2.906） |
| key-to-frame-burst-200 | **11.266** | **2.090** | 2.695（3.369） |
| open-large-file-16mib | 201.587 | 202.457 | 249.783（312.2） |
| key-to-frame-burst-200-16mib | **14.661** | **2.227** | 7.411（9.264） |
| key-to-frame-palette-5000 | 1.726 | 1.956 | 2.630（4.630） |
| key-to-frame-single-long-line | 6.250 | 6.183 | 5.431（7.431） |

変更後の全値: burst-200 [2.138, 2.107, 2.090, 2.078, 2.071]、burst-200-16mib [2.246, 1.753, 2.227, 2.180, 2.228]。変更前の全値と JSON は `D:/NeNeNib/outputs/perf-day-20261015/baseline-main-89c582d-speed.json`、変更後は同じ場所の `gate-474-9bff5a3-speed.json`。基準値・許容は変更せず `--adopt` はしない。

## 再利用と限界

Vim の鍵の連打は engine が viewport を毎回受け取るので速くならない（ADR 0128 決定 5・後続 Issue）。長い行の単打は 6 KB の行の layout 作成（約 4.6 ms）が床で、本件の対象外。解決は解決する時点の設定・モードで行う（ADR 0128 の限界）。成功後は実装・テスト・関連依存が同一なら再実行しない。Waivers: none。
