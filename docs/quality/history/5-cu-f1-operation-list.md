# 5-cu — F1 の操作の一覧と、操作の 1 つの表（Issue #303・ADR 0078・施主決定 D43）

出典: docs/quality/gate-proofs.md の節 5-cu（2026-10-16 に D50 で移動・本文は移動時のまま）

操作の名前・鍵・実行の中身を core の 1 つの表に寄せ、鍵で押しても一覧（F1・Ctrl+P の面の `?`）から選んでも ui の `run_operation` の 1 本で実行する。設計と受理は設計席、実装は工程ごとに別の実装席（Opus）。

- 確認する退行: 鍵の道を表に移したことで鍵の振る舞いが変わること（Ctrl+O・S・P・Z・Y・文字の大きさ）、タブとブックマークの鍵の意味（表を読む口に変えた）、面の既存の出どころ（`#` `*` `@` `/` `:`）の候補と見た目、面と打鍵の速さ。
- 工程 1（core）: `nib_tests --operations` 344 → 工程 2 の後 **516 checks**。`--tabs` 291・`--bookmarks` 40 は試験を 1 文字も変えずに成功（`tab_command_for` と `toggles_bookmark` が表を読む口になっても意味が同じ）。表 18 行と今の ui の振る舞いを 1 行ずつ照合し、合わない行は無かった。
- 工程 2（面と頼まれた操作）: `--command-palette` 371・`--background-work` 89・`--history` 38（`verify_quiet_paths` を含む）・`--user-theme-selection` 88。直した既存の試験は、設計で期待が変わった 3 か所だけ（表に無い記号の例を `?a` から `!a` へ・案内の文字列に「? 操作」・Ctrl+Shift+O は `open_file`）。
- 工程 3（ui）: `--bookmarks` 41（`?` の面での Ctrl+D は何もしない、を足した）。Debug のビルドは clang-tidy 込みで警告 0。
- #300 の上に載せ直した後、設計席が Debug で全 target をビルドし、上の 6 scope と CTest `nib_window` を回して成功。`python eng/conformance.py --build-dir build` と `python eng/symbols.py --build-dir build --require core application` は違反 0。許可シンボル・NOLINT・waiver は足していない。
- Release: `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `4d84334`・SHA-256 `6E488BC4C2FF64B924E26EACEF187894F7CE0E2F1D3128636147D594641869BB`・1415168 bytes。統合する先頭と `git diff --exit-code 4d84334 HEAD -- src tests eng CMakeLists.txt` が終了 0 なので、以下の実機の結果をそのまま使う（QLT-012）。
- 速さ（QLT-014・8 本・hide の「測っていいよ」の後・負荷 6%・ビルドのプロセス 0）: **8 benches checked・0 regression・0 unmeasurable**。起動 193.579 ms・窓 31.171・1 打鍵 0.683・200 打鍵 3.184・16 MiB を開く 242.312・16 MiB の 200 打鍵 5.696・Ctrl+P の 5000 件 2.444・長い行の 1 打鍵 6.171（worktree の `out/speed/2026-10-06T16-42-43Z.json`）。Ctrl+P の画 `palette-2026-10-06T16-41-50Z-5752.png` を設計席が見て、件数 1 / 5000 と、既存の面の行の見た目が変わっていないことを確かめた。
- 実機の操作（hide の了承の後・1 回）: `python -B D:/NeNeNib/scripts/303-rina-verify-20261007.py --executable <Release 4d84334> --name first` → 31 場面・終了 0。文字と F1 は投函、Ctrl の鍵だけ本物のキー入力。出力は `D:/NeNeNib/outputs/303-verify-20261007/`。
  - 設計席が見た画: F1 の一覧（行の右端の鍵の枠・16 件）・「ほぞん」で 2 件・Ctrl+P の面の案内（「? 操作」が欄に収まる）・`#` の面の補足が今のまま・Vim の「ctrl+f4」でタブを閉じるが `Ctrl+F4`・Vim の「もとにもどす」は候補なし・Ctrl+Shift+; で文字が大きくなる。
  - 画素の比較で確かめたこと: 一覧から新しいタブ／前に使ったタブ（1 歩で確定）／文字を大きく／戻す／Vim へ切り替えで画が変わる。文字の大きさを戻すと前の画に一致。打った後の画とやり直した後の画が一致（Ctrl+Z → Ctrl+Y）。Vim の Ctrl+Z の前後が一致（何も起きない）。`#` の面の上で F1 を押すと、Ctrl+P の面で `?` を打った画に一致。
  - 一覧から「ファイルを開く」で OS のダイアログが出て取消で戻る。Ctrl+S（無題）で保存のダイアログ。閉じるときの「保存しますか」。3 つとも道具が見つけて答え、終了コード 0。
- 振る舞いの変化（設計席が受理した）: Ctrl+Alt+O・S・Z・Y は効かなくなる（新しい道は Alt のとき鍵を写さない。AltGr の配列で誤って走らない向きで、文字の大きさ・タブ・ブックマークの鍵の今の守りと揃う）。Shift を見ていなかった鍵（Ctrl+Shift+O など）は、表に行を明示して今のまま。
- 見つけて直していないもの: 面の「候補なし」の文字が面の左端に寄っている（この差分は触っていない・前からの見た目）。別 Issue にする。
- 実機で確かめていないもの: ライトのテーマでの鍵の枠・一覧のクリックでの実行・Ctrl+Tab を鍵で押して離したときの確定（経路は変えていない）・Ctrl+ホイール。

適用: #303 / ADR 0078 / D43 / FR-018 / ARC-001 / ARC-012 / CPP-002 / CPP-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014。保存 schema・基準値・許容の変更なし。Waivers: none。
