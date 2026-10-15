# 5-ct — 描画の保持の契約を窓なしの試験で守る（Issue #300・ADR 0077）

出典: docs/quality/gate-proofs.md の節 5-ct（2026-10-16 に D50 で移動・本文は移動時のまま）

#291 の独立レビューの L2・L4・O2 を受けた。実装は実装席（Opus）、受理と実機の確認は設計席。製品のコードを触ったので、ADR 0075 の形で確かめた。

**見つかった契約の破れ。** `tests/ui` の試験を書いたところ、ADR 0073 の「未対応の装飾が来たら収集の全体を棄却する」が実際の DirectWrite では成り立っていなかった。`IDWriteTextLayout::Draw` は callback の HRESULT を無視して S_OK を返すので、下線・取り消し線・inline object・effect を `E_NOTIMPL` で断っても字形の保持は「使える」のまま残り、装飾だけが黙って落ちる。本文の layout は装飾も effect も使っていない（grep 0 件）ので表示は壊れていなかった。`BodyGlyphCollector` に棄却の印を持たせ、字形を渡す口 `take()` の 1 か所が決める形に直した。

- 確認する退行: 字形の保持の決め方と字体選択の取り付けを変えたので、画面に出るもの・起動と描画の速さ・保持の契約（一致条件・上限・棄却）。
- `ctest --test-dir build -R nib_window` → 1/1 成功・**1521 checks**・測れなかった検査 0（実装席・Debug・clang-tidy 込み）。環境依存として分けたのは画素の一致 180 組（3 family × 3 大きさ × 4 幅 × 5 行。保持した字形 == `DrawTextLayout`）と画面外の判定の境界で、この機械では全部測れた。
- 反例 2 つ（実装席）: `FontFallbackKey.cpp` の locale の比較を外すと 2 件、`take()` で印を見る 1 行を外すと 8 件落ちる。どちらも戻して成功を確かめ、`git diff` が空。
- `python eng/conformance.py`・`--build-dir build`・`python eng/symbols.py --build-dir build --require core application` → 違反 0。NOLINT は `tests/ui/ScriptedFontFallback.cpp` の CPP-019 の印つき 1 行だけ（CNF-012 が通る）。
- Release: `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `d59f0b2`・SHA-256 `15C59E33BAA030015A6314476DEA07F03DA5C9FE6DE5CE563A01BD24A5B2685C`・1399296 bytes（`out/release/d59f0b2.json`）。
- 表示の前後比較（hide の了承の後）: `python -B D:/NeNeNib/scripts/300-rina-visual-20261007.py`（#291 の 22 場面の道具の出力先だけを変えた写し）を、統合済みの `f42a95d` と `d59f0b2` で 1 回ずつ。編集・クリック・選択・検索・scroll・幅・書式とテーマの変更・多言語 / 結合 / Tab / 双方向の **22 場面すべて本文とステータス 0 画素差**。刺激で画が変わることも道具が確かめる。設計席が `15-search.png` を見た。出力は `D:/NeNeNib/outputs/300-visual-20261007/`。
- 速さ（QLT-014・8 本）は 2 回ある。**1 回目は無効な測定として残す。**
  - 1 回目（`out/speed/2026-10-06T15-21-22Z.json`）: 起動の 3 本が上限を越えて終了 1（起動 240.750 ms / 上限 239.360・窓 46.123 / 43.666・16 MiB を開く 322.197 / 312.229）。打鍵の 5 本は基準内。測定の途中に同じ機械で別の席のビルドが動いていた（設計席は開始の前の 6 秒しか確かめていなかった。直後にビルドのプロセス 16・CPU 負荷 58%）。hide も「Claude と Codex をたくさん立ち上げている」と答えた。起動の全区間が一様に約 25% 遅く、この変更が触れていない区間（プロセスの立ち上げ・device の生成）まで遅い。測定の前提（測定中に重い処理を走らせない）を満たしていない。
  - 2 回目（`out/speed/2026-10-06T16-40-36Z.json`）: hide の「測っていいよ」の後、負荷 4〜11%・ビルドのプロセス 0 を確かめてから 1 回。**8 benches checked・0 regression・0 unmeasurable**。起動 199.818 ms・窓 32.740・1 打鍵 0.642・200 打鍵 3.044・16 MiB を開く 245.312・16 MiB の 200 打鍵 5.226・Ctrl+P の 5000 件 2.321・長い行の 1 打鍵 6.339。基準値と許容は変えていない。
  - 合格するまで測り直したのではない。1 回目は原因（測定中の負荷）を特定し、前提を満たした状態で 1 回だけ測った。
- 残り: 取り付けが失敗する経路の試験は無い（`IDWriteFactory2` の替え玉を作らない設計・ADR 0077）。`WM_FONTCHANGE` は扱わない（ADR 0071 の限界）。

適用: #300 / ADR 0071 / 0073 / 0075 / 0076 / 0077 / CPP-019 / CNF-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014。Waivers: none。
