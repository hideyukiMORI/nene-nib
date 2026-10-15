# 5-cp — 改善一式の手動試用を開始（Issue #291）

出典: docs/quality/gate-proofs.md の節 5-cp（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-06 00:15:30 JST、hideの明示依頼で改善一式の通常Releaseを起動した。生成元`f42a95d`、exeは`D:/NeNeNib/worktrees/291-display-width-capsule-20261005/build/release-f42a95d/NeNeNib.exe`。起動記録は[manual-launch-2026-10-06.json](../manual-launch-2026-10-06.json)。main統合・性能受理の保留は継続し、実機試用の結果は未受領。

- 対象と理由: 試用するexeの版の取り違えと起動失敗を確認する。`git diff --exit-code f42a95d HEAD -- src tests eng CMakeLists.txt`成功。`Get-FileHash -LiteralPath <exe> -Algorithm SHA256`がRelease記録の`44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900`と一致した。
- `Start-Process -FilePath <exe> -WorkingDirectory C:/Users/info/WORKS/NeNeNib -WindowStyle Normal -PassThru`で起動。`WaitForInputIdle(10000)`がtrue、プロセス継続とウィンドウhandle `462050`を確認。起動時PID `568`。通常環境を継承し、profile上書き・測定引数なし。
- 再利用: 現在の製品入力と同一の既存Releaseを使用。追加変更は日報・引き継ぎ・状態要約・起動記録のみで、アプリの自動テスト・再ビルド・性能再測定は選ばない。文書は差分空白・リンク・JSONと原記録の一致を確認する。
- 規則: QLT-001 / QLT-012 / QLT-013 / GIT-003 / GIT-004。製品コード・保存schema・基準値・waiverは変更なし。既存WVR-0001/0002はSDK署名の局所抑制のまま。

起動後はアプリを開いたままhideへ渡した。2026-10-06 00:21:14 JSTに`@(Get-Process NeNeNib -ErrorAction SilentlyContinue).Count`で0件を確認した。こちらでは終了・再起動を行っていない。終了理由と利用者の試用結果は未受領。worktreeは未統合の候補として保持する。起動確認は利用者の受理や性能合格を意味しない。
