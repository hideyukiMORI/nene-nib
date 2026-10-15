# 5-eq — 私有退避の資源寿命を製品外で確認（Issue #465・ADR0125）

出典: docs/quality/gate-proofs.md の節 5-eq（2026-10-16 に D50 で移動・本文は移動時のまま）

試作commit `73b0b74f6069c247b61df52f886a66078a09d48a`で、元ファイルを有界の同期stepで私有退避へコピーし、全量成功後だけ封印する。範囲要求の受付時に資源を保持し、owner release後も既存要求を実行できる。完了bytes/IDは資源を保持せず、元path差し替えや退避削除後も変わらない。

固定clang-cl 19.1.5、専用targetのDebug/tidy/ASan/UBSan build、`nib_source_resource_probe.exe` 228 checks/0 failures、C++16ファイルformat、conformance 0違反。初回の試験側複雑度12/入れ子4違反を関数抽出で直し、失敗ログを保存。独立レビューで終了状態とmove代入の不足を補完し、未解決指摘なし。試験input/2spool/空folderは削除済み。[コマンド・SHA・範囲・再利用](../source-resource-2026-10-13.md)。

実験コード/CMakeは専用branchに保存し、mainへはADR・結果・進捗文書だけ。製品API/機械強制は追加しない。OS取消・100ms・2GiB・製品temp/予算・UTF-8/索引・全caller移行は未確認で、#452の性能待ちも維持。ARC/CPPはADR0125、QLT-001/012/013、保存schema変更なし、Waivers none。文書差分/目視で統合しCI不要。恒久証拠と監査整理は`D:/NeNeNib/evidence/source-resource-465/closeout.json`。
