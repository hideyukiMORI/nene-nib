# 5-de — 第三陣3件の統合と直接実機確認（#351・進行中）

出典: docs/quality/gate-proofs.md の節 5-de（2026-10-16 に D50 で移動・本文は移動時のまま）

統合sourceは`c907c7a3bc312ef57f0c9aa81909a6b8b2ff01b7`。変更した製品は#347/#348B1/#349だけ。`351-reuse-identity.json`で各成功時の全変更source/testと共通toolchain/flags/規約/referenceの14項目が一致。保存は本文を読むだけでcollectを呼ばず、検索移動も本文を編集しないため、部品の334/235/1591checksをQLT-012により再利用する。統合した操作の結合は下記の実機で確認した。

- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/351-speed-integration`：通常Release113steps成功。`out/release/c907c7a.json`、exe1445888bytes、SHA256 `057E588D33C9F2391CD996773A74A6AF412F062A0B9A1BB02D9340FCE61E9A0B`。製品flags/依存は変更なし。
- `python eng/protected-diff.py --base d692c2e --head c907c7a`：fixture1853不変、protected filesnone、終了0。selectorはfile-saveを追加して44。exeを渡していない43既存scopeは未測であり、全件同数を主張しない。
- `verify-351-boundaries.py --label before/after --executable <固定exe>` と `--compare`：旧製品2993bb4と統合製品を同じprofile設定/120dpi/1280×800で比較。通常編集、UTF8/UTF8 BOM/CP932の:w実保存、非重複検索の次/前/折返し、a*入力中preview/取消の18場面が本文/ステータス0画素差。両版の3保存は独立expectedと全byte一致、6process正常終了、settings不変。7操作遷移に非0画素変化を確認し、届かなかった刺激を一致の根拠にしていない。画像も直接目視した。
- GUIの初回はCtrl+S後0.5秒の確認で旧版の保存が未反映、保存入口を:wへ切り替えた2回目は0.5秒の読取で共有拒否。元script/画面/文書/失敗recordを保持。v2からv3では本文/期待/製品/操作を保ち、保存完了と共有解除を最大15秒待つ同期へ直して上記成功。Ctrl+Sの未反映原因を特定したとはしない。待ち時間は性能値に転用しない。
- computer-useの@oai/skyはnative pipe不在で接続できず、再初期化後も同じだった。入力操作は行われず、既存window_driver/verify-windowの専用CLIを使用。失敗を隠してcomputer-use成功とは記さない。
- 正式QLT-014はcollectを通る空文書1打鍵/200打鍵/16MiB200打鍵/長い日本語行1打鍵の4条件を選び、`formal-351-plan.json`に測定前固定。起動/open/paletteの刺激は変更した関数を呼ばず、QLT-001により繰り返さない。基準値/25%許容/2ms床を変えず各5試行の正規`eng/measure-speed.py --check --bench`で判定する。正式4条件は各5試行で全件基準内、0退行/0計測不能。中央値は空文書1打鍵0.646ms、200打鍵2.073ms、16MiB200打鍵2.248ms、長い日本語行1打鍵5.950ms。指紋bc8a356f37c68491の既存基準で判定し、参考記録だけの成功とは区別した。CI・main統合・恒久収載は継続中。全試料はformal-351各logと4つの正規out/speed JSONへ保持。既存runnerが繰り返し上書きするmarksは各bench終了時にsnapshotを取り、全試行のmarksを保持したとは主張しない。

配置/失敗/探索誤り/実行順の詳細はjournal.md。worktree checkout完了待ち前のcherry-pick拒否も保持し、clean確認後に残りと未適用設計を取り込んだ。元の事前設計commitは実装より前で不変。採用済み・整理済みとするのはD41と収載/監査の完了後とする。

- 恒久証跡を`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-1005/`へ収載し、997file/265,043,842bytesのコピー元/先SHA256一致を確認。manifest SHA256 `a8f4bdf49326557f43dcc138ed32429e738c69f2677976c08b766a504a48296b`。`files/`以下はD:/NeNeNibからの元pathを保持し、source/には6枝のHEADを保存。348A/350は未受理実験の時点snapshotとして含む。B1の古いtest exeは上書き済みであり、現存AのexeをB1のものと表示しない。再生成可能なobject/cacheは収載対象外。PR #352のCI後にmainへ統合し、完了した347/349/351の作業木は未保存/未追跡/無視file・リンク・稼働参照と取込を監査してから整理する。枝とcommitは保持する。
